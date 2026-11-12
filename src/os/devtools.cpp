#include "devtools.h"

#include <Arduino.h>
#include <lvgl.h>

#include <cstdlib>
#include <cstring>

#include "input.h"
#include "dev_input.h"
#include "dev_line.h"
#include "transfer_codec.h"
#include "crc32.h"
#include "game_store.h"
#include "registry.h"
#include "tg_board.h"

namespace {

devtools::Overrides state;
devtools::LineReader reader;
controller_stream::State controller;
bool stream_format_error = false;

constexpr std::size_t INSTALL_CHUNK_BYTES = tg::engine::transfer::CHUNK_BYTES;
constexpr std::uint32_t INSTALL_TIMEOUT_MS = 30000;
bool install_active = false;
std::uint32_t install_crc32 = 0;
std::uint32_t install_last_activity = 0;

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
    Serial.printf("[DEV] game storage ready=%d total=%lu free=%lu max-bundle=%lu\n",
                  game_store::ready(),
                  static_cast<unsigned long>(game_store::totalBytes()),
                  static_cast<unsigned long>(game_store::freeBytes()),
                  static_cast<unsigned long>(game_store::maxBundleBytes()));
    for (std::uint8_t i = 0; i < registry::count(); ++i) {
        Serial.printf("[DEV] game %s \"%s\"\n", registry::get(i).id, registry::get(i).name);
    }
}

void help() {
    Serial.println(
        "[DEV] commands: shot | turn <left|right> <signed-transitions> |\n"
        "[DEV]   click|hold|down|up [left|right] |\n"
        "[DEV]   tilt <x> <y>|off | launch <id> | info | log on|off |\n"
        "[DEV]   stream on|off|ping | install 1 capabilities|begin|chunk|finish|cancel ...");
}

char* nextToken(char*& cursor) {
    while (cursor != nullptr && (*cursor == ' ' || *cursor == '\t')) ++cursor;
    if (cursor == nullptr || *cursor == '\0') return nullptr;
    char* token = cursor;
    while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') ++cursor;
    if (*cursor != '\0') *cursor++ = '\0';
    return token;
}

void installError(game_store::Error error) {
    Serial.printf("[DEV] error: %s\n", game_store::errorMessage(error));
}

bool installCommand(char* arguments) {
    std::uint8_t bytes[INSTALL_CHUNK_BYTES];
    char* cursor = arguments;
    char* version = nextToken(cursor);
    char* action = nextToken(cursor);
    if (version == nullptr || std::strcmp(version, "1") != 0 || action == nullptr) {
        Serial.println("[DEV] error: use install 1 capabilities|begin|chunk|finish|cancel ...");
        return false;
    }

    if (std::strcmp(action, "capabilities") == 0) {
        if (nextToken(cursor) != nullptr) {
            Serial.println("[DEV] error: install 1 capabilities takes no arguments");
            return false;
        }
        Serial.printf("[DEV] install-capabilities 1 profile=%s game-api=1\n", tg::board::ID);
    } else if (std::strcmp(action, "begin") == 0) {
        char* id = nextToken(cursor);
        char* size_text = nextToken(cursor);
        char* checksum_text = nextToken(cursor);
        std::uint32_t size = 0;
        std::uint32_t checksum = 0;
        if (id == nullptr || size_text == nullptr || checksum_text == nullptr ||
            nextToken(cursor) != nullptr ||
            !tg::engine::transfer::parseNumber(size_text, size, 10) ||
            std::strlen(checksum_text) != 8 ||
            !tg::engine::transfer::parseNumber(checksum_text, checksum, 16)) {
            Serial.println("[DEV] error: install 1 begin <id> <size> <crc32hex>");
            return false;
        }
        const auto result = game_store::beginInstall(id, size);
        if (result != game_store::Error::None) {
            installError(result);
            return false;
        }
        install_active = true;
        install_crc32 = checksum;
        install_last_activity = millis();
    } else if (std::strcmp(action, "chunk") == 0) {
        char* offset_text = nextToken(cursor);
        char* checksum_text = nextToken(cursor);
        char* encoded = nextToken(cursor);
        std::uint32_t offset = 0;
        std::uint32_t checksum = 0;
        std::size_t size = 0;
        if (!install_active || offset_text == nullptr || checksum_text == nullptr ||
            encoded == nullptr || nextToken(cursor) != nullptr ||
            !tg::engine::transfer::parseNumber(offset_text, offset, 10) ||
            std::strlen(checksum_text) != 8 ||
            !tg::engine::transfer::parseNumber(checksum_text, checksum, 16) ||
            !tg::engine::transfer::decodeBase64(
                encoded, bytes, sizeof(bytes), size) ||
            tg::engine::crc32(bytes, size) != checksum) {
            if (install_active) {
                game_store::cancelInstall();
                install_active = false;
            }
            Serial.println("[DEV] error: invalid install chunk or checksum");
            return false;
        }
        const auto result = game_store::writeChunk(offset, bytes, size);
        if (result != game_store::Error::None) {
            game_store::cancelInstall();
            install_active = false;
            installError(result);
            return false;
        }
        install_last_activity = millis();
    } else if (std::strcmp(action, "finish") == 0) {
        if (nextToken(cursor) != nullptr || !install_active) {
            Serial.println("[DEV] error: no active install to finish");
            return false;
        }
        const auto result = game_store::finishInstall(install_crc32);
        install_active = false;
        if (result != game_store::Error::None) {
            if (result == game_store::Error::Incomplete) game_store::cancelInstall();
            installError(result);
            return false;
        }
    } else if (std::strcmp(action, "cancel") == 0) {
        if (nextToken(cursor) != nullptr || !install_active) {
            Serial.println("[DEV] error: no active install to cancel");
            return false;
        }
        const auto result = game_store::cancelInstall();
        install_active = false;
        if (result != game_store::Error::None) {
            installError(result);
            return false;
        }
    } else {
        Serial.println("[DEV] error: unknown install action");
        return false;
    }
    Serial.println("[DEV] ok");
    return true;
}

