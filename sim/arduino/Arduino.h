#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>

std::uint32_t millis();
std::uint32_t micros();
void yield();

class SimSerial {
public:
    void begin(unsigned long) {}
    int available() const;
    int availableForWrite() const { return 4096; }
    explicit operator bool() const { return false; }
    int read();
    std::size_t write(const std::uint8_t* bytes, std::size_t size);
    std::size_t write(std::uint8_t byte) { return write(&byte, 1); }
    std::size_t print(const char* text);
    std::size_t println(const char* text = "");
    std::size_t printf(const char* format, ...);
    bool enqueue(const char* command);

private:
    char input_[4096]{};
    std::size_t head_ = 0;
    std::size_t tail_ = 0;
};

class SimESP {
public:
    std::uint32_t getFreeHeap() const;
    std::uint32_t getFreePsram() const { return getFreeHeap(); }
    std::uint32_t getPsramSize() const { return 8 * 1024 * 1024; }
};

extern SimSerial Serial;
extern SimESP ESP;
