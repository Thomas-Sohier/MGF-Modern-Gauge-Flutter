#include "ui/screens/music_screen.h"

#include <string.h>

#include "domain/music_cover.h"
#include "ui/screens/music_cover_fit.h"
#include "src/misc/cache/lv_image_cache.h"
#include "ui/themes/ui_theme.h"
#include "ui/ui_layout.h"
#include "ui/widgets/amber_draw.h"
#include "ui/widgets/amber_kit.h"
#include "ui/widgets/amber_ui.h"

#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Lecture en cours sur le téléphone (application compagnon). Le lien BLE est à
// sens unique (téléphone -> jauge) : la pochette JPEG est réassemblée puis
// décodée hors LVGL (companion_art_worker) et déposée ici en RGB565 ambre. En
// l'absence d'image valide, un pictogramme musical sombre sur fond ambre sert
// de repli.
//
// Composition : anneau de progression continu proche du bord (piste sombre,
// départ à 12 h, sens horaire), pochette en cadrage « cover » (échelle
// uniforme, rognage centré) sur toute la largeur intérieure de l'anneau et
// jusqu'au titre, découpée par l'arc intérieur de l'anneau, puis
// titre, artiste et durée totale. Aucun header, bouton ni pictogramme de
// transport : l'état figé (pause/arrêt) se lit uniquement au fait que l'anneau
// cesse d'avancer.
// Géométrie (repère 320) et cadrage « cover » : music_cover_fit.h. La
// pochette remplit toute la largeur intérieure de l'anneau jusqu'au titre ;
// son parent est un disque tangent au bord intérieur de l'anneau, qui découpe
// les côtés de l'image selon le même arc que la progression.
#define RING_R         MUSIC_RING_R
#define RING_W         MUSIC_RING_W
#define COVER_CLIP_XY  MUSIC_COVER_CLIP_XY
#define COVER_CLIP_D   MUSIC_COVER_CLIP_D
#define COVER_IMAGE_Y1 MUSIC_COVER_IMAGE_Y1
#define TRACK_Y        MUSIC_TRACK_Y
#define ARTIST_Y       MUSIC_ARTIST_Y
#define RULE_Y         MUSIC_RULE_Y
#define TIME_Y         MUSIC_TIME_Y
#define TRACK_WIDTH    MUSIC_TRACK_WIDTH
#define ARTIST_WIDTH   MUSIC_ARTIST_WIDTH

struct music_screen_s {
    amber_page_t page;
    lv_obj_t *cover;          // disque qui découpe les côtés selon l'anneau
    lv_obj_t *cover_viewport; // coupe horizontale avant le titre
    lv_obj_t *cover_img;      // image RGB565 ambre
    lv_obj_t *ring;           // calque de l'anneau, au-dessus de la pochette
    lv_image_dsc_t cover_dsc;
    uint16_t *cover_pixels; // possédé par l'écran
    int cover_width;
    int cover_height;
    lv_obj_t *track;
    lv_obj_t *artist;
    lv_obj_t *duration;
    companion_media_t media;
    uint32_t received_ms;
    bool has_media;
    bool linked;
    float progress;        // 0..1, ou < 0 si inconnue
    int64_t shown_seconds; // dernière position affichée
};

// ── Pochette ────────────────────────────────────────────────────────────────
// Repli sans pochette : aplat ambre et pictogramme musical couleur fond. Le
// remplissage par cordes reproduit le même bord circulaire que l'image réelle.
static void draw_cover_fallback(lv_layer_t *layer, const ui_layout_t *layout) {
    const lv_color_t amber = ui_theme_amber_bright();
    const lv_color_t bg = ui_theme_amber_bg();
    const float radius = COVER_CLIP_D * 0.5f;
    const float cy = COVER_CLIP_XY + radius;
    for (float y = COVER_CLIP_XY; y <= COVER_IMAGE_Y1; y += 0.75f) {
        const float dy = y - cy;
        const float half = sqrtf(LV_MAX(0.0f, radius * radius - dy * dy));
        amber_draw_line(layer, layout, AMBER_KIT_CX - half, y,
                        AMBER_KIT_CX + half, y, 1.25f, amber, false);
    }

    // Double croche stylisée, suffisamment massive pour rester lisible sur le
    // fond tramé de l'écran physique.
    amber_draw_line(layer, layout, 140.0f, 88.0f, 140.0f, 157.0f, 8.0f, bg,
                    false);
    amber_draw_line(layer, layout, 178.0f, 78.0f, 178.0f, 147.0f, 8.0f, bg,
                    false);
    amber_draw_line(layer, layout, 140.0f, 88.0f, 178.0f, 78.0f, 9.0f, bg,
                    false);
    amber_draw_line(layer, layout, 140.0f, 105.0f, 178.0f, 95.0f, 7.0f, bg,
                    false);
    amber_draw_dot(layer, layout, 129.0f, 159.0f, 14.0f, bg);
    amber_draw_dot(layer, layout, 167.0f, 149.0f, 14.0f, bg);
}

