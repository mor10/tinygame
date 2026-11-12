#include "bundle_script.h"

#include <cstring>
#include <string_view>

#include "manifest_view.h"

namespace tg::engine {

BundleScript::BundleScript(std::uint32_t seed) : runtime_(seed) {}

bool BundleScript::load(const std::uint8_t* bytes, std::size_t size) {
    loaded_ = false;
    error_ = "";
    runtime_.reset();

    if (!bundle_.parse(bytes, size)) {
        error_ = "Invalid game bundle";
        return false;
    }

    BundleFileView manifest_file;
    ManifestView manifest;
    if (!bundle_.find("game.toml", manifest_file) ||
        !parseManifest(std::string_view(reinterpret_cast<const char*>(manifest_file.data),
                                        manifest_file.size), manifest)) {
        error_ = "Invalid game manifest";
        return false;
    }

    BundleFileView rules;
    if (!bundle_.find("rules.lua", rules)) {
        error_ = "Bundle is missing rules.lua";
        return false;
    }
    if (!runtime_.load(reinterpret_cast<const char*>(rules.data), rules.size,
                       "rules.lua", resolveModule, &bundle_)) {
        return false;
    }

    loaded_ = true;
    return true;
}

bool BundleScript::resolveModule(void* context, const char* name, std::size_t name_size,
                                 LuaModuleSource& module) {
    if (context == nullptr || name == nullptr || name_size == 0 ||
        name_size > LuaRuntime::MAX_MODULE_NAME_BYTES) {
        return false;
    }
    constexpr char prefix[] = "modules/";
    constexpr char suffix[] = ".lua";
    char path[BundleView::MAX_PATH_BYTES + 1] = {};
    std::memcpy(path, prefix, sizeof(prefix) - 1);
    std::memcpy(path + sizeof(prefix) - 1, name, name_size);
    std::memcpy(path + sizeof(prefix) - 1 + name_size, suffix, sizeof(suffix));

    BundleFileView file;
    const std::size_t path_size = sizeof(prefix) - 1 + name_size + sizeof(suffix) - 1;
    if (!static_cast<const BundleView*>(context)->find(
            std::string_view(path, path_size), file)) {
        return false;
    }
    module.source = reinterpret_cast<const char*>(file.data);
    module.size = file.size;
    return true;
}

bool BundleScript::update(float delta_seconds) {
    return update(delta_seconds, tg::Input{});
}

bool BundleScript::update(float delta_seconds, const tg::Input& input) {
    return update(delta_seconds, input, WorldSize{});
}

bool BundleScript::update(float delta_seconds, const tg::Input& input,
                          const WorldSize& world) {
    if (!ready()) {
        error_ = "Game script is not ready";
        return false;
    }
    if (!runtime_.callUpdate(delta_seconds, input, world)) return false;
    return true;
}

const char* BundleScript::error() const {
    return runtime_.error()[0] == '\0' ? error_ : runtime_.error();
}

}  // namespace tg::engine
