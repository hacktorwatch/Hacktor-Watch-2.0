#pragma once

#include <Arduino.h>

#ifndef HACKTOR_DEBUG_LEVEL
#define HACKTOR_DEBUG_LEVEL 0
#endif

#if HACKTOR_DEBUG_LEVEL > 0
#define LOG_PRINT(level, message)                                     \
  do {                                                                \
    if ((level) <= HACKTOR_DEBUG_LEVEL) { Serial.println(message); }  \
  } while (0)
#define LOG_PRINTF(level, format, ...)                                     \
  do {                                                                     \
    if ((level) <= HACKTOR_DEBUG_LEVEL) { Serial.printf(format, __VA_ARGS__); } \
  } while (0)
#else
#define LOG_PRINT(level, message) ((void)0)
#define LOG_PRINTF(level, format, ...) ((void)0)
#endif

