// telemetry.h
#pragma once
#include <Arduino.h> // ✅ For uint32_t
void telemetry_task(void *pvParameters);
// 📡 Add this so main.cpp and telemetry.cpp can use it
const char* get_mode_name(uint32_t mode);