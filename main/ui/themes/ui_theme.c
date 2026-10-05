#include "ui/themes/ui_theme.h"

#include "domain/amber_palette.h"
#include "src/core/lv_obj_private.h"
#include "src/core/lv_obj_style_private.h"

// Palette active. Lue et écrite uniquement dans la tâche LVGL (création des
// écrans, callbacks de dessin, application des réglages).
static bool s_inverted;

static lv_color_t role_color(amber_palette_role_t role) {
    return lv_color_hex(amber_palette_rgb(s_inverted, role));
}

lv_color_t ui_theme_amber_bg(void) {
    return role_color(AMBER_PALETTE_ROLE_BG);
}

lv_color_t ui_theme_amber_bright(void) {
    return role_color(AMBER_PALETTE_ROLE_BRIGHT);
}

lv_color_t ui_theme_amber_dim(void) {
    return role_color(AMBER_PALETTE_ROLE_DIM);
}

lv_color_t ui_theme_amber_separator(void) {
    return role_color(AMBER_PALETTE_ROLE_SEPARATOR);
}

bool ui_theme_is_inverted(void) {
    return s_inverted;
}

void ui_theme_set_inverted(bool inverted) {
    s_inverted = inverted;
}

lv_color_t ui_theme_remap_color(lv_color_t color, bool from_inverted,
                                bool to_inverted) {
    uint32_t mapped = 0;
    if (!amber_palette_remap_rgb(lv_color_to_u32(color), from_inverted,
                                 to_inverted, &mapped)) {
        return color;
    }
    return lv_color_hex(mapped);
}

// Propriétés couleur susceptibles de porter une couleur de palette.
static const lv_style_prop_t k_color_props[] = {
    LV_STYLE_BG_COLOR,      LV_STYLE_BG_GRAD_COLOR, LV_STYLE_BG_IMAGE_RECOLOR,
    LV_STYLE_BORDER_COLOR,  LV_STYLE_OUTLINE_COLOR, LV_STYLE_SHADOW_COLOR,
    LV_STYLE_IMAGE_RECOLOR, LV_STYLE_LINE_COLOR,    LV_STYLE_ARC_COLOR,
    LV_STYLE_TEXT_COLOR,
};

static void restyle_object(lv_obj_t *obj, bool from_inverted,
                           bool to_inverted) {
    // Seuls les styles locaux sont concernés : l'UI n'utilise aucun style
    // partagé (lv_obj_remove_style_all + lv_obj_set_style_*). La liste des
    // styles n'est pas modifiée ici (on ne remplace que des propriétés déjà
    // présentes), l'itération reste donc valide.
    for (uint32_t i = 0; i < obj->style_cnt; i++) {
        const lv_obj_style_t *entry = &obj->styles[i];
        if (!entry->is_local || entry->style == NULL) continue;
        const lv_style_selector_t selector = entry->selector;
        for (size_t p = 0; p < sizeof(k_color_props) / sizeof(k_color_props[0]);
             p++) {
            lv_style_value_t value;
            if (lv_style_get_prop(entry->style, k_color_props[p], &value) !=
                LV_STYLE_RES_FOUND) {
                continue;
            }
            const lv_color_t mapped =
                ui_theme_remap_color(value.color, from_inverted, to_inverted);
            if (lv_color_eq(mapped, value.color)) continue;
            value.color = mapped;
            lv_obj_set_local_style_prop(obj, k_color_props[p], value, selector);
        }
    }
    const uint32_t count = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < count; i++) {
        restyle_object(lv_obj_get_child(obj, (int32_t)i), from_inverted,
                       to_inverted);
    }
}

void ui_theme_restyle_tree(lv_obj_t *root, bool from_inverted,
                           bool to_inverted) {
    if (root == NULL) return;
    if (from_inverted != to_inverted) {
        restyle_object(root, from_inverted, to_inverted);
    }
    // Les callbacks de dessin relisent la palette active au prochain rendu.
    lv_obj_invalidate(root);
}

bool ui_theme_apply_inverted(lv_obj_t *root, bool inverted) {
    const bool from = s_inverted;
    if (from == inverted) return false;
    s_inverted = inverted;
    ui_theme_restyle_tree(root, from, inverted);
    return true;
}
