#include "ui/fonts/ui_fonts.h"

const lv_font_t *ui_font_xl = NULL;
const lv_font_t *ui_font_l = NULL;
const lv_font_t *ui_font_m = NULL;

// Tailles (px) des trois polices, calées sur le panneau réel 480x480 de
// l'ESP32-S3-Touch-LCD-2.1 (les polices tiny_ttf sont en px fixes : elles
// doivent correspondre à la résolution de rendu, ici 480).
#define UI_FONT_XL_PX 56   // « 800 »
#define UI_FONT_L_PX  20
#define UI_FONT_M_PX  16   // légendes compactes, adaptées aux zones sûres du disque

void ui_fonts_init(const void *ttf_data, size_t ttf_size) {
    if (ui_font_xl || !ttf_data || ttf_size == 0) return; // déjà fait / pas de données
    ui_font_xl = lv_tiny_ttf_create_data(ttf_data, ttf_size, UI_FONT_XL_PX);
    ui_font_l = lv_tiny_ttf_create_data(ttf_data, ttf_size, UI_FONT_L_PX);
    ui_font_m = lv_tiny_ttf_create_data(ttf_data, ttf_size, UI_FONT_M_PX);
}
