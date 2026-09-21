#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "domain/ecu_data.h"

// Protocole Rover MEMS (ROSCO) — couche PURE, sans aucune E/S.
//
// Portage C de la partie décodage du projet Go de référence
// `andrewdjackson/rosco` (paquet `rosco`, ex-`readmems`). On y reprend
// verbatim :
//   - la séquence d'init et les octets de commande (`commands.go`) ;
//   - la disposition octet par octet des trames 0x80 / 0x7D (`structures.go`) ;
//   - les conversions appliquées (`rosco.go::createMemsDataframe`).
//
// La liaison physique est une K-line ISO-9141 : UART **9600 bps, 8N1**,
// half-duplex (l'ECU ré-émet — « echo » — chaque octet de commande avant sa
// réponse). Cette couche ne connaît pas la K-line : elle ne fait que décoder des
// tampons d'octets déjà reçus. Le dialogue est dans `mems_session.[ch]`.

// --- Octets de commande (commands.go) ---------------------------------------
#define MEMS_CMD_INIT_A   0xCAu  // 1er octet d'init « normale »
#define MEMS_CMD_INIT_B   0x75u  // 2e octet d'init
#define MEMS_CMD_HEARTBEAT 0xF4u // heartbeat / bascule mode diag 5
#define MEMS_CMD_INIT_ECU_ID 0xD0u // requête ID ECU (dernier pas d'init)
#define MEMS_CMD_DATA_80  0x80u  // requête trame 0x80 (RPM, temps, batterie…)
#define MEMS_CMD_DATA_7D  0x7Du  // requête trame 0x7D (papillon, lambda…)

// Adresse ECU émise lors du réveil 5 bauds (slow init) de MEMS 1.9.
#define MEMS_ECU_ADDRESS  0x16u

// --- Tailles de réponse attendues, écho de commande inclus (ecureader.go) ---
// (l'ECU renvoie [écho commande] + [données]).
#define MEMS_RESP_INIT_A    1u   // CA
#define MEMS_RESP_INIT_B    1u   // 75
#define MEMS_RESP_HEARTBEAT 2u   // F4 00
#define MEMS_RESP_ECU_ID    5u   // D0 99 00 xx 03
#define MEMS_FRAME_80_LEN   29u  // 80 1C + 27 octets de données
#define MEMS_FRAME_7D_LEN   33u  // 7D 20 + 31 octets de données

// Instantané « riche » décodé depuis les deux trames MEMS. On garde les champs
// utiles au tableau de bord et au diagnostic ; les nombreux octets inconnus du
// protocole ne sont pas exposés. Valeurs déjà converties en unités physiques.
typedef struct {
    // Trame 0x80
    int   engine_rpm;            // tr/min (big-endian 16 bits)
    int   coolant_temp;          // °C (brut - 55)
    int   ambient_temp;          // °C (brut - 55)
    int   intake_air_temp;       // °C (brut - 55)
    int   fuel_temp;             // °C (brut - 55 ; 0xFF => non supporté)
    float map_kpa;               // pression collecteur (kPa)
    float battery_voltage;       // V (brut / 10)
    float throttle_pot_voltage;  // V (brut * 0.02)
    bool  idle_switch;           // papillon fermé (ralenti)
    bool  park_neutral_switch;   // point mort / parking
    uint8_t dtc0;                // codes défaut (bitfield) trame 0x80
    uint8_t dtc1;
    int   iac_position;          // position moteur pas-à-pas ralenti (0..180)
    float ignition_advance;      // ° (brut / 2 - 24)
    float coil_time;             // ms (brut * 0.002)

    // Trame 0x7D
    bool  ignition_switch;
    int   throttle_angle;        // ° (round(brut * 6 / 10))
    float air_fuel_ratio;        // (brut / 10)
    int   lambda_voltage_mv;     // mV (brut * 5)
    bool  closed_loop;           // régulation lambda en boucle fermée

    // Défauts dérivés des bits DTC (rosco.go)
    bool  coolant_sensor_fault;
    bool  intake_air_sensor_fault;
    bool  fuel_pump_fault;
    bool  throttle_pot_fault;
} mems_data_t;

// Décode la trame 0x80 (RPM/températures/batterie…). `frame` doit contenir
// exactement MEMS_FRAME_80_LEN octets, écho de commande 0x80 inclus en [0].
// Renvoie false si la taille est mauvaise ou l'écho incorrect.
bool mems_parse_frame_80(const uint8_t *frame, size_t len, mems_data_t *out);

// Décode la trame 0x7D (papillon/lambda…). `frame` doit contenir exactement
// MEMS_FRAME_7D_LEN octets, écho 0x7D inclus en [0]. Complète `out` (les champs
// 0x80 sont laissés intacts ; appeler _80 puis _7D sur le même `out`).
bool mems_parse_frame_7d(const uint8_t *frame, size_t len, mems_data_t *out);

// Projette l'instantané MEMS vers le modèle consommé par l'écran.
//
// Correspondances : rpm/coolant_temp/battery_voltage directs ; throttle estimé
// depuis la tension du potentiomètre papillon (0.6 V fermé .. 4.6 V plein gaz,
// approx.) ; `connected` fixé par l'appelant selon l'état de la liaison.
//
// ⚠️ MEMS 1.6 ne dispose PAS de capteur de température d'huile : `oil_temp` est
// mis à 0 et devra être alimenté par une autre source si un jour disponible.
void mems_to_ecu_data(const mems_data_t *in, bool connected, ecu_data_t *out);
