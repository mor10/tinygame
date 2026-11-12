#pragma once

#include <cstddef>
#include <cstdint>

#include "bundle_script.h"
#include "fixed_step_runner.h"
#include "game_store.h"
#include "lua_presenter.h"
#include "tinygame.h"

namespace os::lua_game {

using BundleRelease = void (*)(const std::uint8_t*);

#ifdef __EMSCRIPTEN__
struct EmbeddedBundle {
    const char* id;
    const char* name;
    const std::uint8_t* bytes;
    std::size_t size;
};

void registerSimulatorGames(const game_store::InstalledGame* installed,
                            std::size_t installed_count);
#endif

class BundleGame final : public tg::Game {
public:
    BundleGame(const std::uint8_t* bundle, std::size_t bundle_size,
               const lua_presenter::Viewport& viewport, std::uint32_t seed,
               BundleRelease release = nullptr, const char* load_error = nullptr);
    ~BundleGame() override;

    void start(lv_obj_t* screen) override;
    void update(const tg::Input& input, float delta_seconds) override;
    void updateTimed(const tg::Input& input, float delta_seconds,
                     std::uint32_t elapsed_us) override;
    void paused(bool is_paused) override;
    void inputBlocked() override;

private:
    void showFailure(lv_obj_t* screen, const char* message);

    const std::uint8_t* bundle_;
    std::size_t bundle_size_;
    lua_presenter::Viewport viewport_;
    BundleRelease release_;
    const char* load_error_;
    tg::engine::BundleScript script_;
    tg::engine::FixedStepRunner runner_;
    lua_presenter::Presenter* presenter_ = nullptr;
    lv_obj_t* error_title_ = nullptr;
    lv_obj_t* error_detail_ = nullptr;
    bool failed_ = false;
    bool overloaded_ = false;
    std::uint32_t ticks_since_presentation_ = 2;
};

tg::Game* createInstalledGame(const char* id);

}  // namespace os::lua_game
