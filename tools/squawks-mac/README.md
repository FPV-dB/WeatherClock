# Bad Squawks helper for the Aircraft Bad Squawks face

Feeds the built-in **Aircraft Bad Squawks** face with the last 20 aircraft seen anywhere in the world squawking:

| Code | Meaning | Colour on the clock |
|---|---|---|
| 7700 | General emergency | red |
| 7500 | Unlawful interference (hijack) | purple |
| 7600 | Radio failure | orange |
| 0100 | Watched on request | blue |

For each aircraft the clock shows the code, callsign, aircraft type, registration, route (or operator), the nearest big airport when it was seen, its altitude, and how long ago it started squawking ("NOW" while it still is). Five aircraft per page, four pages, a page every 2.5 seconds.

## How it works

- `squawks.py` asks the ADS-B network every minute for every aircraft squawking one of the codes (adsb.fi first, adsb.lol if adsb.fi does not answer). A code must be seen on two checks in a row to count, which filters out one-off transponder glitches. The same aircraft and code again after 30 minutes counts as a new event. Routes come from [adsbdb.com](https://www.adsbdb.com/), places from the [OurAirports](https://ourairports.com/) airport list (downloaded once a month). It writes `payload.txt` and only talks to the internet.
- `push.sh` runs every minute and sends that list to `POST /api/squawks` on every clock, using Apple's `curl` (macOS blocks background Python from the local network). It finds the other clocks on the same Wi-Fi by itself, so `clocks.txt` needs only one address.
- If the feed stops, `push.sh` stops sending and the clocks stop showing "NOW" after 10 minutes.

On the clock, the face joins the rotation once it has received a list. Picked by hand with no list yet, it says "No data yet".

## Set up (macOS)

Standard library only: Apple's `/usr/bin/python3` is enough.

```sh
./install.sh 192.168.1.100        # the address of one of your clocks
```

This copies everything to `~/WeatherClockSquawks` and starts two background jobs. Logs: `~/WeatherClockSquawks/squawks.log` (each new event is logged as `NEW`) and `push.log`. To change the codes or the two-check rule, edit `~/WeatherClockSquawks/config.json`. To stop: `~/WeatherClockSquawks/uninstall.sh`.

## What the clock receives

`POST /api/squawks`, newest first, with for each row `i` from 0 to 19: `q<i>` code, `f<i>` callsign, `r<i>` registration, `t<i>` type, `i<i>` route or operator, `w<i>` nearest big airport, `a<i>` altitude in feet (-1 unknown, -2 on the ground), `s<i>` UTC seconds when it started squawking, `l<i>` 1 while it still is.
