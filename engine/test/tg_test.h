#pragma once

// Minimal test harness. Unlike a bare assert, a failed CHECK is recorded and
// the run continues, so one run reports every failure.
//
//   TG_TEST(fills_inside_bounds) { CHECK_EQ(canvas.at(0, 0), red); }
//   int main() { return tg::test::run(); }

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace tg::test {

struct Case {
    const char* name;
    void (*body)();
};

inline std::vector<Case>& cases() {
    static std::vector<Case> all;
    return all;
}
inline int& failures() {
    static int count = 0;
    return count;
}
struct Register {
    Register(const char* name, void (*body)()) { cases().push_back({name, body}); }
};
inline void fail(const char* file, int line, const std::string& message) {
    ++failures();
    std::printf("    FAIL %s:%d: %s\n", file, line, message.c_str());
}
inline int run() {
    int failed_cases = 0;
    for (const Case& c : cases()) {
        const int before = failures();
        c.body();
        if (failures() != before) {
            ++failed_cases;
            std::printf("  failed: %s\n", c.name);
        }
    }
    std::printf("  %zu cases, %d failed\n", cases().size(), failed_cases);
    return failed_cases == 0 ? 0 : 1;
}

}  // namespace tg::test

#define TG_TEST(name)                                              \
    static void name();                                            \
    static const tg::test::Register tg_register_##name(#name, name); \
    static void name()

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) tg::test::fail(__FILE__, __LINE__, "CHECK(" #condition ")"); \
    } while (0)

#define CHECK_EQ(actual, expected)                                              \
    do {                                                                        \
        const auto tg_a = (actual);                                             \
        const auto tg_e = (expected);                                           \
        if (!(tg_a == tg_e)) {                                                  \
            std::ostringstream tg_m;                                            \
            tg_m << #actual << " == " << #expected << " (got " << tg_a << ", want " << tg_e << ")"; \
            tg::test::fail(__FILE__, __LINE__, tg_m.str());                     \
        }                                                                       \
    } while (0)
