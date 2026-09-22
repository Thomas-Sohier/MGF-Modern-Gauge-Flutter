#include <stdio.h>

#include "infrastructure/kline_board_config.h"

#ifndef KLINE_CONFIG_EXPECT_TX
#define KLINE_CONFIG_EXPECT_TX 40
#endif

#ifndef KLINE_CONFIG_EXPECT_RX
#define KLINE_CONFIG_EXPECT_RX 38
#endif

#if MGF_KLINE_TX_GPIO != KLINE_CONFIG_EXPECT_TX
#error "unexpected K-line TX configuration"
#endif

#if MGF_KLINE_RX_GPIO != KLINE_CONFIG_EXPECT_RX
#error "unexpected K-line RX configuration"
#endif

#if MGF_LILYGO_SDMMC_ENABLED != 0
#error "SDMMC must be disabled in the K-line profile"
#endif

#if MGF_USE_MEMS_KLINE != 0
#error "the host/default K-line profile must keep the simulator enabled"
#endif

#if MGF_KLINE_LOCAL_ECHO != 0
#error "local echo must be opt-in"
#endif

int main(void) {
    if (MGF_KLINE_TX_GPIO == MGF_KLINE_RX_GPIO) return 1;
    puts("kline config: OK");
    return 0;
}
