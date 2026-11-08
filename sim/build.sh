#!/bin/sh
# Gera assets.h, compila e roda o simulador; gera out/*.png, out/sheet.png e out/assets.png.
# Needs: zig (https://ziglang.org), node and the Adafruit GFX library installed by arduino-cli.
cd "$(dirname "$0")"
ZIG=$(command -v zig)
if [ -z "$ZIG" ] && [ -n "$LOCALAPPDATA" ]; then  # Windows: zig installed by winget outside the shell's PATH
  ZIG=$(ls "$(cygpath -u "$LOCALAPPDATA")"/Microsoft/WinGet/Packages/zig.zig*/zig-*/zig.exe 2>/dev/null | head -1)
fi
[ -z "$ZIG" ] && ! command -v g++ >/dev/null && { echo "zig (or g++ with multilib) not found"; exit 1; }
node ../assets/generate.js || exit 1  # assets/*.txt -> weatherclock/assets.h
# the firmware is built without float printf/scanf (build.sh): %f and friends would print nothing
if grep -nE "%[-+ #0-9.]*[fFeEgG]|toFloat|atof|strtod|scanf" ../weatherclock/*.cpp ../weatherclock/*.h; then
  echo "use integers or String(x, places) instead of float in printf/scanf"; exit 1
fi
GFX="${GFX:-$HOME/Documents/Arduino/libraries/Adafruit_GFX_Library}"
mkdir -p out
# Windows: 32-bit Windows executable; Linux (also the GitHub automatic build): 32-bit static Linux
if [ -n "$LOCALAPPDATA" ]; then TARGET=x86-windows-gnu; BIN=sim.exe; else TARGET=x86-linux-musl; BIN=sim; fi
# without zig (Linux): the 32-bit g++ (package g++-multilib), the same one the GIF and JPEG tests use; with
# _FILE_OFFSET_BITS=64, otherwise the 32-bit readdir fails on folders with large inodes (WSL) and the packs are left out
if [ -n "$ZIG" ]; then CXX="$ZIG c++ -target $TARGET"; else CXX="g++ -m32 -D_FILE_OFFSET_BITS=64"; fi
$CXX -O1 -std=c++17 -DARDUINO=100 -DARDUINOJSON_ENABLE_ARDUINO_STRING=0 -DARDUINOJSON_ENABLE_ARDUINO_STREAM=0 -DARDUINOJSON_ENABLE_ARDUINO_PRINT=0 -DARDUINOJSON_ENABLE_PROGMEM=0 -Ishim -I"$GFX" -I"${JSON:-$HOME/Documents/Arduino/libraries/ArduinoJson/src}" sim.cpp ../weatherclock/ui.cpp ../weatherclock/face_*.cpp ../weatherclock/facepack.cpp ../weatherclock/fonts.cpp "$GFX/Adafruit_GFX.cpp" -x c++ ../weatherclock/src/qrcode/qrcode.c -o "$BIN" || exit 1
./"$BIN"; r=$?
node png.js > /dev/null
node faces.js
node catalog.js
python3 ../scripts/build_panel.py ..  # weatherbox: Python port of web/build.js (no terser), the one releases use
exit $r
