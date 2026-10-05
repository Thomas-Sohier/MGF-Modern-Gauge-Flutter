#pragma once

// Single-threaded host test: no scheduler or concurrent critical sections.
typedef unsigned portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0U
#define portENTER_CRITICAL(lock)     (++*(lock))
#define portEXIT_CRITICAL(lock)      (--*(lock))
