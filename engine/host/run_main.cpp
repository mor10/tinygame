// Plays a steps script (docs/formats/steps.md) on a packed game, as fast as
// possible, and reports. Used by `tg run`; prints tab-separated lines:
//   syntax <line> <message>              the script has a mistake (exit 2)
//   fail <line> <expected> <found>       an expect did not hold (run goes on)
//   result <ticks> <score> <hurts> <game error or empty>
//   probe-begin / probe lines / probe-end   with --probe
// Exit: 0 all expects held, 1 an expect failed or the game stopped, 2 bad
// arguments, bundle or script.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "../core/button_edges.h"
#include "../engine.h"

namespace {

enum class Op { Eq, Ne, Lt, Le, Gt, Ge };
enum class Cmd { Tilt, Turn, Press, Release, Wait, Expect };

struct Step {
    Cmd cmd;
    int line = 0;
    float a = 0, b = 0;      // tilt x y; turn: a = signed transitions
    long wait = 0;
    std::string name;        // expect: the thing measured
    Op op = Op::Eq;
    double value = 0;
};

constexpr long MAX_WAIT = 600000;  // ten minutes of game time

std::vector<std::string> words(const std::string& text) {
    std::vector<std::string> out;
    std::istringstream in(text.substr(0, text.find('#')));
    for (std::string w; in >> w;) out.push_back(w);
    return out;
}

bool number(const std::string& text, double& out) {
    char* end = nullptr;
    out = std::strtod(text.c_str(), &end);
    return !text.empty() && *end == '\0' && std::isfinite(out);
}

bool wholeNumber(const std::string& text, long& out) {
    double v;
    if (!number(text, v) || v != std::floor(v) || v < 0 || v > MAX_WAIT) return false;
    out = static_cast<long>(v);
    return true;
}

bool knownName(const std::string& n) {
    static const char* const names[] = {"score", "hurts", "tick", "actors", "player.x", "player.y",
                                        "player.vx", "player.vy", "player.ground"};
    for (const char* k : names)
        if (n == k) return true;
    if (n.rfind("count.", 0) == 0 && n.size() > 6) {
        for (std::size_t i = 6; i < n.size(); ++i) {
            const char c = n[i];
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
        }
        return true;
    }
    return false;
}

bool parseOp(const std::string& t, Op& op) {
    if (t == "==") op = Op::Eq;
    else if (t == "!=") op = Op::Ne;
    else if (t == "<") op = Op::Lt;
    else if (t == "<=") op = Op::Le;
    else if (t == ">") op = Op::Gt;
    else if (t == ">=") op = Op::Ge;
    else return false;
    return true;
}

// Returns the number of mistakes; each is printed as a "syntax" line.
int parseSteps(const std::string& text, std::vector<Step>& steps) {
    int mistakes = 0;
    std::istringstream lines(text);
    std::string raw;
    int line = 0;
    auto bad = [&](const std::string& message) {
        std::printf("syntax\t%d\t%s\n", line, message.c_str());
        ++mistakes;
    };
    while (std::getline(lines, raw)) {
        ++line;
        const std::vector<std::string> w = words(raw);
        if (w.empty()) continue;
        Step s;
        s.line = line;
        double x, y;
        if (w[0] == "tilt") {
            if (w.size() != 3 || !number(w[1], x) || !number(w[2], y)) {
                bad("tilt needs two numbers, like: tilt 0.5 0");
                continue;
            }
            s.cmd = Cmd::Tilt;
            s.a = static_cast<float>(x);
            s.b = static_cast<float>(y);
        } else if (w[0] == "turn") {
            long n;
            if (w.size() != 3 || (w[1] != "left" && w[1] != "right") || !wholeNumber(w[2], n)) {
                bad("turn needs left or right and a whole number, like: turn right 20");
                continue;
            }
            s.cmd = Cmd::Turn;
            s.a = static_cast<float>(w[1] == "left" ? -n : n);
        } else if (w[0] == "press" || w[0] == "release") {
            if (w.size() != 1) {
                bad(w[0] + " takes nothing after it");
                continue;
            }
            s.cmd = w[0] == "press" ? Cmd::Press : Cmd::Release;
        } else if (w[0] == "wait") {
            if (w.size() != 2 || !wholeNumber(w[1], s.wait)) {
                bad("wait needs a whole number of ticks (60 ticks are one second), like: wait 30");
                continue;
            }
            s.cmd = Cmd::Wait;
        } else if (w[0] == "expect") {
            if (w.size() != 4 || !knownName(w[1]) || !parseOp(w[2], s.op) || !number(w[3], s.value)) {
                bad("expect needs a name, a comparison (== != < <= > >=) and a number, like: expect score >= 10. "
                    "Names: score hurts tick actors player.x player.y player.vx player.vy player.ground count.<kind>");
                continue;
            }
            s.cmd = Cmd::Expect;
            s.name = w[1];
        } else {
            bad("I do not know '" + w[0] + "'. The words are: tilt turn press release wait expect");
            continue;
        }
        steps.push_back(s);
    }
    return mistakes;
}

bool measure(const tg::Engine& engine, const std::string& name, double& out) {
    if (name == "score") out = engine.score();
    else if (name == "hurts") out = engine.hurts();
    else if (name == "tick") out = engine.ticks();
    else if (name == "actors") out = engine.actorCount();
    else if (name.rfind("player.", 0) == 0) {
        tg::ActorProbe p;
        if (!engine.player(p)) return false;
        if (name == "player.x") out = p.x;
        else if (name == "player.y") out = p.y;
        else if (name == "player.vx") out = p.vx;
        else if (name == "player.vy") out = p.vy;
        else out = p.on_ground ? 1 : 0;
    } else {  // count.<kind>
        const std::string kind = name.substr(6);
        int n = 0;
        tg::ActorProbe p;
        for (int i = 0; engine.actor(i, p); ++i)
            if (kind == p.kind) ++n;
        out = n;
    }
    return true;
}

bool holds(Op op, double found, double want) {
    switch (op) {
        case Op::Eq: return found == want;
        case Op::Ne: return found != want;
        case Op::Lt: return found < want;
        case Op::Le: return found <= want;
        case Op::Gt: return found > want;
        case Op::Ge: return found >= want;
    }
    return false;
}

const char* opText(Op op) {
    static const char* const t[] = {"==", "!=", "<", "<=", ">", ">="};
    return t[static_cast<int>(op)];
}

std::string num(double v) {
    char buf[64];
    if (v == std::floor(v) && std::fabs(v) < 1e9) std::snprintf(buf, sizeof buf, "%.0f", v);
    else std::snprintf(buf, sizeof buf, "%.2f", v);
    return buf;
}

bool readFile(const char* path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    const char* bundle_path = nullptr;
    const char* steps_path = nullptr;
    const char* raw_path = nullptr;
    int width = 0, height = 0;
    bool probe = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const bool more = i + 1 < argc;
        if (a == "--bundle" && more) bundle_path = argv[++i];
        else if (a == "--steps" && more) steps_path = argv[++i];
        else if (a == "--size" && i + 2 < argc) { width = std::atoi(argv[++i]); height = std::atoi(argv[++i]); }
        else if (a == "--raw" && more) raw_path = argv[++i];
        else if (a == "--probe") probe = true;
        else { std::fprintf(stderr, "bad argument: %s\n", a.c_str()); return 2; }
    }
    if (!bundle_path || !steps_path || width <= 0 || height <= 0) {
        std::fprintf(stderr, "usage: run_host --bundle F --steps F --size W H [--raw F] [--probe]\n");
        return 2;
    }
    std::string bundle, script;
    if (!readFile(bundle_path, bundle)) { std::fprintf(stderr, "cannot read %s\n", bundle_path); return 2; }
    if (!readFile(steps_path, script)) { std::fprintf(stderr, "cannot read %s\n", steps_path); return 2; }
    std::vector<Step> steps;
    if (parseSteps(script, steps) > 0) return 2;

