#pragma once
#include <stdint.h>

// Compare before division so a value just over a limit cannot round down to 100%.
inline uint8_t droneWindCondition(unsigned value, unsigned limit) {
  if (!limit || value > limit) return 3;
  if (value * 100 < limit * 70) return 0;
  if (value * 100 < limit * 90) return 1;
  return 2;
}

inline uint8_t droneCondition(unsigned wind, unsigned gust, unsigned windLimit,
                              unsigned gustLimit, bool rain) {
  const uint8_t a = droneWindCondition(wind, windLimit);
  const uint8_t b = droneWindCondition(gust, gustLimit);
  uint8_t result = a > b ? a : b;
  return rain && result < 2 ? 2 : result;
}
