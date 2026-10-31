#include "shared.h"

SpscQueue<CoreEvent, 128> g_events;
SpscQueue<OutReport, 256> g_outbox;

volatile uint8_t g_leds = 0;
volatile bool g_keyboard_connected = false;
volatile uint32_t g_core1_heartbeat = 0;

void post_consumer_click(uint16_t usage) {
  if (g_outbox.freeSpace() < 2) return;
  OutReport r = { OUT_CONSUMER, 2, 8, { (uint8_t)(usage & 0xFF), (uint8_t)(usage >> 8) } };
  g_outbox.push(r);
  OutReport rel = { OUT_CONSUMER, 2, 8, { 0, 0 } };
  g_outbox.push(rel);
}

void post_mouse_nudge() {
  if (g_outbox.freeSpace() < 2) return;
  // buttons, x, y, wheel, pan
  OutReport right = { OUT_MOUSE, 5, 8, { 0, 1, 0, 0, 0 } };
  OutReport left = { OUT_MOUSE, 5, 8, { 0, (uint8_t)-1, 0, 0, 0 } };
  g_outbox.push(right);
  g_outbox.push(left);
}
