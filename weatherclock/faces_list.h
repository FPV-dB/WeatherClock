// faces_list.h — registry of the faces (the only list to edit to add a new one).
//
// Each line: X(ID, function, "Name in the panel", "Short description").
// The order is the number saved in the settings: always add at the END, never in the middle.
// This list produces: the THEME_* enum, the drawing table in ui.cpp, the panel's
// gallery (/api/faces), the tests and the simulator previews. Guide: docs/MAKING_A_FACE.md
#pragma once

#define UI_FACES(X)                                                                         \
  X(MATRIX, themeMatrix, "Matrix", "Pixel art with weather and wind cards")         \
  X(TOMATO, themeTomato, "Scene", "Pixel art landscape that changes with the time and weather")      \
  X(ANALOG, themeAnalog, "Analog", "Baton hands and bold markers")                 \
  X(WEATHER, themeWeather, "Weather", "Big temperature and mini cards")                   \
  X(NEON, themeNeon, "Neon", "Pink and cyan glowing tubes")                              \
  X(FLIP, themeFlip, "Flip", "Flipping cards with the Bebas Neue font")               \
  X(LCD, themeLcd, "Retro LCD", "80s digital watch (DSEG7 font)")               \
  X(WORDS, themeWords, "Words", "The time spelled out, in the clock language")         \
  X(FORECAST, themeForecast, "Forecast", "Today and the next 4 days, with rain and low/high") \
  X(FORECAST_CARDS, themeForecastCards, "Forecast cards", "Four days in cards, with the high in focus") \
  X(FORECAST_LCD, themeForecastLcd, "Forecast LCD", "Four days on the retro LCD screen") \
  X(WIND, themeWind, "Wind", "Compass, speed, gusts and wind strength") \
  X(SKY, themeSky, "Sky", "Sun path with sunrise and sunset, and the moon phase") \
  X(HOURS, themeHours, "Next 12 hours", "Temperature graph and chance of rain, hour by hour") \
  X(UMBRELLA, themeUmbrella, "Umbrella", "Do I need an umbrella today? Plus feels-like, UV and air quality") \
  X(AIR, themeAir, "Air & UV", "UV index with today's peak, and PM2.5 and PM10 particle levels") \
  X(MOON, themeMoon, "Moon", "Phase and light, moonrise and moonset, where it is now, and the next full and new moon")
