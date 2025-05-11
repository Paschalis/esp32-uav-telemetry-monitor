#pragma once
#include <Preferences.h>

extern Preferences prefs;
extern volatile bool upload_in_progress;
extern volatile int upload_progress;
extern volatile unsigned long upload_start_time;