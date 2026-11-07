# Flash layout migration: 4M3M → 4M2M

Up to 1.23.x the original firmware used `eesz=4M3M`; it now uses `eesz=4M2M`. Weather Clock has always been 4M2M, so this matters only for clocks coming from an old SmallTV Pomodoro version. This page explains what changed and the limits involved.

## Flash map (4 MB)

| Range | Before (4M3M) | After (4M2M) |
|---|---|---|
| `0x000000` | boot loader + program (up to ~1019 KB, set by the linker) | same |
| free area for updates | from the end of the program to `0x100000` (≈ 1 MB − program) | from the end of the program to `0x200000` (≈ 2 MB − program) |
| LittleFS | `0x100000`–`0x3FA000` (3 MB, 8 KB blocks) | `0x200000`–`0x3FA000` (2 MB, 8 KB blocks) |
| EEPROM (settings) | `0x3FB000` | `0x3FB000` (same) |
| RF cal | `0x3FC000` | `0x3FC000` (same) |
| Wi-Fi / SDK | `0x3FD000`–`0x3FFFFF` | same |

Checked against core 3.1.2 (`boards.txt` and `tools/sdk/ld/eagle.flash.4m2m.ld` / `4m3m.ld`): `_EEPROM_start = 0x405fb000`, `rfcal_addr=0x3FC000`, `_FS_end = 0x405FA000`, block `0x2000` and page `0x100` in both; only `_FS_start` moves (`0x40300000` → `0x40400000`), and the program window (`irom0_0_seg`, 0xfeff0 bytes ≈ 1019 KB) is the same. Also confirmed in the compiled ELF (`nm`). So **settings, password, saved Wi-Fi and the restart counter survive**; only LittleFS moves.

New firmware limit: **~1019 KB** (1,044,464 bytes, the ESP8266's execution ceiling). Before, the practical limit was ~600 KB (firmware + `.gz` fitting in 1 MB).

## The transition limit

The first 4M2M version reaches clocks as a network update done by the **old** (4M3M) firmware, which only sees 1 MB: the `.gz` is written at the end of that 1 MB and the decompressed firmware must end before it. So **the first 4M2M version has to fit the old layout**: `firmware (rounded to 4 KB) + .gz (rounded, +512 bytes of upload overhead) ≤ 1 MB`.

Safeguard: `build.sh` **refuses** a build over that limit while `CLOCKS_4M3M=1` (the default). Change it on purpose, with `CLOCKS_4M3M=0 sh build.sh`, once no clock is left on the old layout. A clock on the old layout that receives a version over the limit refuses the swap ("firmware too large to decompress") and keeps its current firmware, with no risk.

## What happens on the clock

- **First boot on 4M2M** (`fsMount` in `main.cpp`): without the `FS2A` mark in EEPROM (offset 433), whatever sits at `0x200000` is a piece of the old file system (or of the original firmware). The firmware formats once, recreates `/image`, `/gif` and `/faces`, writes the mark and raises the "reorganised" notice (`fs.mig` in `/api/state`). On later boots LittleFS is mounted **without formatting**; if the mark exists and mounting fails, it does not format (the panel shows a *Format storage* button).
- Formatting only happens on the 4M2M layout (detected through the linker's `_FS_start` symbol): the same source built as 4M3M never formats.
- *Erase everything* (`/rec/wipe`) rewrites the mark after clearing EEPROM, so the next boot doesn't format again or offer a backup that doesn't exist.
- Lost: photos and GIFs, downloaded faces and the panel in `/www` (the loader downloads it again by itself). Kept: settings, passwords, Wi-Fi and network (EEPROM) and, copied from the old LittleFS before formatting, the history (`/stats.bin`), the rotation (`/rotation.txt`) and the end-of-phase notices (`/hooks.txt`). On the first boot the old LittleFS (0x100000-0x3FA000) is still intact: a network update only writes below 0x100000.

## Photos, GIFs and the panel backup

When the offered version has `"layout":"4m2m"` in `latest.json`/`beta.json` and the clock doesn't report `fs.l2` yet (old layout), the panel, before updating:

1. says what will be erased and asks for confirmation;
2. creates and downloads **one** backup `.json` with settings, history, the list of faces, notice links **and the photos and GIFs** (base64, read from `/fs?f=...`);
3. only downloads and installs the firmware if the backup worked and the user confirms the file was saved.

After the update the panel shows a "file storage was reorganised" card; **Restore backup** sends back settings, history, notices, faces (from the gallery) and photos/GIFs (through `/api/upload`; anything that doesn't fit in 2 MB is counted and reported) and dismisses the card (`/api/migdone`).

## Going back from 4M2M

Going back to a 4M3M version (or to the original firmware) mixes up the addresses: 4M3M expects LittleFS at `0x100000` and may write over what 4M2M keeps from `0x200000`. Settings survive; make a backup first and restore it afterwards (see also RECOVERY.md).
