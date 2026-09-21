#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "ui/ui_layout.h"

static void close_to(float actual, float expected) {
    assert(fabsf(actual - expected) < 0.001f);
}

static void test_target(void) {
    const ui_layout_t layout = ui_layout_fit(UI_DISPLAY_SIZE_PX, UI_DISPLAY_SIZE_PX);
    close_to(layout.scale, 1.5f);
    close_to(layout.ox, 0);
    close_to(layout.oy, 0);
    close_to(ui_layout_x(&layout, UI_REFERENCE_SIZE / 2), 240);
    close_to(ui_layout_y(&layout, UI_REFERENCE_SIZE / 2), 240);
    close_to(UI_EDGE_MARGIN * layout.scale, 3);
    close_to((UI_REFERENCE_SIZE / 2 - UI_EDGE_MARGIN) * layout.scale, 237);
}

static void test_centering(void) {
    const ui_layout_t wide = ui_layout_fit(800, 480);
    close_to(wide.scale, 1.5f);
    close_to(ui_layout_x(&wide, 0), 160);
    close_to(ui_layout_x(&wide, UI_REFERENCE_SIZE), 640);
    close_to(ui_layout_y(&wide, 0), 0);

    const ui_layout_t tall = ui_layout_fit(480, 800);
    close_to(ui_layout_x(&tall, 0), 0);
    close_to(ui_layout_y(&tall, 0), 160);
    close_to(ui_layout_y(&tall, UI_REFERENCE_SIZE), 640);
}

static void test_scaling(void) {
    const ui_layout_t small = ui_layout_fit(240, 240);
    close_to(small.scale, 0.75f);
    close_to(ui_layout_x(&small, UI_REFERENCE_SIZE), 240);
    const ui_layout_t large = ui_layout_fit(960, 960);
    close_to(large.scale, 3);
    close_to(ui_layout_y(&large, UI_REFERENCE_SIZE), 960);
}

int main(void) {
    test_target();
    test_centering();
    test_scaling();
    puts("UI layout tests: OK");
    return 0;
}
