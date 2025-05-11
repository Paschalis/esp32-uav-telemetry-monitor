// ESP32_UAV_Telemetry_Monitor/src/ui.cpp
#include <Arduino.h>  // Needed for millis()
#include "ui.h"
#include <lvgl.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Preferences.h>
#include "version.h"
#include "ota_web.h"  // ✅ To get access to ota_progress_bar
#include "globals.h"

lv_obj_t* ota_progress_bar = nullptr;  // <-- define it exactly once here
// UI Objects
lv_obj_t *label_mode, *label_gps, *label_alt, *label_speed, *label_heading, *label_rssi;
lv_obj_t *tab_telemetry, *tab_settings, *tabview;
lv_obj_t *ta_ssid, *ta_pass, *btn_save_wifi, *btn_reboot, *btn_reset, *btn_wifi, *label_save_wifi, *label_reboot, *label_reset, *label_wifi, *msgbox;

extern Preferences prefs;  // declare it exists
extern volatile unsigned long upload_start_time; 

// Create the main UI
void create_ui() {
  // Create a full-screen base
  lv_obj_t *screen = lv_obj_create(NULL);
  lv_obj_set_size(screen, lv_pct(100), lv_pct(100));
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(screen, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

  // Create tabview
  tabview = lv_tabview_create(screen);
  lv_tabview_set_tab_bar_position(tabview, LV_DIR_TOP);
  lv_tabview_set_tab_bar_size(tabview, 50);

  // Add tabs
  tab_telemetry = lv_tabview_add_tab(tabview, "Telemetry");
  tab_settings  = lv_tabview_add_tab(tabview, "Settings");

  // --- Telemetry Labels ---
  label_mode = lv_label_create(tab_telemetry);
  lv_label_set_text(label_mode, "Mode: ---");
  lv_obj_align(label_mode, LV_ALIGN_TOP_LEFT, 10, 10);

  label_gps = lv_label_create(tab_telemetry);
  lv_label_set_text(label_gps, "GPS: ---");
  lv_obj_align(label_gps, LV_ALIGN_TOP_LEFT, 10, 40);

  label_alt = lv_label_create(tab_telemetry);
  lv_label_set_text(label_alt, "Altitude: --- m");
  lv_obj_align(label_alt, LV_ALIGN_TOP_LEFT, 10, 70);

  label_speed = lv_label_create(tab_telemetry);
  lv_label_set_text(label_speed, "Speed: --- m/s");
  lv_obj_align(label_speed, LV_ALIGN_TOP_LEFT, 10, 100);

  label_heading = lv_label_create(tab_telemetry);
  lv_label_set_text(label_heading, "Heading: ---°");
  lv_obj_align(label_heading, LV_ALIGN_TOP_LEFT, 10, 130);

  label_rssi = lv_label_create(tab_telemetry);
  lv_label_set_text(label_rssi, "RSSI: ---%");
  lv_obj_align(label_rssi, LV_ALIGN_TOP_LEFT, 10, 160);

  // 🛠 Create a simple flex container in Settings tab
    lv_obj_t* settings_container = lv_obj_create(tab_settings);
    lv_obj_set_size(settings_container, lv_pct(100), lv_pct(100));
    lv_obj_center(settings_container);
    lv_obj_set_layout(settings_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(settings_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(settings_container, 12, 0);
    lv_obj_set_style_pad_row(settings_container, 16, 0);

    // 🔄 Reboot Button
    lv_obj_t* btn_reboot = lv_btn_create(settings_container);
    lv_obj_t* lbl_reboot = lv_label_create(btn_reboot);
    lv_label_set_text(lbl_reboot, "Reboot ESP32");
    lv_obj_center(lbl_reboot);
    lv_obj_add_event_cb(btn_reboot, [](lv_event_t *e) {
      lv_obj_t* mbox = lv_msgbox_create(NULL);
      lv_msgbox_add_title(mbox, "Rebooting...");
      lv_msgbox_add_text(mbox, "Please wait...");
      lv_obj_center(mbox);
      lv_scr_load(mbox);
      lv_timer_handler();
      delay(500);
      ESP.restart();
    }, LV_EVENT_CLICKED, NULL);

    // ♻️ Factory Reset Button
    lv_obj_t* btn_reset = lv_btn_create(settings_container);
    lv_obj_t* lbl_reset = lv_label_create(btn_reset);
    lv_label_set_text(lbl_reset, "Factory Reset (AP Mode)");
    lv_obj_center(lbl_reset);
    lv_obj_add_event_cb(btn_reset, [](lv_event_t *e) {
        Preferences prefs;
        prefs.begin("netmon", false);
        prefs.clear();
        prefs.putBool("apmode", true);
        prefs.end();

        lv_obj_t* mbox = lv_msgbox_create(NULL);
        lv_msgbox_add_title(mbox, "Resetting...");
        lv_msgbox_add_text(mbox, "Please wait...");
        lv_obj_center(mbox);
        lv_scr_load(mbox);
        lv_timer_handler();
        delay(500);
        ESP.restart();
    }, LV_EVENT_CLICKED, NULL);

    // 🛰️ Show AP Info (Optional)
    lv_obj_t* label_apinfo = lv_label_create(settings_container);
    String apInfo = "SSID: ESP32-UAV\nPassword: password123\nUDP Port: 14550";
    lv_label_set_text(label_apinfo, apInfo.c_str());

  // Load screen
  lv_scr_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, 500, 0, true);

}

// Show a minimal OTA upload screen
void show_upload_screen() {
  lv_obj_t *screen = lv_obj_create(NULL);
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(screen, lv_color_white(), LV_PART_MAIN);

  // Title
  lv_obj_t *label = lv_label_create(screen);
  lv_label_set_text(label, "Waiting for upload...");
  lv_obj_set_style_text_color(label, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 30);

  // Progress bar
  ota_progress_bar = lv_bar_create(screen);
  lv_obj_set_size(ota_progress_bar, 200, 20);
  lv_obj_align(ota_progress_bar, LV_ALIGN_CENTER, 0, 30);
  lv_bar_set_range(ota_progress_bar, 0, 100);
  lv_bar_set_value(ota_progress_bar, 0, LV_ANIM_OFF); // Start empty

  lv_scr_load(screen);
}

// Display a custom splash logo
void show_boot_logo() {
  lv_obj_t *boot_screen = lv_obj_create(NULL);
  lv_obj_clear_flag(boot_screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(boot_screen, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);

  lv_obj_t *label = lv_label_create(boot_screen);
  lv_label_set_text(label, "ESP32 UAV Monitor");
  lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, -20);

  lv_obj_t *ver = lv_label_create(boot_screen);
  lv_label_set_text_fmt(ver, "Firmware: %s", FW_VERSION);
  lv_obj_set_style_text_color(ver, lv_color_white(), LV_PART_MAIN);
  lv_obj_align(ver, LV_ALIGN_CENTER, 0, 20);

  lv_scr_load(boot_screen);

  // 🕒 Hold for 5 seconds *with* LVGL ticking
  unsigned long start = millis();
  while (millis() - start < 5000) {
      lv_tick_inc(5);
      lv_timer_handler();
      delay(5);
  }
}

void ota_ui_task(void *pvParameters) {
  unsigned long last_tick = millis();
  unsigned long last_ui_update = millis();
  int displayed_progress = 0;

  while (true) {
    unsigned long now = millis();
    lv_tick_inc(now - last_tick);
    last_tick = now;

    lv_timer_handler();

    if (ota_progress_bar) {
      if (now - last_ui_update > 100) {

        // 🚀 Smooth fake progress using elapsed time
        if (upload_start_time > 0) {
          unsigned long elapsed = now - upload_start_time;

          if (upload_progress < 100) {
            displayed_progress = map(elapsed, 0, 15000, 0, 90);  // 15s to reach 90%
            if (displayed_progress > 90) displayed_progress = 90;
          } else {
            displayed_progress = 100;
          }
          lv_bar_set_value(ota_progress_bar, displayed_progress, LV_ANIM_OFF);
          Serial.printf("Smoothed OTA: %d%%\n", displayed_progress);
        }

        last_ui_update = now;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(5));
  }

}






// FreeRTOS UI task
void ui_task(void *pvParameters) {
  // 🕒 Let LVGL settle before touching UI
  unsigned long start = millis();
  while (millis() - start < 300) {
    lv_tick_inc(5);
    lv_timer_handler();
    delay(5);
  }

  show_boot_logo();  // 🖼️ Optional splash

  vTaskDelay(pdMS_TO_TICKS(500));  // Let splash show a little

  create_ui();  // 📄 Now create your real UI

  start = millis();
  while (true) {
    unsigned long now = millis();
    lv_tick_inc(now - start);
    start = now;
    lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}




