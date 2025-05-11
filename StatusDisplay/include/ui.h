// ui.h
#ifndef UI_H
#define UI_H

#include <lvgl.h>
#include "globals.h"

// Extern declarations for UI elements (so telemetry.cpp etc. can update them)
extern lv_obj_t *label_mode;
extern lv_obj_t *label_gps;
extern lv_obj_t *label_alt;
extern lv_obj_t *label_speed;
extern lv_obj_t *label_heading;
extern lv_obj_t *label_rssi;

// Also for your tab objects if needed elsewhere
extern lv_obj_t *tab_telemetry;
extern lv_obj_t *tab_settings;
extern lv_obj_t *tabview;

// Progress bar for OTA upload (optional, but cleaner to have it declared)
extern lv_obj_t *ota_progress_bar;

// UI functions
void create_ui();
void show_upload_screen();
void show_boot_logo();
void ui_task(void *pvParameters);
void ota_ui_task(void *pvParameters);   // 🛠️ OTA Upload Screen Task (NEW)
#endif // UI_H
