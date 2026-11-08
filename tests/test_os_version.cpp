#include <cassert>
#include <cstdio>
#include <string_view>

#include "tg_os_version.h"

int main() {
    const std::string_view version = os_version::VERSION;
    unsigned parts = 1;
    bool digit = false;
    bool leading_zero = false;
    for (char c : version) {
        if (c == '.') {
            assert(digit && parts < 3);
            ++parts;
            digit = leading_zero = false;
        } else {
            assert(c >= '0' && c <= '9' && !leading_zero);
            leading_zero = !digit && c == '0';
            digit = true;
        }
    }
    assert(digit && parts == 3);
    const std::string_view label = os_version::LABEL;
    assert(label.substr(0, 9) == "Firmware ");
    assert(label.substr(9) == version);
    assert(label.size() <= 28); // Welcome label fits before its corner decoration.
    std::puts("OS SemVer and Firmware label tests passed");
}
