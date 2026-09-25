#include "ui/screens/music_screen.h"

#include "ui/themes/ui_theme.h"
#include "ui/ui_layout.h"
#include "ui/widgets/amber_draw.h"
#include "ui/widgets/amber_kit.h"

#include <math.h>
#include <stdio.h>

// Lecture en cours sur le téléphone (application compagnon). Un anneau
// continu fait le tour du cadran et porte la progression du morceau (départ
// à 12 h, sens horaire) ; le centre porte titre, artiste, temps et l'état de
// lecture. Le lien BLE est à sens unique (téléphone -> jauge) : le
// pictogramme central est un indicateur, pas une commande.
#define RING_R_OUT   157.0f
#define RING_WIDTH   8.0f
#define TRACK_Y      92.0f
#define ARTIST_Y     119.0f
#define RULE_Y       137.0f
#define TIME_Y       156.0f
#define STATE_Y      212.0f
#define TRACK_WIDTH  250.0f
#define ARTIST_WIDTH 230.0f

struct music_screen_s {
    amber_page_t page;
    lv_obj_t *track;
    lv_obj_t *artist;
    lv_obj_t *position;
    lv_obj_t *slash;
    lv_obj_t *duration;
    companion_media_t media;
    uint32_t received_ms;
    bool has_media;
    bool linked;
    float progress;        // 0..1, ou < 0 si inconnue
    int64_t shown_seconds; // dernière position affichée
};

static void draw_state(lv_layer_t *layer, const ui_layout_t *layout,
                       void *context) {
    const music_screen_t *scr = context;
    if (scr == NULL) return;
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();
    const lv_color_t sep = ui_theme_amber_separator();

    amber_draw_circle(layer, layout, AMBER_KIT_CX, AMBER_KIT_CX, RING_R_OUT,
                      RING_WIDTH, dim);
    if (scr->progress > 0.0f) {
        amber_draw_arc_wrapped(layer, layout, AMBER_KIT_CX, AMBER_KIT_CX,
                               RING_R_OUT, RING_WIDTH, 270.0f,
                               360.0f * scr->progress, bright, false);
    }

    amber_draw_line(layer, layout, 112.0f, RULE_Y, 208.0f, RULE_Y, 1.0f, sep,
                    false);

    const bool active = scr->has_media && scr->linked;
    const companion_playback_t state =
        active ? scr->media.state : COMPANION_PLAYBACK_STOPPED;
    const lv_color_t glyph = active ? bright : dim;
    if (state == COMPANION_PLAYBACK_PLAYING) {
        // Lecture : triangle.
        for (float y = -10.0f; y <= 10.0f; y += 1.0f) {
            const float x_end = 156.0f + 16.0f * (1.0f - fabsf(y) / 10.0f);
            amber_draw_line(layer, layout, 155.0f, STATE_Y + y, x_end,
                            STATE_Y + y, 1.2f, glyph, false);
        }
    } else if (state == COMPANION_PLAYBACK_PAUSED) {
        amber_draw_line(layer, layout, 154.0f, STATE_Y - 9.0f, 154.0f,
                        STATE_Y + 9.0f, 4.0f, glyph, false);
        amber_draw_line(layer, layout, 166.0f, STATE_Y - 9.0f, 166.0f,
                        STATE_Y + 9.0f, 4.0f, glyph, false);
    } else {
        // Arrêt ou aucun média : carré.
        for (float y = -8.0f; y <= 8.0f; y += 1.0f) {
            amber_draw_line(layer, layout, 152.0f, STATE_Y + y, 168.0f,
                            STATE_Y + y, 1.2f, glyph, false);
        }
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

static void place_times(music_screen_t *scr) {
    amber_kit_place(scr->slash, AMBER_KIT_CX, TIME_Y, AMBER_ALIGN_CENTER);
    amber_kit_place(scr->position, AMBER_KIT_CX - 9.0f, TIME_Y,
                    AMBER_ALIGN_RIGHT);
    amber_kit_place(scr->duration, AMBER_KIT_CX + 9.0f, TIME_Y,
                    AMBER_ALIGN_LEFT);
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

    char text[16];
    format_time(text, sizeof(text), position);
    bool changed = amber_kit_set_text(scr->position, text);
    format_time(text, sizeof(text), duration);
    changed |= amber_kit_set_text(scr->duration, text);
    if (changed) place_times(scr);

    if (seconds != scr->shown_seconds || progress != scr->progress) {
        scr->shown_seconds = seconds;
        scr->progress = progress;
        amber_page_invalidate(&scr->page);
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

    const char *status = "";
    bool emphasized = false;
    if (scr->linked && scr->has_media) {
        switch (scr->media.state) {
        case COMPANION_PLAYBACK_PLAYING:
            status = "LECTURE";
            emphasized = true;
            break;
        case COMPANION_PLAYBACK_PAUSED:
            status = "PAUSE";
            break;
        default:
            status = "ARRET";
            break;
        }
    }
    amber_page_set_status(&scr->page, status, emphasized);
}

music_screen_t *music_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;
    music_screen_t *scr = lv_malloc(sizeof(*scr));
    if (scr == NULL) return NULL;
    lv_memzero(scr, sizeof(*scr));
    scr->progress = -1.0f;
    scr->shown_seconds = -2;

    const amber_page_spec_t spec = {
        .title = "MUSIQUE",
        .rim = AMBER_RIM_NONE,
        .no_grid = true,
        .draw_extra = draw_state,
        .context = scr,
    };
    if (!amber_page_create(&scr->page, parent, &spec)) goto fail;
    // Le gabarit fournit une valeur héros inutile ici : on la masque.
    lv_obj_add_flag(scr->page.hero.value, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *root = scr->page.root;
    const ui_layout_t layout =
        ui_layout_fit(lv_obj_get_width(root), lv_obj_get_height(root));
    scr->track = amber_kit_label(root, amber_kit_font_value(),
                                 ui_theme_amber_bright(), "");
    scr->artist = amber_kit_caption(root, "");
    scr->position = amber_kit_label(root, amber_kit_font_label(),
                                    ui_theme_amber_bright(), "--:--");
    scr->slash = amber_kit_label(root, amber_kit_font_label(),
                                 ui_theme_amber_dim(), "/");
    scr->duration = amber_kit_label(root, amber_kit_font_label(),
                                    ui_theme_amber_separator(), "--:--");
    if (scr->track == NULL || scr->artist == NULL || scr->position == NULL ||
        scr->slash == NULL || scr->duration == NULL) {
        goto fail;
    }
    lv_obj_set_width(scr->track, (int32_t)lroundf(TRACK_WIDTH * layout.scale));
    lv_label_set_long_mode(scr->track, LV_LABEL_LONG_DOT);
    lv_obj_set_width(scr->artist,
                     (int32_t)lroundf(ARTIST_WIDTH * layout.scale));
    lv_label_set_long_mode(scr->artist, LV_LABEL_LONG_DOT);
    place_times(scr);
    refresh_texts(scr);
    return scr;

fail:
    music_screen_destroy(scr);
    return NULL;
}

void music_screen_set_media(music_screen_t *scr, const companion_media_t *media,
                            uint32_t now_ms) {
    if (scr == NULL) return;
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
    lv_free(scr);
}
