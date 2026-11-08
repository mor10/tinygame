#pragma once

// Bump for OS releases: MAJOR = breaking, MINOR = feature, PATCH = fix.
#define TG_FIRMWARE_VERSION "1.0.0"

namespace os_version {
inline constexpr char VERSION[] = TG_FIRMWARE_VERSION;
inline constexpr char LABEL[] = "Firmware " TG_FIRMWARE_VERSION;
}  // namespace os_version

#undef TG_FIRMWARE_VERSION
