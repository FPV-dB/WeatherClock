#!/bin/sh
# Builds the firmware into build/ (run sim/build.sh first: it generates assets.h and web.h and tests the screens) and
# also makes the .bin.gz for the compressed update.
#
# build.float= removes newlib's decimal printf/scanf (%f) and strtod: ~7 KB that nothing uses
# (sim/build.sh rejects %f, toFloat and scanf in the code, because on the clock they would print nothing).
#
# Flash layout (LAYOUT, default 4M2M): program up to ~1019 KB (the ESP8266 limit, enforced by the linker) and a 2 MB LittleFS
# at 0x200000. Up to 1.23 it was 4M3M (3 MB LittleFS at 0x100000, only ~1 MB between the program and the file
# storage). LAYOUT=4M3M still builds the same source in the old layout (a transition version that only carries the new panel).
#
# Limits (compressed update: the .gz sits at the end of the free area and the decompressed firmware must end before
# it; the eboot decompresses over the current firmware):
#  - TRANSITION (RELOGIOS_4M3M=1, the default): the first 4M2M version arrives as a network update done by the
#    OLD firmware (4M3M), which only sees 1 MB: rounded firmware + rounded .gz (with ~512 bytes of upload overhead)
#    <= 1 MB (1.23.1: 551 KB + .gz 407 KB). Above that the build is refused: clocks on the old layout could not
#    install it. Once every clock is on 4M2M, run with RELOGIOS_4M3M=0: the ceiling becomes the linker's,
#    ~1019 KB, and the free 2 MB hold firmware + .gz with room to spare;
#  - installing the uncompressed .bin from versions before 1.14: at most 524,288 bytes
#    (warning: above that, those versions must go through 1.14 first);
#  - going back to the original firmware: uncompressed up to ~532 KB; above that, through its .gz (compress.js).
cd "$(dirname "$0")"
CLI="${ARDUINO_CLI:-arduino-cli}"
LAYOUT="${LAYOUT:-4M2M}"
case "$LAYOUT" in 4M2M|4M3M) ;; *) echo "LAYOUT must be 4M2M or 4M3M"; exit 2 ;; esac
"$CLI" compile --fqbn "esp8266:esp8266:generic:eesz=$LAYOUT,xtal=160,ip=lm2n" --build-property "build.float=" \
  --output-dir build weatherclock "$@" || exit 1
node compress.js build/weatherclock.ino.bin > /dev/null || exit 1
size=$(wc -c < build/weatherclock.ino.bin)
gz=$(wc -c < build/weatherclock.ino.bin.gz)
r() { echo $(( ($1 + 4095) / 4096 * 4096 )); }
# 1 MB window of the old layout: applies on 4M3M and, while clocks still use it, on 4M2M too
AREA=1048576
[ "$LAYOUT" = 4M2M ] && [ "${RELOGIOS_4M3M:-1}" = 0 ] && AREA=2097152  # the ceiling is just the linker's (1044464 bytes)
if [ $(( $(r $size) + $(r $((gz + 512))) )) -gt $AREA ]; then
  echo "firmware of $size bytes (.gz $gz): would not fit in the compressed update done by the old layout (4M3M, 1 MB):"
  echo "firmware ($(r $size)) + .gz ($(r $((gz + 512)))) > $AREA. Shrink the code, or use RELOGIOS_4M3M=0 if no clock is left on the old layout."
  exit 1
fi
[ "$size" -gt 524288 ] && echo "warning: $size bytes; clocks on a version before 1.14 must install 1.14 first"
if [ "$AREA" = 2097152 ]; then left=$(( 1044464 - size )); else left=$(( (AREA - $(r $size) - $(r $((gz + 512)))) * 100 / 174 )); fi
echo "firmware: $size bytes, .gz $gz bytes, layout $LAYOUT (can still grow ~$(( left / 1024 )) KB)"
