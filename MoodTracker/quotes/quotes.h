#pragma once
#include <stdint.h>

#define MOOD_QUOTES_COUNT 30

// Returns a quote string by index (0..MOOD_QUOTES_COUNT-1)
const char* mood_quote_get(uint8_t index);
