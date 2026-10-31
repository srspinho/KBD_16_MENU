#include "clock.h"
#include "config.h"

#if RTC_ENABLED
#include <Wire.h>
#define DS3231_ADDR 0x68
#endif

static bool is_set = false;
static bool has_rtc = false;
static uint32_t base_sod = 0;   // segundos do dia no instante base_ms
static uint32_t base_ms = 0;
static uint32_t last_sod = 0;
static bool day_changed = false;
static uint32_t last_rtc_sync = 0;

static uint8_t bcd2bin(uint8_t v) { return (v & 0x0F) + (v >> 4) * 10; }
static uint8_t bin2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

#if RTC_ENABLED
static bool rtc_read(uint8_t &h, uint8_t &m, uint8_t &s) {
  // Registrador 0x0F bit 7 (OSF): oscilador parou -> hora inválida
  Wire1.beginTransmission(DS3231_ADDR);
  Wire1.write(0x0F);
  if (Wire1.endTransmission(false) != 0) return false;
  if (Wire1.requestFrom(DS3231_ADDR, 1) != 1) return false;
  if (Wire1.read() & 0x80) return false;

  Wire1.beginTransmission(DS3231_ADDR);
  Wire1.write(0x00);
  if (Wire1.endTransmission(false) != 0) return false;
  if (Wire1.requestFrom(DS3231_ADDR, 3) != 3) return false;
  s = bcd2bin(Wire1.read() & 0x7F);
  m = bcd2bin(Wire1.read() & 0x7F);
  uint8_t hr = Wire1.read();
  if (hr & 0x40) {  // modo 12h
    h = bcd2bin(hr & 0x1F) % 12;
    if (hr & 0x20) h += 12;
  } else {
    h = bcd2bin(hr & 0x3F);
  }
  return h < 24 && m < 60 && s < 60;
}

static void rtc_write(uint8_t h, uint8_t m, uint8_t s) {
  Wire1.beginTransmission(DS3231_ADDR);
  Wire1.write(0x00);
  Wire1.write(bin2bcd(s));
  Wire1.write(bin2bcd(m));
  Wire1.write(bin2bcd(h));  // modo 24h
  Wire1.endTransmission();
  // Limpa o flag OSF
  Wire1.beginTransmission(DS3231_ADDR);
  Wire1.write(0x0F);
  Wire1.write(0x00);
  Wire1.endTransmission();
}
#endif

static void set_base(uint32_t sod) {
  base_sod = sod % 86400UL;
  base_ms = millis();
  last_sod = base_sod;
  is_set = true;
}

void clock_begin() {
#if RTC_ENABLED
  Wire1.setSDA(PIN_RTC_SDA);
  Wire1.setSCL(PIN_RTC_SCL);
  Wire1.setTimeout(20);
  Wire1.begin();
  Wire1.beginTransmission(DS3231_ADDR);
  has_rtc = (Wire1.endTransmission() == 0);
  if (has_rtc) {
    uint8_t h, m, s;
    if (rtc_read(h, m, s)) set_base(h * 3600UL + m * 60UL + s);
    last_rtc_sync = millis();
  } else {
    Wire1.end();
  }
#endif
}

uint32_t clock_seconds_of_day() {
  if (!is_set) return 0;
  return (base_sod + (millis() - base_ms) / 1000UL) % 86400UL;
}

void clock_update() {
  if (!is_set) return;
  uint32_t now = millis();
  // Rebase diário: evita o estouro do millis() (49 dias)
  if (now - base_ms > 86400000UL) set_base(clock_seconds_of_day());

  uint32_t sod = clock_seconds_of_day();
  if (sod < last_sod) day_changed = true;
  last_sod = sod;

#if RTC_ENABLED
  // Ressincroniza com o RTC a cada hora (o cristal do RP2040 deriva ~2 s/dia)
  if (has_rtc && now - last_rtc_sync > 3600000UL) {
    last_rtc_sync = now;
    uint8_t h, m, s;
    if (rtc_read(h, m, s)) {
      uint32_t rtc_sod = h * 3600UL + m * 60UL + s;
      bool wrapped = rtc_sod < last_sod;
      set_base(rtc_sod);
      if (wrapped && last_sod > 86000UL) day_changed = true;
    }
  }
#endif
}

bool clock_is_set() { return is_set; }
bool clock_has_rtc() { return has_rtc; }

void clock_get(uint8_t &h, uint8_t &m, uint8_t &s) {
  uint32_t sod = clock_seconds_of_day();
  h = sod / 3600;
  m = (sod / 60) % 60;
  s = sod % 60;
}

void clock_set(uint8_t h, uint8_t m, uint8_t s) {
  set_base(h * 3600UL + m * 60UL + s);
#if RTC_ENABLED
  if (has_rtc) rtc_write(h, m, s);
#endif
}

bool clock_day_changed() {
  bool d = day_changed;
  day_changed = false;
  return d;
}

void clock_format(char *buf, size_t n, bool blink_colon) {
  if (!is_set) {
    snprintf(buf, n, "--:--");
    return;
  }
  uint8_t h, m, s;
  clock_get(h, m, s);
  char sep = (blink_colon && (s & 1)) ? ' ' : ':';
  snprintf(buf, n, "%02u%c%02u", h, sep, m);
}
