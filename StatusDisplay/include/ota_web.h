#pragma once
#include "version.h"
#include <WebServer.h>
#include "globals.h"
#include <lvgl.h>

extern WebServer server;

void start_upload_webserver();
void web_server_task(void *pvParameters);
