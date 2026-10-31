#pragma once
#include <Arduino.h>
#include "class/hid/hid.h"
#include <vector>
struct SentReport { uint8_t id; std::vector<uint8_t> data; };
extern std::vector<SentReport> g_sent;
extern bool g_dev_ready, g_dev_mounted, g_dev_suspended;
typedef void (*set_report_cb_t)(uint8_t, hid_report_type_t, uint8_t const*, uint16_t);
struct Adafruit_USBD_HID {
  Adafruit_USBD_HID(const uint8_t*, uint16_t, uint8_t, uint8_t, bool) {}
  void setReportCallback(void*, set_report_cb_t) {}
  bool begin() { return true; }
  bool ready() { return g_dev_ready; }
  bool sendReport(uint8_t id, const void* d, uint8_t len) { g_sent.push_back({id, std::vector<uint8_t>((const uint8_t*)d, (const uint8_t*)d + len)}); return true; }
};
struct FakeDevice { bool mounted(){return g_dev_mounted;} bool suspended(){return g_dev_suspended;} void detach(){} void attach(){} bool remoteWakeup(){return true;} };
extern FakeDevice TinyUSBDevice;
struct Adafruit_USBH_Host { bool configure_pio_usb(uint8_t, const void*){return true;} bool begin(uint8_t){return true;} void task(){} };
typedef struct { uint8_t report_id; uint8_t usage; uint16_t usage_page; } tuh_hid_report_info_t;
inline uint8_t tuh_hid_interface_protocol(uint8_t, uint8_t idx) { return idx == 0 ? HID_ITF_PROTOCOL_KEYBOARD : HID_ITF_PROTOCOL_NONE; }
inline bool tuh_hid_receive_report(uint8_t, uint8_t) { return true; }
inline uint8_t tuh_hid_parse_report_descriptor(tuh_hid_report_info_t* a, uint8_t, const uint8_t*, uint16_t) { a[0] = {3, 1, HID_USAGE_PAGE_CONSUMER}; return 1; }
extern int g_led_sets;
inline bool tuh_hid_set_report(uint8_t, uint8_t, uint8_t, uint8_t, void*, uint16_t) { g_led_sets++; return true; }
#define TUD_HID_REPORT_DESC_KEYBOARD(...) 0
#define TUD_HID_REPORT_DESC_CONSUMER(...) 0
#define TUD_HID_REPORT_DESC_MOUSE(...) 0