static void draw_state(lv_layer_t *layer, const ui_layout_t *layout,
                       void *context) {
    music_screen_t *scr = context;
    if (scr == NULL) return;

    // Remplissage de repli sous l'objet image : le canvas est le seul à le
    // dessiner quand aucune pochette réelle n'est prête.
    if (scr->cover_pixels == NULL) {
        draw_cover_fallback(layer, layout);
    }

    amber_draw_line(layer, layout, 112.0f, RULE_Y, 208.0f, RULE_Y, 1.0f,
                    ui_theme_amber_separator(), false);
}

// L'anneau est dessiné sur un calque dédié, au-dessus de la pochette (il borde
// l'image qui occupe toute la largeur) mais sous les textes.
static void ring_draw_cb(lv_event_t *event) {
    lv_obj_t *canvas = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    music_screen_t *scr = lv_obj_get_user_data(canvas);
    if (scr == NULL || layer == NULL) return;

    lv_area_t area;
    lv_obj_get_coords(canvas, &area);
    const ui_layout_t layout = amber_draw_layout(&area);
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();

    // Anneau continu : piste complète sombre, progression éventuelle en ambre
    // vif depuis 12 h (angle 270°), dans le sens horaire.
    amber_draw_circle(layer, &layout, AMBER_KIT_CX, AMBER_KIT_CX, RING_R,
                      RING_W, dim);
    if (scr->progress > 0.0f) {
        amber_draw_arc_wrapped(layer, &layout, AMBER_KIT_CX, AMBER_KIT_CX,
                               RING_R, RING_W, 270.0f, 360.0f * scr->progress,
                               bright, false);
    }
}

static void format_time(char *out, size_t size, int64_t ms) {
    if (ms < 0) {
        snprintf(out, size, "--:--");
        return;
    }
    const int64_t total = ms / 1000;
    const int64_t hours = total / 3600;
    if (hours > 0) {
        snprintf(out, size, "%d:%02d:%02d", (int)hours,
                 (int)((total / 60) % 60), (int)(total % 60));
    } else {
        snprintf(out, size, "%02d:%02d", (int)(total / 60), (int)(total % 60));
    }
}

static void place_duration(music_screen_t *scr) {
    amber_kit_place(scr->duration, AMBER_KIT_CX, TIME_Y, AMBER_ALIGN_CENTER);
}

// Recalcule position, durée et progression ; n'invalide que si l'affichage
// change (une fois par seconde en lecture).
static void refresh_progress(music_screen_t *scr, uint32_t now_ms) {
    const bool active = scr->has_media && scr->linked;
    const int64_t position =
        active
            ? companion_media_position_at(&scr->media, scr->received_ms, now_ms)
            : -1;
    const int64_t duration =
        active && scr->media.duration_ms > 0 ? scr->media.duration_ms : -1;
    const int64_t seconds = position < 0 ? -1 : position / 1000;
    // Quantifiée à la seconde : un seul redessin par seconde en lecture.
    const float progress =
        seconds >= 0 && duration > 0
            ? (float)((double)(seconds * 1000) / (double)duration)
            : -1.0f;

    // La position courante est portée uniquement par l'anneau. Le texte en
    // bas indique donc seulement la durée totale du morceau.
    char text[16];
    format_time(text, sizeof(text), duration);
    if (amber_kit_set_text(scr->duration, text)) place_duration(scr);

    if (seconds != scr->shown_seconds || progress != scr->progress) {
        scr->shown_seconds = seconds;
        scr->progress = progress;
        amber_page_invalidate(&scr->page);
        if (scr->ring != NULL) lv_obj_invalidate(scr->ring);
    }
}

static void refresh_texts(music_screen_t *scr) {
    const bool active = scr->has_media && scr->linked;
    const char *title = !scr->linked                  ? "TELEPHONE"
                        : !scr->has_media             ? "AUCUN MEDIA"
                        : scr->media.title[0] != '\0' ? scr->media.title
                                                      : "SANS TITRE";
    if (amber_kit_set_text(scr->track, title)) {
        amber_kit_place(scr->track, AMBER_KIT_CX, TRACK_Y, AMBER_ALIGN_CENTER);
    }
    lv_obj_set_style_text_color(
        scr->track,
        active ? ui_theme_amber_bright() : ui_theme_amber_separator(), 0);

    const char *artist = !scr->linked ? "NON CONNECTE"
                         : active     ? scr->media.artist
                                      : "";
    if (amber_kit_set_text(scr->artist, artist)) {
        amber_kit_place(scr->artist, AMBER_KIT_CX, ARTIST_Y,
                        AMBER_ALIGN_CENTER);
    }
}

