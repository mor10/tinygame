#include "Arduino.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <emscripten.h>
#include <emscripten/heap.h>
#include <malloc.h>

SimSerial Serial;
SimESP ESP;

namespace {
const double started_at = emscripten_get_now();
double paused_at = 0.0;
double paused_total = 0.0;
bool clock_paused = false;

double simulatedTime() {
    return (clock_paused ? paused_at : emscripten_get_now()) - started_at - paused_total;
}

EM_JS(void, serialOutput, (const char* bytes, int size), {
    const text = UTF8ToString(bytes, size);
    if (Module.onSerial) Module.onSerial(text);
});
}  // namespace

std::uint32_t millis() {
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(simulatedTime()));
}

std::uint32_t micros() {
    return static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(simulatedTime() * 1000.0));
}

void simSetClockPaused(bool paused) {
    if (paused == clock_paused) return;
    const double now = emscripten_get_now();
    if (paused) paused_at = now;
    else paused_total += now - paused_at;
    clock_paused = paused;
}

void yield() {}

int SimSerial::available() const {
    return static_cast<int>((tail_ + sizeof(input_) - head_) % sizeof(input_));
}

int SimSerial::read() {
    if (head_ == tail_) return -1;
    const auto value = static_cast<unsigned char>(input_[head_]);
    head_ = (head_ + 1) % sizeof(input_);
    return value;
}

bool SimSerial::enqueue(const char* command) {
    if (!command) return false;
    const std::size_t size = std::strlen(command);
    const bool newline = size == 0 || command[size - 1] != '\n';
    if (size + newline > sizeof(input_) - 1 - available()) return false;
    for (std::size_t i = 0; i < size; ++i) {
        input_[tail_] = command[i];
        tail_ = (tail_ + 1) % sizeof(input_);
    }
    if (newline) {
        input_[tail_] = '\n';
        tail_ = (tail_ + 1) % sizeof(input_);
    }
    return true;
}

std::size_t SimSerial::write(const std::uint8_t* bytes, std::size_t size) {
    serialOutput(reinterpret_cast<const char*>(bytes), static_cast<int>(size));
    return size;
}

std::size_t SimSerial::print(const char* text) {
    return write(reinterpret_cast<const std::uint8_t*>(text), std::strlen(text));
}

std::size_t SimSerial::println(const char* text) {
    const auto size = print(text);
    return size + print("\n");
}

std::size_t SimSerial::printf(const char* format, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, format);
    const int length = std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (length <= 0) return 0;
    return write(reinterpret_cast<const std::uint8_t*>(buffer),
                 static_cast<std::size_t>(length) < sizeof(buffer)
                     ? static_cast<std::size_t>(length) : sizeof(buffer) - 1);
}

std::uint32_t SimESP::getFreeHeap() const {
    const auto used = static_cast<std::size_t>(mallinfo().uordblks);
    const auto size = emscripten_get_heap_size();
    return static_cast<std::uint32_t>(size > used ? size - used : 0);
}
