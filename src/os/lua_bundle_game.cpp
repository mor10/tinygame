#include "lua_bundle_game.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <new>

#include "tg_board.h"
#include "tg_style.h"
#ifdef __EMSCRIPTEN__
#include "registry.h"
#endif

namespace os::lua_game {

#ifdef __EMSCRIPTEN__
extern const EmbeddedBundle tg_sim_bundles[];
extern const std::size_t tg_sim_bundle_count;
#endif

BundleGame::BundleGame(const std::uint8_t* bundle, std::size_t bundle_size,
                       const lua_presenter::Viewport& viewport, std::uint32_t seed,
                       BundleRelease release, const char* load_error)
    : bundle_(bundle), bundle_size_(bundle_size), viewport_(viewport), release_(release),
      load_error_(load_error), script_(seed) {}

BundleGame::~BundleGame() {
    delete presenter_;
    if (release_ != nullptr && bundle_ != nullptr) release_(bundle_);
}

void BundleGame::start(lv_obj_t* screen) {
    if (load_error_ != nullptr) {
        showFailure(screen, load_error_);
        return;
    }
    if (bundle_ == nullptr || !script_.load(bundle_, bundle_size_)) {
        showFailure(screen, script_.error());
        return;
    }

    presenter_ = new (std::nothrow) lua_presenter::Presenter(screen, viewport_);
    if (presenter_ == nullptr || !presenter_->ready()) {
        showFailure(screen, "Could not create game display.");
        return;
    }

    error_title_ = tg::ui::label(screen, "", tg::Font::M, tg::color::BAD);
    error_detail_ = tg::ui::label(screen, "", tg::Font::S, tg::color::TEXT);
    lv_obj_set_pos(error_title_, 8, 8);
    lv_obj_set_pos(error_detail_, 8, 28);
    lv_obj_set_width(error_detail_, tg::board::WIDTH - 16);
    lv_obj_set_hidden(error_title_, true);
    lv_obj_set_hidden(error_detail_, true);
}

void BundleGame::update(const tg::Input& input, float delta_seconds) {
    if (!std::isfinite(delta_seconds) || delta_seconds < 0.0f ||
        static_cast<double>(delta_seconds) * 1'000'000.0 >
            std::numeric_limits<std::uint32_t>::max()) {
        showFailure(nullptr, "Invalid game time.");
        return;
    }
    updateTimed(input, delta_seconds,
                static_cast<std::uint32_t>(static_cast<double>(delta_seconds) * 1'000'000.0));
}

void BundleGame::updateTimed(const tg::Input& input, float, std::uint32_t elapsed_us) {
    if (failed_ || presenter_ == nullptr) return;
    const auto result = runner_.advance(elapsed_us, input, [this](const tg::Input& tick_input) {
        return script_.update(tg::engine::FixedStepRunner::STEP_SECONDS, tick_input,
                              presenter_->worldSize());
    });
    if (result.error != tg::engine::StepError::None) {
        presenter_->hide();
        showFailure(nullptr, result.error == tg::engine::StepError::CallbackFailed
                                 ? script_.error()
                                 : tg::engine::stepErrorMessage(result.error));
        return;
    }
    if (result.pending_steps > 0 && !overloaded_) {
        tg::log("Simulation catching up (%llu ticks pending)",
                static_cast<unsigned long long>(result.pending_steps));
        overloaded_ = true;
    } else if (result.pending_steps == 0) {
        overloaded_ = false;
    }
    ticks_since_presentation_ += result.steps;
    if (result.steps > 0 && ticks_since_presentation_ >= 2) {
        presenter_->present(script_.frame());
        ticks_since_presentation_ = 0;
    }
}

void BundleGame::paused(bool) {
    runner_.discardInput();
}

void BundleGame::inputBlocked() {
    runner_.discardInput();
}

void BundleGame::showFailure(lv_obj_t* screen, const char* message) {
    failed_ = true;
    if (screen != nullptr) {
        error_title_ = tg::ui::label(screen, "GAME ERROR", tg::Font::M, tg::color::BAD);
        error_detail_ = tg::ui::label(screen, message != nullptr ? message : "Invalid game bundle.",
                                      tg::Font::S, tg::color::TEXT);
        lv_obj_set_pos(error_title_, 8, 8);
        lv_obj_set_pos(error_detail_, 8, 28);
        lv_obj_set_width(error_detail_, tg::board::WIDTH - 16);
    } else {
        if (error_title_ != nullptr) lv_obj_set_hidden(error_title_, false);
        if (error_detail_ != nullptr) {
            tg::ui::text(error_detail_, "%s", message != nullptr ? message : "Game stopped.");
            lv_obj_set_hidden(error_detail_, false);
        }
    }
    tg::log("Lua game stopped: %s", message != nullptr ? message : "unknown error");
}

#ifdef __EMSCRIPTEN__
void registerSimulatorGames(const game_store::InstalledGame* installed,
                            std::size_t installed_count) {
    for (std::size_t i = 0; i < tg_sim_bundle_count; ++i) {
        const EmbeddedBundle& bundle = tg_sim_bundles[i];
        bool replaced = false;
        for (std::size_t j = 0; j < installed_count; ++j) {
            if (std::strcmp(bundle.id, installed[j].id) == 0) {
                replaced = true;
                break;
            }
        }
        if (replaced) continue;
        const tg::GameInfo info{
            bundle.id, bundle.name, tg::color::MINT, nullptr, createInstalledGame};
        if (!registry::add(info)) {
            tg::log("Could not register simulator bundle %s", bundle.id);
        }
    }
}
#endif

tg::Game* createInstalledGame(const char* id) {
    lua_presenter::Viewport viewport;
    viewport.x = tg::board::SAFE_X;
    viewport.y = tg::board::SAFE_TOP;
    viewport.width = tg::board::WIDTH - 2 * tg::board::SAFE_X;
    viewport.height = tg::board::HEIGHT - tg::board::SAFE_TOP - tg::board::SAFE_BOTTOM;
    viewport.world = {viewport.width / 2.0f, viewport.height / 2.0f};
    std::uint8_t* bytes = nullptr;
    std::size_t size = 0;
    const game_store::Error result = game_store::loadBundle(id, bytes, size);
    if (result == game_store::Error::None) {
        return new BundleGame(bytes, size, viewport, lv_tick_get(), game_store::releaseBundle);
    }
#ifndef __EMSCRIPTEN__
    return new BundleGame(
        nullptr, 0, viewport, lv_tick_get(), nullptr, game_store::errorMessage(result));
#else
    if (result != game_store::Error::NotFound) {
        return new BundleGame(
            nullptr, 0, viewport, lv_tick_get(), nullptr, game_store::errorMessage(result));
    }
    if (id == nullptr) return nullptr;
    for (std::size_t i = 0; i < tg_sim_bundle_count; ++i) {
        const EmbeddedBundle& bundle = tg_sim_bundles[i];
        if (std::strcmp(id, bundle.id) == 0) {
            return new BundleGame(bundle.bytes, bundle.size, viewport, lv_tick_get());
        }
    }
    return nullptr;
#endif
}

}  // namespace os::lua_game
