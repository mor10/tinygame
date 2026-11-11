#pragma once

#include <cstdint>

namespace imu {

// Keep repeated read errors visible without flooding the serial console.
struct ReadHealth {
    enum class Notice { None, Failed, Recovered };
    bool ok = true;
    bool has_error_notice = false;
    bool failure_reported = false;
    std::uint32_t last_error_at = 0;

    Notice record(bool success, std::uint32_t now_ms) {
        Notice notice = Notice::None;
        if (success) {
            if (!ok && failure_reported) notice = Notice::Recovered;
            failure_reported = false;
        } else if (!has_error_notice || now_ms - last_error_at >= 5000) {
            last_error_at = now_ms;
            has_error_notice = true;
            failure_reported = true;
            notice = Notice::Failed;
        }
        ok = success;
        return notice;
    }
};

}  // namespace imu
