# Lightning helper for the Lightning face

Feeds the built-in **Lightning** face with live lightning strikes from the [Blitzortung.org](https://www.blitzortung.org/) community lightning network: the nearest strike, how many strikes in the last 10 minutes, the direction of the storm from the clock and the direction it is moving.

Blitzortung data is free for private, non-commercial use. Please keep it that way.

## How it works

- `lightning.py` stays connected to the Blitzortung live feed and keeps the strikes within the radius (100 km by default) around the clock's city. Every 30 seconds it writes a short summary to `payload.txt`. It only talks to the internet.
- `push.sh` runs every minute and sends that summary to `POST /api/lightning` on every clock, using Apple's `curl` (macOS blocks background Python from the local network). It reads the clock's city from the clock itself, and finds the other clocks on the same Wi-Fi by itself, so `clocks.txt` needs only one address.
- If the feed stops, `push.sh` stops sending and the clocks show "waiting" after 5 minutes.

On the clock, the Lightning face joins the rotation only while there has been a strike within the radius in the last 30 minutes. Picked by hand, it always shows: "No lightning nearby" when all is clear.

## Set up (macOS)

Standard library only: Apple's `/usr/bin/python3` is enough.

```sh
./install.sh 192.168.1.100        # the address of one of your clocks
```

This copies everything to `~/WeatherClockLightning` and starts two background jobs. Logs: `~/WeatherClockLightning/lightning.log` and `push.log`. To change the radius, edit `radius_km` in `~/WeatherClockLightning/config.json`. To stop: `~/WeatherClockLightning/uninstall.sh`.

## What the clock receives

`POST /api/lightning` with `near` (nearest strike in km, -1 none), `n` (strikes in the last 10 minutes), `from` (bearing from the clock to the storm, degrees), `mv` (direction the storm is moving, degrees, -1 unknown), `r` (radius in km) and `ago` (seconds since the latest strike, -1 none seen).
