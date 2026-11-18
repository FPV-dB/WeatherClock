# Roadmap and to-do list

Ideas and planned work for Weather Clock. Nothing here is released yet. Items move into the [README](../README.md) once they ship.

## Local sensor modules (modular, user-defined)

Add small ESP32 boards around the house, each with its own sensors, and show their readings on any Weather Clock on the same Wi-Fi. You choose which modules you have, what they measure and where they sit.

- [ ] **Module firmware for ESP32 boards**: one firmware that detects common sensors on I²C and reports what it finds
- [ ] **Supported sensors (first set)**:
  - [ ] BME280: temperature, humidity, pressure
  - [ ] BME680 / BME688: adds air quality (gas resistance)
  - [ ] BMP280 / BMP388 / BMP390 / BMP580: temperature and pressure only (no humidity)
  - [ ] SHT31 / SHT4x: accurate temperature and humidity
  - [ ] SCD40 / SCD41: CO₂
  - [ ] PMS5003 / SPS30: PM2.5 and PM10 particles
  - [ ] BH1750 / VEML7700: light level
  - [ ] AS3935: real lightning detector (strikes up to about 40 km away, with distance)
  - [ ] DS18B20: waterproof temperature probe (pool, fridge, garden soil)
- [ ] **Setup like the clock**: setup Wi-Fi network with a QR code, then a small page to name the module ("Bedroom", "Garage", "Outside")
- [ ] **Discovery**: modules announce themselves on the local network, and the clock panel lists them next to **My clocks**
- [ ] **User-defined readings**: in the panel, pick which module and which reading appear on which face
- [ ] **New faces**: "Indoor / Outdoor" (local sensor next to the forecast), "Rooms" (one tile per module), and a CO₂ / air quality face with colour levels
- [ ] **History**: keep the last 24 hours on the module and show small graphs in the panel
- [ ] **Alerts**: optional warning card on the clock (for example CO₂ too high, freezer too warm)
- [ ] **Battery option**: deep sleep between readings for modules that run on a battery
- [ ] **Optional extras**: a piezo buzzer for alerts, an open data format (JSON over HTTP) so other projects can read the modules
- [ ] **Docs**: wiring diagrams, parts list with links, and a printable case

## Clock faces

- [ ] Night face (dim, minimal, for bedrooms)
- [ ] News face: choose sources and how long each story shows; custom RSS feed
- [ ] World clock and world weather faces
- [ ] Lightning detected screen: animated alert with nearest strike distance, strike count, and storm direction (preview in the README; needs live strike data from an AS3935 module or an online lightning feed)
- [ ] AI usage face: how much of your Claude Code and OpenAI Codex limits are used, with reset times
- [ ] YouTube new video alert: a card when one of your favourite channels posts a new video (channel picker in the panel)
- [ ] Stocks and commodities face: a few indexes and commodities (for example S&P 500, ASX 200, gold, oil, Bitcoin) with daily change, chosen in the panel
- [ ] Add Rain and Air & UV to the default rotation

## Multiple clocks

- [ ] Each clock gets its own local name automatically (`weatherclock-2.local` and so on)

## Helpers without a computer

- [ ] Cloud radar service, so clocks given as gifts get rain radar without a helper computer
- [ ] Share one aircraft feed between all devices in the house to stay within data limits

## Gifts and first install

- [ ] Test the setup QR codes with a range of phones
- [ ] "Gift-ready" checklist: English defaults, device names, radar in firmware

## Documentation

- [ ] Wiring and parts guide for sensor modules
- [ ] More troubleshooting for clones that use different screens
