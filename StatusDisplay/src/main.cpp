#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Preferences.h>
#include "ui.h"
#include "telemetry.h"
#include "ota_web.h"
#include "version.h"
#include "esp32_smartdisplay.h"
#include "globals.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BOOT_BUTTON_PIN 0

Preferences prefs;
WiFiUDP udp;

void setup() {
  Serial.begin(115200);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  delay(100);

  bool bootPressed = (digitalRead(BOOT_BUTTON_PIN) == LOW);

  // 🖥️ Display init
  smartdisplay_init();
  lv_display_t *disp = lv_display_get_default();
  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_90);
  lv_timer_create_basic();

  // 🕒 Allow LVGL settle
  unsigned long start = millis();
  while (millis() - start < 200) {
      lv_tick_inc(5);
      lv_timer_handler();
      delay(5);
  }

  // 🛠 Check boot button
  if (bootPressed) {
    Serial.println("🛠 BOOT button held - forcing AP OTA mode...");
    prefs.begin("netmon", false);
    prefs.putBool("apmode", true);
    prefs.end();
    delay(300);
    ESP.restart();
  }

  prefs.begin("netmon", false);
  bool apMode = prefs.getBool("apmode", false);
  prefs.end();

  if (apMode) {
    Serial.println("🚀 Starting in OTA Upload Mode...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32-UAV", "password123");

    start_upload_webserver();
    delay(200);
    xTaskCreatePinnedToCore(ota_ui_task, "ota_ui_task", 8192, NULL, 2, NULL, 1);

    while (true) {
      server.handleClient();
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }

  // 🛰 Normal telemetry mode
  Serial.println("📡 Starting in Telemetry Mode...");

  // 📋 Step 1: Start WiFi AP mode (important)
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32-UAV", "password123");


  delay(200);  // Let WiFi stack initialize

  // 📋 Step 2: Start UDP server
  udp.begin(14550);

  // 📋 Step 3: Start UI task
  xTaskCreatePinnedToCore(ui_task, "ui_task", 8 * 1024, NULL, 2, NULL, 1);

  delay(200);  // Allow UI to settle

  // 📋 Step 4: Start telemetry + web tasks
  xTaskCreatePinnedToCore(telemetry_task, "telemetry_task", 8 * 1024, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(web_server_task, "web_server_task", 4 * 1024, NULL, 1, NULL, 1);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1));
}
