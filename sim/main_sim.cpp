#include <Arduino.h>
#include <emscripten.h>

#include <algorithm>
#include <cstdint>
#include <string_view>

#include "bundle_view.h"
#include "crc32.h"
#include "game_store.h"
#include "manifest_view.h"
#include "transfer_codec.h"
#include "sim_hal.h"
#include "tg_board.h"

void setup();
void loop();

namespace {
bool stopped = false;
bool wake_requested = false;
bool hidden = false;

EM_JS(void, notifyWake, (int side), {
    if (Module.onWake) Module.onWake(side);
});

void frame() {
    if (!stopped && !sim::sleeping()) loop();
}
}  // namespace

extern "C" {
EMSCRIPTEN_KEEPALIVE void tg_sim_turn(int side, std::int32_t delta) {
    sim::turn(side, delta);
}
EMSCRIPTEN_KEEPALIVE void tg_sim_button(int side, int down) {
    if (side < 0 || side > 1) return;
    if (sim::sleeping()) {
        // Deep sleep loses RAM on the board. The host creates a fresh factory
        // instance on wake rather than rerunning setup over live OS globals.
        if (down && !wake_requested) {
            wake_requested = true;
            notifyWake(side);
        }
        return;
    }
    sim::button(side, down != 0);
}
EMSCRIPTEN_KEEPALIVE void tg_sim_tilt(float x, float y) { sim::tilt(x, y); }
EMSCRIPTEN_KEEPALIVE int tg_sim_command(const char* command) {
    return !stopped && !sim::sleeping() && Serial.enqueue(command);
}
EMSCRIPTEN_KEEPALIVE void tg_sim_serial_connected(int connected) {
    Serial.setConnected(connected != 0);
}
EMSCRIPTEN_KEEPALIVE int tg_sim_install_bundle(const std::uint8_t* bytes, std::uint32_t size) {
    if (stopped || sim::sleeping() || bytes == nullptr || size == 0) {
        const auto error = game_store::Error::InvalidBundle;
        Serial.printf("[STORE] bundle install failed: %s\n", game_store::errorMessage(error));
        return static_cast<int>(error);
    }

    tg::engine::BundleView bundle;
    if (!bundle.parse(bytes, size)) {
        const auto error = game_store::Error::InvalidBundle;
        Serial.printf("[STORE] bundle install failed: %s\n", game_store::errorMessage(error));
        return static_cast<int>(error);
    }
    tg::engine::BundleFileView manifest_file;
    tg::engine::ManifestView manifest;
    if (!bundle.find("game.toml", manifest_file) ||
        !tg::engine::parseManifest(std::string_view(
            reinterpret_cast<const char*>(manifest_file.data), manifest_file.size), manifest)) {
        const auto error = game_store::Error::InvalidManifest;
        Serial.printf("[STORE] bundle install failed: %s\n", game_store::errorMessage(error));
        return static_cast<int>(error);
    }

    game_store::Error result = game_store::beginInstall(manifest.id, size);
    if (result != game_store::Error::None) {
        Serial.printf("[STORE] bundle install failed: %s\n", game_store::errorMessage(result));
        return static_cast<int>(result);
    }
    for (std::size_t offset = 0; offset < size;) {
        const std::size_t amount = std::min<std::size_t>(
            tg::engine::transfer::CHUNK_BYTES, size - offset);
        result = game_store::writeChunk(offset, bytes + offset, amount);
        if (result != game_store::Error::None) {
            game_store::cancelInstall();
            return static_cast<int>(result);
        }
        offset += amount;
    }
    result = game_store::finishInstall(tg::engine::crc32(bytes, size));
    if (result != game_store::Error::None) {
        Serial.printf("[STORE] bundle install failed: %s\n", game_store::errorMessage(result));
    } else {
        Serial.printf("[STORE] installed %s (%u bytes); restart to launch\n",
                      manifest.id, static_cast<unsigned>(size));
    }
    return static_cast<int>(result);
}
EMSCRIPTEN_KEEPALIVE const std::uint16_t* tg_sim_framebuffer() {
    return sim::framebuffer();
}
EMSCRIPTEN_KEEPALIVE int tg_sim_width() { return tg::board::WIDTH; }
EMSCRIPTEN_KEEPALIVE int tg_sim_height() { return tg::board::HEIGHT; }
EMSCRIPTEN_KEEPALIVE int tg_sim_sleeping() { return sim::sleeping(); }
EMSCRIPTEN_KEEPALIVE int tg_sim_brightness() { return sim::brightness(); }
EMSCRIPTEN_KEEPALIVE std::uint32_t tg_sim_led() { return sim::led(); }
EMSCRIPTEN_KEEPALIVE void tg_sim_stop() {
    stopped = true;
    emscripten_cancel_main_loop();
}
EMSCRIPTEN_KEEPALIVE void tg_sim_pause() { emscripten_pause_main_loop(); }
EMSCRIPTEN_KEEPALIVE void tg_sim_resume() {
    if (!stopped) emscripten_resume_main_loop();
}
EMSCRIPTEN_KEEPALIVE void tg_sim_visible(int visible) {
    if (stopped) return;
    const bool next_hidden = visible == 0;
    if (hidden == next_hidden) return;
    hidden = next_hidden;
    if (visible) {
        simSetClockPaused(false);
        emscripten_resume_main_loop();
    } else {
        emscripten_pause_main_loop();
        simSetClockPaused(true);
    }
}
}

int main() {
    setup();
    frame();
    emscripten_set_main_loop(frame, 0, false);
    return 0;
}
