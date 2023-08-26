// Copyright (c) 2023 Inaba (@hollyhockberry)
// This software is released under the MIT License.
// http://opensource.org/licenses/mit-license.php

#include <string>
#include <vector>
#include <M5Unified.h>

template<typename ... Args>
std::string format(const std::string& fmt, Args ... args) {
  const auto len = std::snprintf(nullptr, 0, fmt.c_str(), args ... );
  std::vector<char> buf(len + 1);
  std::snprintf(&buf[0], len + 1, fmt.c_str(), args ... );
  return std::string(&buf[0], &buf[0] + len);
}

enum class Alignment {
  Left, Center, Right
};

void print_align(const std::string& str, M5Canvas& sprite, Alignment align) {
  const auto font = sprite.getFont();

  const auto text_width = sprite.textWidth(str.c_str(), font);
  auto width = 0;
  switch (align) {
  case Alignment::Center:
    width = (sprite.width() - text_width) / 2;
    break;
  case Alignment::Right:
    width = sprite.width() - text_width;
    break;
  case Alignment::Left:
  default:
    break;
  }
  sprite.setCursor(width, sprite.getCursorY());
  sprite.println(str.c_str());
}

bool is_normal_boot();
void sync_time();
m5::rtc_datetime_t get_datetime();

M5Canvas sprite(&M5.Display);

void update_display() {
  sprite.setCursor(0, 0);

  sprite.setTextSize(2.f);
  print_align("Button was", sprite, Alignment::Center);
  print_align("pressed at", sprite, Alignment::Center);
  sprite.setCursor(0, 180);
  print_align(format("BAT. %03d", M5.Power.getBatteryLevel()), sprite, Alignment::Right);

  const auto dt = get_datetime();
  sprite.setCursor(0, 60);
  sprite.setTextSize(3.f, 6.f);
  print_align(format("%4d/%2d/%2d", dt.date.year, dt.date.month, dt.date.date),
              sprite, Alignment::Center);
  print_align(format("%02d:%02d:%02d", dt.time.hours, dt.time.minutes, dt.time.seconds),
              sprite, Alignment::Center);

  sprite.pushSprite(0, 0);
}

void setup() {
  M5.begin();
  M5.Display.invertDisplay(true);
  sprite.createSprite(M5.Display.width(), M5.Display.height());

  if (is_normal_boot()) {
    sync_time();
  } else {
    update_display();
  }
}

#ifdef SDL_h_

bool is_normal_boot() {
  return true;
}

void sync_time() {}

m5::rtc_datetime_t get_datetime() {
  m5::rtc_datetime_t dt;
  const auto now = std::time(nullptr);
  const auto time = std::localtime(&now);
  dt.date.year = time->tm_year + 1900;
  dt.date.month = time->tm_mon + 1;
  dt.date.date = time->tm_mday;
  dt.time.hours = time->tm_hour;
  dt.time.minutes = time->tm_min;
  dt.time.seconds = time->tm_sec;
  return dt;
}

void loop() {
  M5.update();
  if (M5.BtnA.wasPressed()) {
    update_display();
  }
}

#else  // SDL_h_
#include <WiFi.h>
#include <esp_sntp.h>

bool connect_wifi() {
  WiFi.begin();
  const auto now = ::millis();
  const auto TIMEOUT = 5 * 1000UL;
  M5.Display.print("Connecting to WiFi");
  while ((::millis() - now) < TIMEOUT) {
    if (WiFi.status() == WL_CONNECTED) {
      M5.Display.println("Connected!");
      return true;
    }
    M5.Display.print(".");
    ::delay(500);
  }
  M5.Display.println();
  M5.Display.print("Waiting for SmartConfig ");

	WiFi.mode(WIFI_AP_STA);
	WiFi.beginSmartConfig();
	while (!WiFi.smartConfigDone()) {
		::delay(500);
    M5.Display.print(".");
	}
  M5.Display.println();
  M5.Display.print("Connecting to WiFi ");
  while (WiFi.status() != WL_CONNECTED) {
    M5.Display.print(".");
    ::delay(500);
  }
  M5.Display.println("Connected!");
  return true;
}

void sync_time() {
  M5.Display.clear(TFT_BLACK);
  M5.Display.setCursor(0, 0);
  M5.Display.setTextSize(2.f);
  if (!connect_wifi()) {
    return;
  }
  M5.Display.print("Sync sntp ");
  ::configTzTime("JST-9", "ntp.nict.jp", "time.google.com", "ntp.jst.mfeed.ad.jp");
  while (::sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET) {
    M5.Display.print(".");
    ::delay(1000);
  }
  struct tm localTime;
  ::getLocalTime(&localTime);
  M5.Rtc.setDateTime(&localTime);
  M5.Display.clear(TFT_BLACK);
}

bool is_normal_boot() {
  return ::esp_sleep_get_wakeup_cause() == 0;
}

m5::rtc_datetime_t get_datetime() {
  m5::rtc_datetime_t dt;
  M5.Rtc.getDateTime(&dt);
  return dt;
}

void loop() {
  ::pinMode(GPIO_NUM_33, INPUT_PULLUP);
  ::esp_sleep_enable_ext0_wakeup(GPIO_NUM_33, LOW);
  M5.Power.deepSleep(0, false);
}

#endif  // SDL_h_
