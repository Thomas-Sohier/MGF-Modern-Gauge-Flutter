#ifndef DASH_ICONS_H
#define DASH_ICONS_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DASH_ICON_COOLANT = 0,
    DASH_ICON_BATTERY,
    DASH_ICON_OIL,
    DASH_ICON_ENGINE,
    DASH_ICON_OBD_LINK
} dash_icon_type_t;

/**
 * Crée un widget LVGL transparent qui dessine une icône en pur LVGL.
 *
 * @param parent parent LVGL
 * @param type   type d'icône
 * @param size   largeur/hauteur du widget en pixels
 * @param color  couleur de l'icône
 */
lv_obj_t * dash_icon_create(lv_obj_t * parent,
                            dash_icon_type_t type,
                            lv_coord_t size,
                            lv_color_t color);

/**
 * Modifie la couleur d'une icône existante.
 */
void dash_icon_set_color(lv_obj_t * obj, lv_color_t color);

/**
 * Modifie la taille carrée du widget.
 */
void dash_icon_set_size(lv_obj_t * obj, lv_coord_t size);

#ifdef __cplusplus
}
#endif

#endif