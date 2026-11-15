#pragma once
// Static cart checks used by `tinygame-sim --check`.
#include <stddef.h>

// level: 1 = error (the cart won't work as written), 0 = warning. line is -1 when not tied to one.
typedef void (*lint_report_fn)(void *ctx, int level, int line, const char *msg);

int lint_source(const char *src, size_t len, lint_report_fn report, void *ctx);   // returns the error count
int lint_icon(const char *art, lint_report_fn report, void *ctx);                 // returns the error count
