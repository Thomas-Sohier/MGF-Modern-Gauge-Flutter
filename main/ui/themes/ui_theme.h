#pragma once

#include <stdbool.h>

#include "lvgl.h"

// Palette du style ambre cible. Les coordonnées et dimensions du cadran
// restent dans style_amber.c afin de préserver sa géométrie validée.
//
// Deux palettes (domain/amber_palette.h) : normale (ambre sur fond sombre)
// ou inversée (fond ambre, premier plan quasi noir). Les accesseurs renvoient
// toujours le rôle demandé dans la palette ACTIVE ; les callbacks de dessin
// qui les appellent suivent donc la palette sans autre action. Toutes les
// fonctions s'appellent dans la tâche LVGL.
lv_color_t ui_theme_amber_bg(void);
lv_color_t ui_theme_amber_bright(void);
lv_color_t ui_theme_amber_dim(void);
lv_color_t ui_theme_amber_separator(void);

bool ui_theme_is_inverted(void);
// Choisit la palette SANS toucher aux objets existants : à appeler avant de
// créer l'interface (démarrage, simulateur).
void ui_theme_set_inverted(bool inverted);

// Couleur du même rôle dans l'autre palette ; une couleur hors palette est
// renvoyée telle quelle. Exact et réversible (aucune dérive).
lv_color_t ui_theme_remap_color(lv_color_t color, bool from_inverted,
                                bool to_inverted);

// Remappe les couleurs de palette des styles locaux de `root` et de tous ses
// descendants (textes, fonds, bordures, lignes, arcs, icônes...), puis
// invalide `root` pour que les callbacks de dessin relisent la palette.
// N'agit pas sur les pixels des images (pochette : cf. music_cover).
void ui_theme_restyle_tree(lv_obj_t *root, bool from_inverted,
                           bool to_inverted);

// Bascule la palette active et restyle `root` en place, sans recréer les
// écrans (leur état est conservé). false si la palette était déjà active.
bool ui_theme_apply_inverted(lv_obj_t *root, bool inverted);
