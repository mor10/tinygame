#pragma once

#include <cstring>

namespace diagnostics {

enum class SerialCommand { Info, Counter, Help, Unknown };

inline SerialCommand parseCommand(const char* command) {
    if (command == nullptr) return SerialCommand::Unknown;
    if (std::strcmp(command, "info") == 0) return SerialCommand::Info;
    if (std::strcmp(command, "counter") == 0) return SerialCommand::Counter;
    if (std::strcmp(command, "help") == 0) return SerialCommand::Help;
    return SerialCommand::Unknown;
}

}  // namespace diagnostics
