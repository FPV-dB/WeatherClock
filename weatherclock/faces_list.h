// faces_list.h — registro dos mostradores (a única lista a editar para criar um novo).
//
// Cada linha: X(ID, função, "Nome no painel", "Descrição curta").
// A ordem é o número salvo nas configurações: acrescente sempre no FIM, nunca no meio.
// A partir desta lista saem: o enum THEME_*, a tabela de desenho em ui.cpp, a galeria do
// painel (/api/faces), os testes e as prévias do simulador. Guia: docs/MAKING_A_FACE.md
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
  X(WIND, themeWind, "Wind", "Compass, speed, gusts and wind strength")
