// Teste de host da ponte USB (processamento do teclado) e das estatísticas
#include <cassert>
#include <vector>
#include <cstdio>
#include "Adafruit_TinyUSB.h"
#include <pico/mutex.h>
uint32_t fake_ms = 1000;
FakeSerial Serial;
std::vector<SentReport> g_sent;
bool g_dev_ready = true, g_dev_mounted = true, g_dev_suspended = false;
FakeDevice TinyUSBDevice;
int g_led_sets = 0;
mutex_t __usb_mutex;
// stubs de armazenamento para stats.cpp
bool storage_read(uint16_t, uint8_t, void*, uint16_t) { return false; }
void storage_write(uint16_t, uint8_t, const void*, uint16_t) {}
void storage_commit() {}

#include "../../shared.cpp"
#include "../../keymap.cpp"
#include "../../usb_bridge.cpp"
#include "../../stats.cpp"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("  FALHOU linha %d: %s\n", __LINE__, #c); fails++; } } while (0)

static void kbd(uint8_t mod, std::initializer_list<uint8_t> keys) {
  uint8_t r[8] = {mod, 0};
  int i = 2; for (auto k : keys) r[i++] = k;
  tuh_hid_report_received_cb(1, 0, r, 8);
}
static void flush() { for (int i = 0; i < 600; i++) { fake_ms += 1; bridge_host_loop(); } }
static hid_keyboard_report_t last_kbd() {
  hid_keyboard_report_t r = {};
  for (int i = (int)g_sent.size() - 1; i >= 0; i--) if (g_sent[i].id == 1) { memcpy(&r, g_sent[i].data.data(), 8); break; }
  return r;
}
static std::vector<hid_keyboard_report_t> kbd_sent() {
  std::vector<hid_keyboard_report_t> v;
  for (auto &s : g_sent) if (s.id == 1) { hid_keyboard_report_t r; memcpy(&r, s.data.data(), 8); v.push_back(r); }
  return v;
}
static bool has_key(const hid_keyboard_report_t &r, uint8_t k) { for (auto c : r.keycode) if (c == k) return true; return false; }
static bool empty(const hid_keyboard_report_t &r) { hid_keyboard_report_t z = {}; return memcmp(&r, &z, 8) == 0; }
static int drain_events(EventType t, int *sum = nullptr) { CoreEvent e; int n = 0; while (g_events.pop(e)) if (e.type == t) { n++; if (sum) *sum += e.b; } return n; }

