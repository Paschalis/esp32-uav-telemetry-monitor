#include "telemetry.h"
#include "ui.h"
#include <WiFiUdp.h>
#include "mavlink.h"

static const char* system_type = "Unknown";  // PX4, ArduPilot, etc.
static uint8_t detected_autopilot = 0xFF; // saved valid autopilot type


static lv_obj_t *lost_screen = nullptr;
static bool telemetry_lost_displayed = false;
static unsigned long last_packet_time = 0;

extern WiFiUDP udp;

// 🛫 Mode name helper
const char* get_mode_name(uint32_t mode) {
  switch (mode) {
    case 0: return "Stabilize";
    case 1: return "Acro";
    case 2: return "AltHold";
    case 3: return "Auto";
    case 4: return "Guided";
    case 5: return "Loiter";
    case 6: return "RTL";
    case 7: return "Circle";
    case 9: return "Land";
    case 11: return "Drift";
    case 13: return "Sport";
    case 14: return "Flip";
    case 15: return "AutoTune";
    case 16: return "Position";
    case 17: return "Brake";
    case 18: return "Throw";
    case 19: return "Avoid ADSB";
    case 20: return "Guided No GPS";
    case 21: return "Smart RTL";
    case 81: return "PX4 Manual";
    case 82: return "PX4 AltCtl";
    case 83: return "PX4 PosCtl";
    case 84: return "PX4 Mission";
    case 85: return "PX4 Hold";
    case 86: return "PX4 Return";
    case 87: return "PX4 Offboard";
    default: return "Unknown";
  }
}

// 📡 Show telemetry lost screen
void show_lost_signal_screen() {
    if (!lost_screen) {
        lost_screen = lv_obj_create(NULL);
        lv_obj_clear_flag(lost_screen, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(lost_screen, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);

        lv_obj_t *label = lv_label_create(lost_screen);
        lv_label_set_text(label, "TELEMETRY LOST");
        lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    }
    lv_scr_load_anim(lost_screen, LV_SCR_LOAD_ANIM_FADE_IN, 500, 0, false);
}

// 🔁 Restore main UI
void restore_main_ui() {
    create_ui();  // Rebuild tabs
    lv_scr_load_anim(lv_scr_act(), LV_SCR_LOAD_ANIM_FADE_IN, 500, 0, false);
}

void telemetry_task(void *pvParameters) {
  mavlink_message_t msg;
  mavlink_status_t status;

  bool first_ui_created = false;  // 🆕

  while (true) {
    int packetSize = udp.parsePacket();
    if (packetSize) {
      while (udp.available()) {
        uint8_t c = udp.read();
        if (mavlink_parse_char(MAVLINK_COMM_0, c, &msg, &status)) {
          last_packet_time = millis();  // 📍 Update last telemetry packet received

          switch (msg.msgid) {
            case MAVLINK_MSG_ID_HEARTBEAT: {
              mavlink_heartbeat_t hb;
              mavlink_msg_heartbeat_decode(&msg, &hb);

              if (hb.autopilot != MAV_AUTOPILOT_INVALID) {
                if (detected_autopilot != hb.autopilot) {
                  detected_autopilot = hb.autopilot;
                  if (hb.autopilot == MAV_AUTOPILOT_ARDUPILOTMEGA) system_type = "ArduPilot";
                  else if (hb.autopilot == MAV_AUTOPILOT_PX4) system_type = "PX4";
                  else system_type = "Unknown";
                }
              }

              lv_label_set_text_fmt(label_mode, "Mode: %s (%s)", get_mode_name(hb.custom_mode), system_type);
              break;
            }

            case MAVLINK_MSG_ID_VFR_HUD: {
              mavlink_vfr_hud_t hud;
              mavlink_msg_vfr_hud_decode(&msg, &hud);
              lv_label_set_text_fmt(label_alt, "Altitude: %.1f m", hud.alt);
              lv_label_set_text_fmt(label_speed, "Speed: %.1f m/s", hud.groundspeed);
              lv_label_set_text_fmt(label_heading, "Heading: %d°", hud.heading);
              break;
            }

            case MAVLINK_MSG_ID_GPS_RAW_INT: {
              mavlink_gps_raw_int_t gps;
              mavlink_msg_gps_raw_int_decode(&msg, &gps);
              const char* fix_str = gps.fix_type >= 3 ? "3D Fix" : gps.fix_type == 2 ? "2D Fix" : "No Fix";
              lv_label_set_text_fmt(label_gps, "GPS: %s (%d sats)", fix_str, gps.satellites_visible);
              break;
            }

            case MAVLINK_MSG_ID_RADIO_STATUS: {
              mavlink_radio_status_t r;
              mavlink_msg_radio_status_decode(&msg, &r);

              lv_color_t color;
              if (r.rssi > 70) color = lv_palette_main(LV_PALETTE_GREEN);
              else if (r.rssi > 40) color = lv_palette_main(LV_PALETTE_YELLOW);
              else color = lv_palette_main(LV_PALETTE_RED);

              lv_label_set_text_fmt(label_rssi, "RSSI: %d%%", r.rssi);
              lv_obj_set_style_text_color(label_rssi, color, LV_PART_MAIN);
              break;
            }
          }
        }
      }
    }

    // 🧠 Handle telemetry lost or restored
    unsigned long now = millis();

    if (!first_ui_created) {
      if (now > 5000) {   // 📋 UI and screen should be ready after 5 seconds
        first_ui_created = true;
        last_packet_time = millis();  // ✅ Restart timing properly
      }
    } else {
      if (!telemetry_lost_displayed && now - last_packet_time > 5000) {
        show_lost_signal_screen();
        telemetry_lost_displayed = true;
      } else if (telemetry_lost_displayed && now - last_packet_time < 5000) {
        restore_main_ui();
        telemetry_lost_displayed = false;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