void run(char* command) {
    char* arg = std::strchr(command, ' ');
    if (arg != nullptr) {
        *arg++ = '\0';
    }
    if (install_active && std::strcmp(command, "install") != 0) {
        Serial.println("[DEV] error: install active; finish or cancel first");
        return;
    }
    if (controller.active() && std::strcmp(command, "install") == 0) {
        Serial.println("[DEV] error: controller mode; use stream off first");
        return;
    }
    if (std::strcmp(command, "stream") == 0) {
        if (arg != nullptr && std::strcmp(arg, "on") == 0 && Serial) {
            state = devtools::Overrides{};
            controller.start(millis());
            stream_format_error = false;
        } else if (arg != nullptr && std::strcmp(arg, "off") == 0) {
            controller.stop();
        } else if (arg != nullptr && std::strcmp(arg, "ping") == 0 &&
                   controller.ping(millis())) {
            // Heartbeats keep the controller session alive without local activity.
        } else {
            Serial.println("[DEV] error: use stream on|off|ping (ping needs stream on)");
            return;
        }
        Serial.println("[DEV] ok");
        return;
    }
    if (std::strcmp(command, "shot") == 0) {
        screenshot();
        return;
    }
    if (std::strcmp(command, "install") == 0) {
        installCommand(arg);
        return;
    }
    if (controller.active() && std::strcmp(command, "help") != 0 &&
        std::strcmp(command, "info") != 0) {
        Serial.println("[DEV] error: controller mode; use stream off first");
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
    controller.check(millis(), static_cast<bool>(Serial));
    if (install_active && millis() - install_last_activity >= INSTALL_TIMEOUT_MS) {
        game_store::cancelInstall();
        install_active = false;
        Serial.println("[DEV] error: install timed out and was cancelled");
    }
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

bool controlling() {
    return controller.active();
}

void streamInput(const controller_stream::Sample& sample, std::uint32_t now) {
    if (!controller.due(now, sample)) return;
    char line[128];
    if (!controller_stream::format(line, sizeof(line), sample)) {
        if (!stream_format_error)
            Serial.println("[DEV] error: invalid controller sample (nonfinite tilt or line overflow)");
        stream_format_error = true;
        return;
    }
    stream_format_error = false;
    // Avoid blocking the OS when a connected host stops reading.
    const auto size = std::strlen(line);
    if (Serial.availableForWrite() < static_cast<int>(size)) return;
    if (Serial.write(reinterpret_cast<const std::uint8_t*>(line), size) == size)
        controller.sent(now, sample);
}

void endFrame() {
    state.clearFrame();
}

}  // namespace devtools
