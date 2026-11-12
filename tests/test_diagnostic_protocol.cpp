#include <cassert>

#include "../src/diagnostics/protocol.h"

int main() {
    assert(diagnostics::parseCommand("info") == diagnostics::SerialCommand::Info);
    assert(diagnostics::parseCommand("counter") == diagnostics::SerialCommand::Counter);
    assert(diagnostics::parseCommand("help") == diagnostics::SerialCommand::Help);
    assert(diagnostics::parseCommand("info ") == diagnostics::SerialCommand::Unknown);
    assert(diagnostics::parseCommand("flash") == diagnostics::SerialCommand::Unknown);
    assert(diagnostics::parseCommand(nullptr) == diagnostics::SerialCommand::Unknown);
}
