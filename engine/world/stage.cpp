#include "stage.h"

namespace tg {

StageLayout layoutStage(int screen_w, int screen_h, int block, const StageRequest& request) {
    StageLayout out;
    out.block = block < 1 ? 1 : block;
    int stage_h = STAGE_ROWS * out.block;
    if (stage_h > screen_h) stage_h = screen_h;
    const int spare = screen_h - stage_h;

    if (request.view == View::Fill && spare >= out.block) {
        // Enough room for extra rows: the world takes the whole screen.
        out.stage = {0, 0, screen_w, screen_h};
        out.status = {0, 0, screen_w, STATUS_STRIP_PX};
        out.status_overlays = true;
    } else {
        out.status = {0, 0, screen_w, spare};
        out.stage = {0, spare, screen_w, stage_h};
    }

    out.playfield = out.stage;
    if (request.playfield_cols > 0 && request.playfield_rows > 0) {
        const int w = request.playfield_cols * out.block;
        const int h = request.playfield_rows * out.block;
        out.playfield = {out.stage.x + (out.stage.w - w) / 2, out.stage.y + (out.stage.h - h) / 2,
                         w, h};
    }
    return out;
}

}  // namespace tg
