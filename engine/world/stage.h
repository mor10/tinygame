#pragma once

// Where things go on each screen. The stage is 10 blocks tall on every board;
// whatever height is left over becomes the status area above it.
//
//   320x172 at 16 px blocks: 12 px status strip, 20 x 10 block stage
//   280x240 at 16 px blocks: 80 px status panel, 17.5 x 10 block stage
//   280x240 with View::Fill:  17.5 x 15 block stage, status strip drawn on top

#include "../gfx/canvas.h"

namespace tg {

inline constexpr int STAGE_ROWS = 10;
inline constexpr int MIN_STAGE_COLS = 17;
inline constexpr int STATUS_STRIP_PX = 12;

enum class View { Standard, Fill };

struct StageRequest {
    View view = View::Standard;
    // A fixed-screen game (for example a brick breaker) names its playfield
    // in blocks and gets it centred. Zero means "use the whole stage".
    int playfield_cols = 0;
    int playfield_rows = 0;
};

struct StageLayout {
    int block = 0;                  // screen pixels per block
    gfx::Rect stage;                // where the world is drawn
    gfx::Rect status;               // score, lives and so on
    bool status_overlays = false;   // status is drawn over the stage (Fill)
    gfx::Rect playfield;            // equals `stage` unless a playfield was requested
};

StageLayout layoutStage(int screen_w, int screen_h, int block, const StageRequest& request);

}  // namespace tg
