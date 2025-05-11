// ota_web.cpp
#include "ota_web.h"
#include <WebServer.h>
#include <Update.h>
#include "version.h"
#include "globals.h"
#include <lvgl.h>
#include "globals.h"
#include "ui.h"
WebServer server(80);
bool fallback_notice = false;


void start_upload_webserver() {
  // OTA Webpage for firmware update
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", R"rawliteral(
    <!DOCTYPE html>
    <html lang='en'>
    <head>
      <meta charset='UTF-8'>
      <meta name='viewport' content='width=device-width, initial-scale=1.0'>
      <title>ESP32 OTA</title>
      <style>
        body {
          font-family: Arial, sans-serif;
          background: #f4f4f4;
          display: flex;
          flex-direction: column;
          align-items: center;
          justify-content: center;
          height: 100vh;
          margin: 0;
        }
        .container {
          background: #fff;
          padding: 2em;
          border-radius: 8px;
          box-shadow: 0 0 20px rgba(0,0,0,0.1);
          text-align: center;
        }
        input[type='file'] {
          margin-bottom: 1em;
        }
        input[type='submit'] {
          padding: 0.5em 1.5em;
          background: #007BFF;
          border: none;
          color: white;
          font-size: 1em;
          border-radius: 4px;
          cursor: pointer;
        }
        input[type='submit']:hover {
          background: #0056b3;
        }
        .progress {
          width: 100%;
          background: #eee;
          border-radius: 5px;
          overflow: hidden;
          margin-top: 1em;
          display: none;
        }
        .bar {
          height: 20px;
          background: #007BFF;
          width: 0;
        }
        .version {
          margin-bottom: 1em;
          font-weight: bold;
        }
      </style>
    </head>
    <body>
      <div class='container'>
        <div class='version' id='fwVersion'>Firmware Version: ...</div>
        <h2>ESP32 UAV MONITOR</h2>
        <h3>Firmware Update</h3>
        <form id='uploadForm' method='POST' action='/update' enctype='multipart/form-data'>
          <input type='file' name='update' required><br>
          <input type='submit' value='Upload & Flash'>
        </form>
        <div class='progress'><div class='bar' id='progressBar'></div></div>
      </div>
      <script>
        // Load version on page load
        window.onload = () => {
          fetch('/version')
            .then(res => res.text())
            .then(version => {
              document.getElementById('fwVersion').innerText = 'Firmware Version: ' + version; 
            });
        };

        const form = document.getElementById('uploadForm');
        const progress = document.querySelector('.progress');
        const bar = document.getElementById('progressBar');

        form.addEventListener('submit', function(e) {
          e.preventDefault();
          const fileInput = form.querySelector("input[type='file']");
          if (!fileInput.files.length) {
            alert("Please select a file first.");
            return;
          }

          const formData = new FormData(form);
          const xhr = new XMLHttpRequest();
          xhr.open("POST", "/update");

          xhr.upload.addEventListener("progress", (e) => {
            if (e.lengthComputable) {
              const percent = (e.loaded / e.total) * 100;
              console.log(`Uploading: ${percent.toFixed(1)}%`);
              bar.style.width = percent + "%";
              progress.style.display = 'block';
            }
          });

          xhr.onload = () => {
            if (xhr.status === 200) {
              alert("Upload successful! Rebooting...");
              location.reload(); // Reboot will interrupt this if it happens quickly
            } else {
              alert("Upload failed.");
            }
          };

          xhr.send(formData);
        });
      </script>
    </body>
    </html>
    )rawliteral");
  });
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", "OK");
  }, []() {
      HTTPUpload& upload = server.upload();
      static size_t total_size = 0;
      static size_t received = 0;

      if (upload.status == UPLOAD_FILE_START) {
          upload_start_time = millis();
          size_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
          Update.begin(maxSketchSpace);
          total_size = upload.totalSize;
          received = 0;
          upload_in_progress = true;
          upload_progress = 0;
      }
      else if (upload.status == UPLOAD_FILE_WRITE) {
          Update.write(upload.buf, upload.currentSize);
          received += upload.currentSize;

          // ✅ just update % variable
          if (total_size > 0) {
              upload_progress = (received * 100) / total_size;
          }
      }
      else if (upload.status == UPLOAD_FILE_END) {
          if (Update.end(true)) {
              Serial.println("✅ Update Success!");
              upload_progress = 100;
              upload_in_progress = false;
              delay(1000);
              fallback_notice = true;
              // 🧠 Force next boot to STA mode (telemetry/dashboard)
              prefs.begin("netmon", false);
              prefs.putBool("apmode", false);  // <- switch to normal mode
              prefs.end();

              ESP.restart();
          } else {
              Serial.printf("❌ Update Error: %s\n", Update.errorString());
              upload_in_progress = false;
          }
      }
  });







  server.on("/version", HTTP_GET, []() {
    String versionInfo = String(FW_VERSION) + " (" + FW_BUILD_DATE + " " + FW_BUILD_TIME + ") [" + GIT_COMMIT_HASH + "]";
    server.send(200, "text/plain", versionInfo);
  });

  server.begin();
}

void web_server_task(void *pvParameters) {
  while (true) {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
