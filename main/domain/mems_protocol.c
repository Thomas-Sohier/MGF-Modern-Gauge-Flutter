#include "domain/mems_protocol.h"

#include <math.h>

// Décalages des trames, calqués sur les structs Go (`structures.go`). L'octet
// [0] est l'écho de commande, [1] la taille utile (BytesinFrame).

// --- Trame 0x80 (offsets absolus dans le tampon reçu) -----------------------
#define F80_CMD          0
#define F80_SIZE         1
#define F80_RPM_HI       2
#define F80_RPM_LO       3
#define F80_COOLANT      4
#define F80_AMBIENT      5
#define F80_INTAKE_AIR   6
#define F80_FUEL         7
#define F80_MAP          8
#define F80_BATTERY      9
#define F80_THROTTLE_POT 10
#define F80_IDLE_SWITCH  11
#define F80_PARK_NEUTRAL 13
#define F80_DTC0         14
#define F80_DTC1         15
#define F80_IDLE_BASE    17
#define F80_IAC_POS      19
#define F80_IDLE_ERR_HI  20
#define F80_IDLE_ERR_LO  21
#define F80_IGN_ADVANCE  23
#define F80_COIL_HI      24
#define F80_COIL_LO      25

// --- Trame 0x7D -------------------------------------------------------------
#define F7D_CMD             0
#define F7D_SIZE            1
#define F7D_IGN_SWITCH      2
#define F7D_THROTTLE        3
#define F7D_AFR             5
#define F7D_LAMBDA_V        7
#define F7D_LOOP            11
#define F7D_LONG_TERM_TRIM  12
#define F7D_SHORT_TERM_TRIM 13
#define F7D_IGN_ADV_OFFSET  21

// --- Bits DTC (structures.go) -----------------------------------------------
#define DTC0_COOLANT_SENSOR 0x01u
#define DTC0_AIR_SENSOR     0x02u
#define DTC1_FUEL_PUMP      0x02u
#define DTC1_THROTTLE_POT   0x40u
#define IDLE_SWITCH_ACTIVE  0x08u

// Arrondi entier au plus proche pour un rationnel positif ou négatif.
static int round_div(int num, int den) {
    if (num >= 0) return (num + den / 2) / den;
    return -(((-num) + den / 2) / den);
}

// Température brute (offset -55 °C) ; 0xFF signale un capteur non supporté.
static float decode_temp(uint8_t raw) {
    if (raw == MEMS_TEMP_UNSUPPORTED_RAW) return NAN;
    return (float)((int)raw - 55);
}

bool mems_parse_frame_80(const uint8_t *frame, size_t len, mems_data_t *out) {
    if (frame == NULL || out == NULL) return false;
    if (len != MEMS_FRAME_80_LEN) return false;
    if (frame[F80_CMD] != MEMS_CMD_DATA_80) return false;
    if (frame[F80_SIZE] != MEMS_FRAME_80_SIZE_BYTE) return false;
    const int rpm = (frame[F80_RPM_HI] << 8) | frame[F80_RPM_LO];
    if (rpm > MEMS_RPM_MAX_PLAUSIBLE) return false;

    out->engine_rpm = rpm;
    out->coolant_temp = decode_temp(frame[F80_COOLANT]);
    out->ambient_temp = decode_temp(frame[F80_AMBIENT]);
    out->intake_air_temp = decode_temp(frame[F80_INTAKE_AIR]);
    out->fuel_temp = decode_temp(frame[F80_FUEL]);
    out->map_kpa = (float)frame[F80_MAP];
    out->battery_voltage = (float)frame[F80_BATTERY] / 10.0f;
    out->throttle_pot_voltage = (float)frame[F80_THROTTLE_POT] * 0.02f;
    out->idle_switch = (frame[F80_IDLE_SWITCH] & IDLE_SWITCH_ACTIVE) != 0;
    out->park_neutral_switch = frame[F80_PARK_NEUTRAL] != 0;
    out->dtc0 = frame[F80_DTC0];
    out->dtc1 = frame[F80_DTC1];
    out->iac_position = frame[F80_IAC_POS];
    out->idle_base_position = frame[F80_IDLE_BASE];
    out->idle_error =
        (frame[F80_IDLE_ERR_HI] == 0xFF && frame[F80_IDLE_ERR_LO] == 0xFF)
            ? NAN
            : (float)((frame[F80_IDLE_ERR_HI] << 8) | frame[F80_IDLE_ERR_LO]);
    out->ignition_advance = (float)frame[F80_IGN_ADVANCE] / 2.0f - 24.0f;
    out->coil_time =
        (float)((frame[F80_COIL_HI] << 8) | frame[F80_COIL_LO]) * 0.002f;

    out->coolant_sensor_fault = (out->dtc0 & DTC0_COOLANT_SENSOR) != 0;
    out->intake_air_sensor_fault = (out->dtc0 & DTC0_AIR_SENSOR) != 0;
    out->fuel_pump_fault = (out->dtc1 & DTC1_FUEL_PUMP) != 0;
    out->throttle_pot_fault = (out->dtc1 & DTC1_THROTTLE_POT) != 0;
    return true;
}

bool mems_parse_frame_7d(const uint8_t *frame, size_t len, mems_data_t *out) {
    if (frame == NULL || out == NULL) return false;
    if (len != MEMS_FRAME_7D_LEN) return false;
    if (frame[F7D_CMD] != MEMS_CMD_DATA_7D) return false;
    if (frame[F7D_SIZE] != MEMS_FRAME_7D_SIZE_BYTE) return false;

    out->ignition_switch = frame[F7D_IGN_SWITCH] != 0;
    out->throttle_angle = round_div((int)frame[F7D_THROTTLE] * 6, 10);
    out->air_fuel_ratio = (float)frame[F7D_AFR] / 10.0f;
    out->lambda_voltage_mv = (int)frame[F7D_LAMBDA_V] * 5;
    out->closed_loop = frame[F7D_LOOP] != 0;

    // Correction lambda : 0xFF = non supporté / non appris.
    const uint8_t stft = frame[F7D_SHORT_TERM_TRIM];
    out->fuelling_feedback_percent = stft == 0xFF ? NAN : (float)stft;
    out->short_term_trim_percent =
        stft == 0xFF ? NAN : (float)((int)stft - 100);
    const uint8_t ltft = frame[F7D_LONG_TERM_TRIM];
    out->long_term_trim_percent = ltft == 0xFF ? NAN : (float)((int)ltft - 128);
    const uint8_t adv = frame[F7D_IGN_ADV_OFFSET];
    out->ignition_advance_offset =
        adv == 0xFF ? NAN : (float)((int)adv - 128) * 0.5f;

    out->has_7d_data = true;
    return true;
}