    tg::Engine engine;
    if (!engine.load(reinterpret_cast<const std::uint8_t*>(bundle.data()), bundle.size(), width, height)) {
        std::fprintf(stderr, "cannot load the game: %s\n", engine.error());
        return 2;
    }
    tg::ButtonEdges edges;
    float tilt_x = 0, tilt_y = 0;
    int buttons = 0;
    int pending_turn = 0;
    int failures = 0;
    for (const Step& s : steps) {
        switch (s.cmd) {
            case Cmd::Tilt: tilt_x = s.a; tilt_y = s.b; break;
            case Cmd::Turn: pending_turn += static_cast<int>(s.a); break;
            case Cmd::Press: buttons |= 2; break;
            case Cmd::Release: buttons &= ~2; break;
            case Cmd::Wait:
                for (long t = 0; t < s.wait; ++t) {
                    engine.tick(edges.step(0, pending_turn, buttons, tilt_x, tilt_y, false));
                    pending_turn = 0;
                }
                break;
            case Cmd::Expect: {
                double found = 0;
                const bool ok = measure(engine, s.name, found);
                if (!ok || !holds(s.op, found, s.value)) {
                    ++failures;
                    const std::string want = "expected " + s.name + " " + opText(s.op) + " " + num(s.value);
                    std::printf("fail\t%d\t%s\t%s\n", s.line, want.c_str(),
                                ok ? num(found).c_str() : "there is no player");
                }
                break;
            }
        }
    }
    const std::string error = engine.error();
    std::printf("result\t%lu\t%d\t%d\t%s\n", static_cast<unsigned long>(engine.ticks()), engine.score(),
                engine.hurts(), error.c_str());
    if (probe) {
        char text[4096];
        engine.probeText(text, sizeof text);
        std::printf("probe-begin\n%sprobe-end\n", text);
    }
    if (raw_path) {
        std::vector<tg::gfx::Color> pixels(static_cast<std::size_t>(width) * height);
        tg::gfx::Canvas canvas({pixels.data(), width, height, width});
        engine.draw(canvas);
        std::FILE* f = std::fopen(raw_path, "wb");
        if (!f || std::fwrite(pixels.data(), sizeof(tg::gfx::Color), pixels.size(), f) != pixels.size()) {
            std::fprintf(stderr, "cannot write %s\n", raw_path);
            return 2;
        }
        std::fclose(f);
    }
    return failures > 0 || !error.empty() ? 1 : 0;
}
