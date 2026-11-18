#pragma once
#include <stdint.h>

// Unsigned subtraction keeps freshness checks valid across millis() rollover.
inline bool lightningRecent(uint32_t now, uint32_t received, uint32_t ago, int16_t nearKm) {
  const uint32_t elapsed = now - received;
  return received != 0 && elapsed <= 300000UL && nearKm >= 0 &&
         ago < 1800UL && elapsed / 1000 < 1800UL - ago;
}

inline bool lightningRotationAllowed(bool eventOnly, uint32_t now, uint32_t received,
                                     uint32_t ago, int16_t nearKm) {
  return !eventOnly || lightningRecent(now, received, ago, nearKm);
}
