#pragma once

// Physical target and logical design space. No implicit padding: components
// share one uniform scale, centered in the largest square that fits the host.
#define UI_DISPLAY_SIZE_PX 480
#define UI_REFERENCE_SIZE 320.0f
#define UI_EDGE_MARGIN 2.0f // 3 physical pixels at 480x480

typedef struct {
    float ox;
    float oy;
    float scale;
} ui_layout_t;

// Width/height must be positive. Coordinates are local to the host.
static inline ui_layout_t ui_layout_fit(float width, float height) {
    const float side = width < height ? width : height;
    return (ui_layout_t){
        .ox = (width - side) * 0.5f,
        .oy = (height - side) * 0.5f,
        .scale = side / UI_REFERENCE_SIZE,
    };
}

static inline float ui_layout_x(const ui_layout_t *layout, float x) {
    return layout->ox + x * layout->scale;
}

static inline float ui_layout_y(const ui_layout_t *layout, float y) {
    return layout->oy + y * layout->scale;
}
