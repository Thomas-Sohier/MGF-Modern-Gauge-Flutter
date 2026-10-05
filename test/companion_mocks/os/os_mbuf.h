#pragma once

#include <stdint.h>

struct os_mbuf {
    const void *data;
    uint16_t length;
};

#define OS_MBUF_PKTLEN(om) ((om)->length)
