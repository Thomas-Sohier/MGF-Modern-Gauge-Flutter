#include "domain/mems_protocol.h"

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
#define F80_IAC_POS      19
#define F80_IGN_ADVANCE  23
#define F80_COIL_HI      24
#define F80_COIL_LO      25

// --- Trame 0x7D -------------------------------------------------------------
#define F7D_CMD          0
#define F7D_IGN_SWITCH   2
#define F7D_THROTTLE     3
#define F7D_AFR          5
#define F7D_LAMBDA_V     7
#define F7D_LOOP         11

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

bool mems_parse_frame_80(const uint8_t *frame, size_t len, mems_data_t *out) {
    if (frame == NULL || out == NULL) return false;
    if (len != MEMS_FRAME_80_LEN) return false;
    if (frame[F80_CMD] != MEMS_CMD_DATA_80) return false;

    out->engine_rpm = (frame[F80_RPM_HI] << 8) | frame[F80_RPM_LO];
    out->coolant_temp = (int)frame[F80_COOLANT] - 55;
    out->ambient_temp = (int)frame[F80_AMBIENT] - 55;
    out->intake_air_temp = (int)frame[F80_INTAKE_AIR] - 55;
    out->fuel_temp = (int)frame[F80_FUEL] - 55;
    out->map_kpa = (float)frame[F80_MAP];
    out->battery_voltage = (float)frame[F80_BATTERY] / 10.0f;
    out->throttle_pot_voltage = (float)frame[F80_THROTTLE_POT] * 0.02f;
    out->idle_switch = (frame[F80_IDLE_SWITCH] & IDLE_SWITCH_ACTIVE) != 0;
    out->park_neutral_switch = frame[F80_PARK_NEUTRAL] != 0;
    out->dtc0 = frame[F80_DTC0];
    out->dtc1 = frame[F80_DTC1];
    out->iac_position = frame[F80_IAC_POS];
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

    out->ignition_switch = frame[F7D_IGN_SWITCH] != 0;
    out->throttle_angle = round_div((int)frame[F7D_THROTTLE] * 6, 10);
    out->air_fuel_ratio = (float)frame[F7D_AFR] / 10.0f;
    out->lambda_voltage_mv = (int)frame[F7D_LAMBDA_V] * 5;
    out->closed_loop = frame[F7D_LOOP] != 0;
    return true;
}

void mems_to_ecu_data(const mems_data_t *in, bool connected, ecu_data_t *out) {
    if (in == NULL || out == NULL) return;

    // Les champs non fournis par MEMS (injection, trims, ralenti...) restent
    // explicitement indisponibles plutôt que non initialisés.
    *out = ecu_data_unavailable();
    out->connected = connected;
    out->rpm = (float)in->engine_rpm;
    out->coolant_temp = (float)in->coolant_temp;
    out->battery_voltage = in->battery_voltage;
    out->ambient_temp = (float)in->ambient_temp;
    out->intake_air_temp = (float)in->intake_air_temp;
    out->fuel_rail_temp = (float)in->fuel_temp;
    out->map_sensor_kpa = in->map_kpa;
    out->throttle_pot_voltage = in->throttle_pot_voltage;
    out->ignition_advance = in->ignition_advance;
    out->coil_1_charge_time = in->coil_time;
    out->coil_2_charge_time = in->coil_time;
    out->coil_time_microseconds = in->coil_time * 1000.0f;
    out->estimated_air_fuel = in->air_fuel_ratio;
    out->lambda_mv = (float)in->lambda_voltage_mv;
    out->o2_mv = (float)in->lambda_voltage_mv;
    out->idle_valve_position = (float)in->iac_position;

    out->faults_available = true;
    out->fault_flags =
        (in->coolant_sensor_fault ? ECU_FAULT_COOLANT_SENSOR : 0u) |
        (in->intake_air_sensor_fault ? ECU_FAULT_INTAKE_AIR_SENSOR : 0u) |
        (in->fuel_pump_fault ? ECU_FAULT_FUEL_PUMP : 0u) |
        (in->throttle_pot_fault ? ECU_FAULT_THROTTLE_POT : 0u);

    // Estimation du % de gaz depuis la tension du potentiomètre papillon
    // (le pourcentage n'est pas fourni tel quel par MEMS). Plage nominale
    // ~0.6 V (fermé) .. ~4.6 V (plein gaz).
    const float lo = 0.6f, hi = 4.6f;
    float pct = (in->throttle_pot_voltage - lo) / (hi - lo) * 100.0f;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    out->throttle = pct;

    // MEMS 1.6/1.9 : pas de sonde de température d'huile. oil_temp reste NAN
    // (affiché « -- ») plutôt qu'un faux 0 °C permanent.
}
