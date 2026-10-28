#include "devtools.h"

#include <Arduino.h>
#include <lvgl.h>

#include <cstdlib>
#include <cstring>

#include "input.h"
#include "dev_input.h"
#include "dev_line.h"
#include "registry.h"

namespace {

devtools::Overrides state;
devtools::LineReader reader;

constexpr char BASE64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Streams `size` bytes as base64 in lines of 1024 characters.
void writeBase64(const std::uint8_t* data, std::size_t size) {
    char out[1028];
    std::size_t used = 0;
    for (std::size_t i = 0; i < size; i += 3) {
        const std::uint32_t n = (data[i] << 16) | ((i + 1 < size ? data[i + 1] : 0) << 8) |
                                (i + 2 < size ? data[i + 2] : 0);
        out[used++] = BASE64[(n >> 18) & 63];
        out[used++] = BASE64[(n >> 12) & 63];
        out[used++] = i + 1 < size ? BASE64[(n >> 6) & 63] : '=';
        out[used++] = i + 2 < size ? BASE64[n & 63] : '=';
        if (used >= 1024) {
            out[used++] = '\n';
            Serial.write(reinterpret_cast<const std::uint8_t*>(out), used);
            used = 0;
        }
    }
    out[used++] = '\n';
    Serial.write(reinterpret_cast<const std::uint8_t*>(out), used);
}

// Renders the active screen (including any pause menu) and sends it.
void screenshot() {
    lv_draw_buf_t* buf = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB565);
    if (buf == nullptr) {
        Serial.println("[DEV] shot failed (out of memory)");
        return;
    }
    const std::uint32_t w = buf->header.w;
    const std::uint32_t h = buf->header.h;
    Serial.printf("SHOT %lu %lu RGB565LE\n", static_cast<unsigned long>(w),
                  static_cast<unsigned long>(h));
    for (std::uint32_t y = 0; y < h; ++y) {
        // Rows are sent packed; skip any stride padding.
        writeBase64(buf->data + y * buf->header.stride, w * 2);
    }
    Serial.println("END");
    lv_draw_buf_destroy(buf);
}

void info() {
    Serial.printf("[DEV] heap free %lu, psram free %lu\n",
                  static_cast<unsigned long>(ESP.getFreeHeap()),
                  static_cast<unsigned long>(ESP.getFreePsram()));
    for (std::uint8_t i = 0; i < registry::count(); ++i) {
        Serial.printf("[DEV] game %s \"%s\"\n", registry::get(i).id, registry::get(i).name);
    }
}

void help() {
    Serial.println(
        "[DEV] commands: shot | turn <left|right> <signed-transitions> |\n"
        "[DEV]   click|hold|down|up [left|right] |\n"
        "[DEV]   tilt <x> <y>|off | launch <id> | info | log on|off");
}

void run(char* command) {
    char* arg = std::strchr(command, ' ');
    if (arg != nullptr) {
        *arg++ = '\0';
    }
    if (std::strcmp(command, "shot") == 0) {
        screenshot();
        return;
    }
    const auto result = devtools::inputCommand(command, arg, state);
    if (result == devtools::CommandResult::Error) {
        Serial.println("[DEV] error: bad arguments; type help");
        return;
    }
    if (result == devtools::CommandResult::Ok) {
        Serial.println("[DEV] ok");
        return;
    }
    if (std::strcmp(command, "launch") == 0 && arg != nullptr) {
        state.launch = registry::find(arg);
        if (state.launch < 0) {
            Serial.printf("[DEV] error: no game with id %s\n", arg);
            return;
        }
    } else if (std::strcmp(command, "info") == 0) {
        info();
    } else if (std::strcmp(command, "help") == 0) {
        help();
    } else if (std::strcmp(command, "log") == 0 && arg != nullptr &&
               (std::strcmp(arg, "on") == 0 || std::strcmp(arg, "off") == 0)) {
        state.input_log = std::strcmp(arg, "on") == 0;
    } else {
        help();
        Serial.println("[DEV] error: unknown command or bad arguments");
        return;
    }
    Serial.println("[DEV] ok");
}

}  // namespace

namespace devtools {

void poll() {
    while (Serial.available() > 0) {
        const auto event = reader.feed(static_cast<char>(Serial.read()));
        if (event == LineReader::Event::None) {
            continue;
        }
        if (event == LineReader::Event::TooLong) {
            Serial.println("[DEV] error: command too long");
        } else {
            run(reader.line());
        }
        return;  // one command per frame, so back-to-back clicks stay separate
    }
}

Overrides& overrides() {
    return state;
}

void endFrame() {
    state.clearFrame();
}

}  // namespace devtools