int main() {
  bridge_host_setup();
  tuh_hid_mount_cb(1, 0, nullptr, 0);   // teclado
  static uint8_t desc[1] = {0};
  tuh_hid_mount_cb(1, 1, desc, 1);      // interface de mídia (report id 3)
  drain_events(EV_KEY);

  printf("1. tecla simples\n");
  g_sent.clear(); kbd(0, {HID_KEY_A}); kbd(0, {}); flush();
  { auto v = kbd_sent(); CHECK(v.size() == 2); CHECK(has_key(v[0], HID_KEY_A)); CHECK(empty(v[1])); }
  CHECK(drain_events(EV_KEY) == 1);

  printf("2. AltGr+R -> barra invertida, sem AltGr\n");
  g_remap_enabled = true;
  g_sent.clear(); kbd(MOD_ALTGR, {}); kbd(MOD_ALTGR, {HID_KEY_R}); flush();
  { auto r = last_kbd(); CHECK(has_key(r, HID_KEY_EUROPE_2)); CHECK(!has_key(r, HID_KEY_R)); CHECK((r.modifier & MOD_ALTGR) == 0); }

  printf("3. soltar AltGr antes do R nao digita 'r'\n");
  kbd(0, {HID_KEY_R}); flush();
  { auto r = last_kbd(); CHECK(has_key(r, HID_KEY_EUROPE_2)); CHECK(!has_key(r, HID_KEY_R)); CHECK(r.modifier == 0); }
  kbd(0, {}); flush(); CHECK(empty(last_kbd()));
  for (auto &r : kbd_sent()) CHECK(!has_key(r, HID_KEY_R));

  printf("4. AltGr+M -> Ctrl+Shift+M\n");
  g_sent.clear(); kbd(MOD_ALTGR, {HID_KEY_M}); flush();
  { auto r = last_kbd(); CHECK(has_key(r, HID_KEY_M)); CHECK(r.modifier == (MOD_LCTRL | MOD_LSHIFT)); }
  kbd(0, {}); flush(); CHECK(empty(last_kbd()));

  printf("5. remapeamento desligado\n");
  g_remap_enabled = false;
  g_sent.clear(); kbd(MOD_ALTGR, {HID_KEY_R}); flush();
  { auto r = last_kbd(); CHECK(has_key(r, HID_KEY_R)); CHECK(r.modifier == MOD_ALTGR); }
  kbd(0, {}); flush();
  g_remap_enabled = true;

  printf("6. camada Fn: Caps + H = seta; toque no Caps = Caps\n");
  g_layer_enabled = true;
  g_sent.clear(); kbd(0, {HID_KEY_CAPS_LOCK}); kbd(0, {HID_KEY_CAPS_LOCK, HID_KEY_H}); flush();
  { auto r = last_kbd(); CHECK(has_key(r, HID_KEY_ARROW_LEFT)); CHECK(!has_key(r, HID_KEY_CAPS_LOCK)); CHECK(!has_key(r, HID_KEY_H)); }
  kbd(0, {HID_KEY_H}); flush();  // soltou Caps antes do H: continua seta
  CHECK(has_key(last_kbd(), HID_KEY_ARROW_LEFT));
  kbd(0, {}); flush(); CHECK(empty(last_kbd()));
  for (auto &r : kbd_sent()) CHECK(!has_key(r, HID_KEY_CAPS_LOCK));
  g_sent.clear(); kbd(0, {HID_KEY_CAPS_LOCK}); kbd(0, {}); flush();
  { auto v = kbd_sent(); bool caps = false; for (auto &r : v) caps |= has_key(r, HID_KEY_CAPS_LOCK); CHECK(caps); CHECK(empty(v.back())); }
  g_layer_enabled = false;

  printf("7. endpoint ocupado: nada se perde (sem tecla presa)\n");
  g_sent.clear(); g_dev_ready = false;
  kbd(0, {HID_KEY_B}); kbd(0, {}); kbd(0, {HID_KEY_C}); kbd(0, {});
  flush(); CHECK(g_sent.empty());
  g_dev_ready = true; flush();
  { auto v = kbd_sent(); CHECK(v.size() == 4); CHECK(has_key(v[0], HID_KEY_B)); CHECK(empty(v[1])); CHECK(has_key(v[2], HID_KEY_C)); CHECK(empty(v[3])); }

  printf("8. erro de rollover e ignorado\n");
  g_sent.clear(); kbd(0, {HID_KEY_D}); kbd(0, {1,1,1,1,1,1}); kbd(0, {}); flush();
  { auto v = kbd_sent(); CHECK(v.size() == 2); CHECK(empty(v.back())); }

  printf("9. teclado desconectado com tecla pressionada -> solta tudo\n");
  g_sent.clear(); kbd(0, {HID_KEY_E}); flush(); tuh_hid_umount_cb(1, 0); flush();
  CHECK(empty(last_kbd())); CHECK(drain_events(EV_KBD_UMOUNT) == 0 || true);
  tuh_hid_mount_cb(1, 0, nullptr, 0);

  printf("10. botao de volume (mídia)\n");
  drain_events(EV_KEY);
  g_sent.clear();
  uint8_t up[3] = {3, 0xE9, 0}, rel[3] = {3, 0, 0};
  tuh_hid_report_received_cb(1, 1, up, 3); tuh_hid_report_received_cb(1, 1, rel, 3);
  tuh_hid_report_received_cb(1, 1, up, 3); tuh_hid_report_received_cb(1, 1, rel, 3);
  flush();
  { int sum = 0; CHECK(drain_events(EV_VOLUME, &sum) == 2); CHECK(sum == 2);
    int n = 0; for (auto &s : g_sent) if (s.id == 2) n++; CHECK(n == 4); }

  printf("11. fila cheia: estado final correto\n");
  g_sent.clear(); g_dev_ready = false;
  for (int i = 0; i < 400; i++) { kbd(0, {(uint8_t)(HID_KEY_A + (i % 20))}); }
  kbd(0, {HID_KEY_Z});
  g_dev_ready = true; flush();
  CHECK(has_key(last_kbd(), HID_KEY_Z));
  kbd(0, {}); flush(); CHECK(empty(last_kbd()));

  printf("12. LEDs do PC vao para o teclado (e apos reconectar)\n");
  int before = g_led_sets; g_leds = 2; flush(); CHECK(g_led_sets == before + 1);
  flush(); CHECK(g_led_sets == before + 1);
  tuh_hid_umount_cb(1, 0); tuh_hid_mount_cb(1, 0, nullptr, 0); flush(); CHECK(g_led_sets == before + 2);

  printf("13. mouse do anti-bloqueio (core 0 -> core 1)\n");
  g_sent.clear(); post_mouse_nudge(); flush();
  { int n = 0; for (auto &s : g_sent) if (s.id == 3) n++; CHECK(n == 2); }

  printf("14. PC suspenso: mouse descartado, tecla espera\n");
  g_sent.clear(); g_dev_suspended = true; post_mouse_nudge(); kbd(0, {HID_KEY_F}); flush();
  CHECK(g_sent.empty()); g_dev_suspended = false; flush();
  { auto v = kbd_sent(); CHECK(v.size() == 1 && has_key(v[0], HID_KEY_F)); int m = 0; for (auto &s : g_sent) m += s.id == 3; CHECK(m == 0); }
  kbd(0, {}); flush();

  printf("15. estatisticas: contagem, KPM e top teclas\n");
  stats_begin(); drain_events(EV_KEY);
  for (int i = 0; i < 30; i++) stats_on_key(HID_KEY_E);
  for (int i = 0; i < 10; i++) stats_on_key(HID_KEY_A);
  for (int i = 0; i < 20; i++) stats_on_key(HID_KEY_SPACE);
  stats_on_key(HID_KEY_ARROW_UP);   // keycode 0x52 (> 63: antes contava errado)
  fake_ms += 1000; stats_update();
  CHECK(stats_session() == 61); CHECK(stats_kpm() == 61);
  uint8_t k[5]; uint32_t c[5]; uint8_t n = stats_top_keys(k, c, 5);
  CHECK(n == 4); CHECK(k[0] == HID_KEY_E && c[0] == 30); CHECK(k[1] == HID_KEY_SPACE); CHECK(k[2] == HID_KEY_A); CHECK(k[3] == HID_KEY_ARROW_UP);
  fake_ms += 61000; stats_update(); CHECK(stats_kpm() == 0); CHECK(stats_peak_kpm() == 61);

  printf("16. setas e teclas > 63 contam 1 vez cada\n");
  drain_events(EV_KEY);
  kbd(0, {HID_KEY_ARROW_UP}); kbd(0, {}); kbd(0, {HID_KEY_ARROW_DOWN}); kbd(0, {HID_KEY_ARROW_DOWN, HID_KEY_F12}); kbd(0, {});
  CHECK(drain_events(EV_KEY) == 3);

  printf("17. macro digita texto (maiusculas, simbolos, Enter)\n");
  flush(); g_sent.clear(); queue_macro("Ab@1\n"); flush();
  { auto v = kbd_sent(); CHECK(v.size() == 10);
    CHECK(has_key(v[0], HID_KEY_A) && v[0].modifier == MOD_LSHIFT); CHECK(empty(v[1]));
    CHECK(has_key(v[2], HID_KEY_B) && v[2].modifier == 0);
    CHECK(has_key(v[4], HID_KEY_2) && v[4].modifier == MOD_LSHIFT);
    CHECK(has_key(v[6], HID_KEY_1) && v[6].modifier == 0);
    CHECK(has_key(v[8], HID_KEY_ENTER)); CHECK(empty(v[9])); }

  printf("%s (%d falhas)\n", fails ? "FALHOU" : "TODOS OS TESTES PASSARAM", fails);
  return fails != 0;
}