void music_screen_set_cover(music_screen_t *scr, uint16_t *pixels, int width,
                            int height) {
    if (scr == NULL) return;
    // Le descripteur garde la même adresse : on purge l'entrée de cache qui
    // pourrait référencer l'ancien tampon avant de le libérer.
    lv_image_cache_drop(&scr->cover_dsc);
    if (scr->cover_pixels != NULL) lv_free(scr->cover_pixels);
    scr->cover_pixels = NULL;
    scr->cover_width = 0;
    scr->cover_height = 0;

    if (pixels != NULL && width > 0 && height > 0 && scr->cover != NULL &&
        scr->cover_viewport != NULL && scr->cover_img != NULL) {
        scr->cover_pixels = pixels;
        scr->cover_width = width;
        scr->cover_height = height;
        scr->cover_dsc = (lv_image_dsc_t){
            .header =
                {
                    .magic = LV_IMAGE_HEADER_MAGIC,
                    .cf = LV_COLOR_FORMAT_RGB565,
                    .w = (uint32_t)width,
                    .h = (uint32_t)height,
                    .stride = (uint32_t)width * 2U,
                },
            .data_size = (uint32_t)width * (uint32_t)height * 2U,
            .data = (const uint8_t *)pixels,
        };
        // Cadrage « cover » : l'objet image a la taille exacte de la fenêtre
        // et centre le bitmap ; une échelle uniforme (pivot au centre) le
        // grossit jusqu'à couvrir largeur ET hauteur. Le dépassement est
        // rogné symétriquement par la fenêtre, sans déformation, quelles que
        // soient les proportions de la source.
        const int32_t box_w = lv_obj_get_width(scr->cover_viewport);
        const int32_t box_h = lv_obj_get_height(scr->cover_viewport);
        music_cover_fit_t fit;
        if (!music_cover_fit(width, height, box_w, box_h, &fit)) {
            fit.scale = LV_SCALE_NONE;
        }
        lv_image_set_src(scr->cover_img, &scr->cover_dsc);
        lv_obj_set_size(scr->cover_img, box_w, box_h);
        lv_image_set_inner_align(scr->cover_img, LV_IMAGE_ALIGN_CENTER);
        lv_image_set_pivot(scr->cover_img, width / 2, height / 2);
        lv_image_set_scale(scr->cover_img, fit.scale);
        lv_obj_center(scr->cover_img);
        lv_obj_remove_flag(scr->cover_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(scr->cover, LV_OBJ_FLAG_HIDDEN);
    } else {
        if (pixels != NULL) lv_free(pixels);
        if (scr->cover_img != NULL) {
            lv_obj_add_flag(scr->cover_img, LV_OBJ_FLAG_HIDDEN);
        }
        if (scr->cover != NULL) {
            lv_obj_add_flag(scr->cover, LV_OBJ_FLAG_HIDDEN);
        }
    }
    amber_page_invalidate(&scr->page);
}

void music_screen_palette_changed(music_screen_t *scr, bool from_inverted,
                                  bool to_inverted) {
    if (scr == NULL || from_inverted == to_inverted) return;
    if (scr->cover_pixels != NULL) {
        music_cover_convert_palette(scr->cover_pixels,
                                    (size_t)scr->cover_width *
                                        (size_t)scr->cover_height,
                                    from_inverted, to_inverted);
        lv_image_cache_drop(&scr->cover_dsc);
        lv_obj_invalidate(scr->cover_img);
    }
}

music_screen_t *music_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;
    music_screen_t *scr = lv_malloc(sizeof(*scr));
    if (scr == NULL) return NULL;
    lv_memzero(scr, sizeof(*scr));
    scr->progress = -1.0f;
    scr->shown_seconds = -2;

    const amber_page_spec_t spec = {
        .title = "",
        .rim = AMBER_RIM_NONE,
        .no_grid = true,
        .draw_extra = draw_state,
        .context = scr,
    };
    if (!amber_page_create(&scr->page, parent, &spec)) goto fail;
    // Le gabarit fournit une valeur héros inutile ici : on la masque.
    lv_obj_add_flag(scr->page.hero.value, LV_OBJ_FLAG_HIDDEN);

    // Disque de clipping tangent au bord intérieur de l'anneau. Le bitmap
    // n'occupe que sa partie haute, mais ses côtés sont ainsi découpés selon
    // l'arc du cadran. Non cliquable pour préserver les gestes du navigateur.
    lv_obj_t *root = scr->page.root;
    const ui_layout_t layout =
        ui_layout_fit(lv_obj_get_width(root), lv_obj_get_height(root));
    const int32_t cover_d = (int32_t)lroundf(COVER_CLIP_D * layout.scale);
    scr->cover = lv_obj_create(root);
    if (scr->cover == NULL) goto fail;
    lv_obj_remove_style_all(scr->cover);
    lv_obj_set_size(scr->cover, cover_d, cover_d);
    lv_obj_set_pos(scr->cover,
                   (int32_t)lroundf(ui_layout_x(&layout, COVER_CLIP_XY)),
                   (int32_t)lroundf(ui_layout_y(&layout, COVER_CLIP_XY)));
    lv_obj_set_style_radius(scr->cover, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(scr->cover, true, 0);
    // Fond transparent : sans image, le motif vectoriel du canvas reste visible.
    lv_obj_set_style_bg_opa(scr->cover, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(scr->cover,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    // Un second clipping, rectangulaire, arrête l'image juste avant le titre.
    // Le parent circulaire continue simultanément à découper ses côtés.
    scr->cover_viewport = lv_obj_create(scr->cover);
    if (scr->cover_viewport == NULL) goto fail;
    lv_obj_remove_style_all(scr->cover_viewport);
    lv_obj_set_size(
        scr->cover_viewport, cover_d,
        (int32_t)lroundf((COVER_IMAGE_Y1 - COVER_CLIP_XY) * layout.scale));
    lv_obj_align(scr->cover_viewport, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_radius(scr->cover_viewport, 0, 0);
    lv_obj_set_style_clip_corner(scr->cover_viewport, true, 0);
    lv_obj_set_style_bg_opa(scr->cover_viewport, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(scr->cover_viewport,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    scr->cover_img = lv_image_create(scr->cover_viewport);
    if (scr->cover_img == NULL) goto fail;
    lv_obj_remove_style_all(scr->cover_img);
    lv_obj_clear_flag(scr->cover_img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(scr->cover_img);
    lv_obj_add_flag(scr->cover_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(scr->cover, LV_OBJ_FLAG_HIDDEN);

    // Calque de l'anneau, créé après la pochette (pour border l'image) mais
    // avant les textes (pour rester dessous).
    scr->ring = amber_ui_canvas_create(root, scr, ring_draw_cb);
    if (scr->ring == NULL) goto fail;

    scr->track = amber_kit_label(root, amber_kit_font_value(),
                                 ui_theme_amber_bright(), "");
    scr->artist = amber_kit_caption(root, "");
    scr->duration = amber_kit_label(root, amber_kit_font_label(),
                                    ui_theme_amber_separator(), "--:--");
    if (scr->track == NULL || scr->artist == NULL || scr->duration == NULL) {
        goto fail;
    }
    lv_obj_set_width(scr->track, (int32_t)lroundf(TRACK_WIDTH * layout.scale));
    lv_label_set_long_mode(scr->track, LV_LABEL_LONG_DOT);
    lv_obj_set_width(scr->artist,
                     (int32_t)lroundf(ARTIST_WIDTH * layout.scale));
    lv_label_set_long_mode(scr->artist, LV_LABEL_LONG_DOT);
    place_duration(scr);
    refresh_texts(scr);
    return scr;

fail:
    music_screen_destroy(scr);
    return NULL;
}

void music_screen_set_media(music_screen_t *scr, const companion_media_t *media,
                            uint32_t now_ms) {
    if (scr == NULL) return;
    // Une nouvelle pochette compagnon n'est envoyée que lorsque l'art_id
    // change ; l'ancienne image est donc abandonnée ici et remplacée par le
    // worker si un transfert suit. Même art_id (réémission de position) ou
    // déconnexion (media == NULL) conservent l'image affichée.
    if (media != NULL && strcmp(scr->media.art_id, media->art_id) != 0) {
        music_screen_set_cover(scr, NULL, 0, 0);
    }
    scr->has_media = media != NULL;
    if (media != NULL) {
        scr->media = *media;
        scr->received_ms = now_ms;
    }
    refresh_texts(scr);
    refresh_progress(scr, now_ms);
    amber_page_invalidate(&scr->page);
}

void music_screen_set_link(music_screen_t *scr, bool linked, uint32_t now_ms) {
    if (scr == NULL || scr->linked == linked) return;
    scr->linked = linked;
    refresh_texts(scr);
    refresh_progress(scr, now_ms);
    amber_page_invalidate(&scr->page);
}

void music_screen_tick(music_screen_t *scr, uint32_t now_ms) {
    if (scr == NULL) return;
    refresh_progress(scr, now_ms);
}

void music_screen_destroy(music_screen_t *scr) {
    if (scr == NULL) return;
    amber_page_destroy(&scr->page);
    if (scr->cover_pixels != NULL) lv_free(scr->cover_pixels);
    lv_free(scr);
}
