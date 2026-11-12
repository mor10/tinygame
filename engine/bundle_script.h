#pragma once

#include <cstddef>
#include <cstdint>

#include "bundle_view.h"
#include "lua_runtime.h"
#include "tg_types.h"

namespace tg::engine {

class BundleScript {
public:
    explicit BundleScript(std::uint32_t seed = 0x6d2b79f5);

    BundleScript(const BundleScript&) = delete;
    BundleScript& operator=(const BundleScript&) = delete;

    // The bundle bytes must remain alive and unchanged while this object is used.
    bool load(const std::uint8_t* bytes, std::size_t size);
    bool update(float delta_seconds);
    bool update(float delta_seconds, const tg::Input& input);
    bool update(float delta_seconds, const tg::Input& input, const WorldSize& world);

    bool ready() const { return loaded_ && runtime_.ready(); }
    BundleError bundleError() const { return bundle_.error(); }
    const DrawFrame& frame() const { return runtime_.frame(); }
    const char* error() const;

private:
    static bool resolveModule(void* context, const char* name, std::size_t name_size,
                              LuaModuleSource& module);

    BundleView bundle_;
    LuaRuntime runtime_;
    bool loaded_ = false;
    const char* error_ = "";
};

}  // namespace tg::engine
