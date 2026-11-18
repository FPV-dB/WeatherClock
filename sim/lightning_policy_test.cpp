#include "../weatherclock/lightning_policy.h"
#include <assert.h>
#include <stdio.h>

int main() {
  assert(!lightningRecent(1000, 0, 0, 10));
  assert(!lightningRecent(1000, 1000, UINT32_MAX, 10));
  assert(!lightningRecent(1000, 1000, 0, -1));
  assert(lightningRecent(1000, 1000, 0, 10));
  assert(lightningRecent(301000, 1000, 0, 10));
  assert(!lightningRecent(301001, 1000, 0, 10));
  assert(lightningRecent(1999, 1000, 1799, 10));
  assert(!lightningRecent(2000, 1000, 1799, 10));
  assert(!lightningRecent(1000, 1000, 1800, 10));
  assert(!lightningRecent(1000, 1000, UINT32_MAX - 1, 10));
  assert(lightningRecent(999, UINT32_MAX - 1000, 0, 10));
  assert(!lightningRotationAllowed(true, 1000, 0, 0, -1));
  assert(lightningRotationAllowed(false, 1000, 0, 0, -1));
  assert(lightningRotationAllowed(true, 1000, 1000, 0, 10));
  puts("Lightning policy: all boundary and rotation-mode tests passed");
}
