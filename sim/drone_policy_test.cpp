#include "../weatherclock/drone_policy.h"
#include <assert.h>
#include <stdio.h>

int main() {
  assert(droneWindCondition(69, 100) == 0);
  assert(droneWindCondition(70, 100) == 1);
  assert(droneWindCondition(89, 100) == 1);
  assert(droneWindCondition(90, 100) == 2);
  assert(droneWindCondition(100, 100) == 2);
  assert(droneWindCondition(101, 100) == 3);
  assert(droneWindCondition(151, 150) == 3);
  assert(droneWindCondition(0, 0) == 3);
  assert(droneCondition(1, 41, 30, 40, false) == 3);
  assert(droneCondition(31, 1, 30, 40, false) == 3);
  assert(droneCondition(1, 1, 30, 40, true) == 2);
  assert(droneCondition(31, 1, 30, 40, true) == 3);
  assert(droneCondition(1, 1, 30, 40, false) == 0);
  puts("Drone policy: thresholds, rounding, rain and worst-component tests passed");
}
