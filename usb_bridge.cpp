/*********************************************************************
 * usb_bridge.cpp - Ponte USB (roda no CORE 1)
 *
 *  - Recebe os reports do teclado físico (USB host via PIO).
 *  - Aplica remapeamentos / camada Fn / macros.
 *  - Envia TUDO para o PC a partir de uma única fila, neste núcleo:
 *    nenhum report é descartado se o endpoint estiver ocupado
 *    (evita tecla "presa" no PC).
 *  - Repassa os LEDs (Caps/Num/Scroll) do PC para o teclado.
 *********************************************************************/
#include <Arduino.h>
#include "pio_usb.h"  // antes do TinyUSB (ver comentário no .ino)
#include "Adafruit_TinyUSB.h"
#include "usb_bridge.h"
#include "config.h"
#include "shared.h"
#include "keymap.h"
#include <pico/mutex.h>
#include <hardware/clocks.h>

// Mutex global da pilha USB device (definido no port RP2040 do
// Adafruit TinyUSB). A tarefa USB roda numa IRQ do core 0; segurando
// este mutex o core 1 pode enviar reports sem disputar a pilha.
extern mutex_t __usb_mutex;

volatile bool g_remap_enabled = true;
volatile bool g_layer_enabled = false;

// ===================================================================
//  USB DEVICE (o que o PC enxerga)
// ===================================================================
static uint8_t const desc_hid_report[] = {
  TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(OUT_KEYBOARD)),
  TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(OUT_CONSUMER)),
  TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(OUT_MOUSE))
};
static Adafruit_USBD_HID usb_hid(desc_hid_report, sizeof(desc_hid_report), HID_ITF_PROTOCOL_KEYBOARD, 2, false);

// Chamado pela pilha USB (core 0) quando o PC muda os LEDs.
static void set_report_cb(uint8_t report_id, hid_report_type_t type, uint8_t const *buf, uint16_t size) {
  if (type != HID_REPORT_TYPE_OUTPUT || size == 0) return;
  uint8_t leds;
  if (report_id == OUT_KEYBOARD) leds = buf[0];
  else if (report_id == 0 && size >= 2 && buf[0] == OUT_KEYBOARD) leds = buf[1];
  else if (report_id == 0 && size == 1) leds = buf[0];
  else return;
  g_leds = leds;  // o core 1 compara com o último valor enviado ao teclado
}

void bridge_device_begin() {
  usb_hid.setReportCallback(NULL, set_report_cb);
  usb_hid.begin();
  // Se o PC já tinha enumerado o dispositivo, força nova enumeração
  // para ele enxergar a interface HID recém-adicionada.
  if (TinyUSBDevice.mounted()) {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  }
}

bool bridge_pc_connected() { return TinyUSBDevice.mounted(); }
bool bridge_pc_suspended() { return TinyUSBDevice.suspended(); }

// ===================================================================
//  FILA DE ENVIO (somente core 1)
// ===================================================================
#define SENDQ_SIZE 256
static OutReport sendq[SENDQ_SIZE];
static uint16_t sq_head = 0, sq_tail = 0;
static uint32_t last_send_us = 0;
static uint32_t next_gap_us = 0;
static bool kbd_resync = false;  // fila encheu: reenviar o estado atual

static uint16_t sendq_free() { return (sq_tail + SENDQ_SIZE - sq_head - 1) % SENDQ_SIZE; }

static bool sendq_push(const OutReport &r) {
  uint16_t next = (sq_head + 1) % SENDQ_SIZE;
  if (next == sq_tail) return false;
  sendq[sq_head] = r;
  sq_head = next;
  return true;
}

static void sendq_clear() { sq_tail = sq_head; }

static void queue_keyboard(const hid_keyboard_report_t &rep, uint8_t gap_ms = 0) {
  OutReport r;
  r.type = OUT_KEYBOARD;
  r.len = sizeof(hid_keyboard_report_t);
  r.gap_ms = gap_ms;
  memcpy(r.data, &rep, sizeof(hid_keyboard_report_t));
  if (!sendq_push(r)) kbd_resync = true;
}

static void queue_consumer(uint16_t usage) {
  OutReport r = { OUT_CONSUMER, 2, 0, { (uint8_t)(usage & 0xFF), (uint8_t)(usage >> 8) } };
  sendq_push(r);
}

static inline void post_event(EventType t, uint8_t a = 0, int8_t b = 0) {
  CoreEvent e = { t, a, b };
  g_events.push(e);  // se a fila estiver cheia, o evento é perdido (só estatística)
}

// ===================================================================
//  PROCESSAMENTO DO TECLADO
// ===================================================================
static uint8_t kbd_dev = 0, kbd_inst = 0;
static bool kbd_present = false;
static int16_t leds_sent = -1;    // último estado de LEDs enviado ao teclado

static uint32_t prev_bits[8];     // teclas físicas pressionadas (bitmap de 256)
static uint8_t prev_modifier = 0;
static uint8_t latch_key[256];    // tecla de saída decidida no momento do toque (0 = engolida)
static uint8_t latch_strip[256];  // modificadores removidos enquanto a tecla estiver pressionada
static uint8_t latch_add[256];    // modificadores adicionados enquanto a tecla estiver pressionada
static bool layer_held = false;
static bool layer_used = false;
static hid_keyboard_report_t last_out;
static hid_keyboard_report_t cur_out;  // último estado calculado (para reenvio)

static inline bool bit_get(const uint32_t *b, uint8_t k) { return b[k >> 5] & (1u << (k & 31)); }
static inline void bit_set(uint32_t *b, uint8_t k) { b[k >> 5] |= (1u << (k & 31)); }

static void reset_keyboard_state() {
  memset(prev_bits, 0, sizeof(prev_bits));
  prev_modifier = 0;
  layer_held = layer_used = false;
  memset(&cur_out, 0, sizeof(cur_out));
}

static void queue_macro(const char *text) {
  // Cada caractere = 2 reports. Limita ao espaço livre da fila.
  for (const char *p = text; *p; p++) {
    if (sendq_free() < 4) break;
    uint8_t mod, key;
    if (!ascii_to_hid(*p, mod, key)) continue;
    hid_keyboard_report_t press = {};
    press.modifier = mod;
    press.keycode[0] = key;
    hid_keyboard_report_t release = {};
    queue_keyboard(press, 4);
    queue_keyboard(release, 4);
  }
}

// Decide o que uma tecla recém-pressionada vai enviar enquanto estiver
// pressionada. Retorna o texto de macro a digitar (ou nullptr).
static const char *decide_latch(uint8_t k, uint8_t modifier) {
  latch_key[k] = k;
  latch_strip[k] = 0;
  latch_add[k] = 0;

  if (g_layer_enabled) {
    if (k == LAYER_KEY) {
      latch_key[k] = 0;  // a tecla da camada não vai para o PC
      layer_held = true;
      layer_used = false;
      return nullptr;
    }
    if (layer_held) {
      layer_used = true;
      for (uint8_t i = 0; i < LAYER_COUNT; i++) {
        if (LAYER_RULES[i].key == k) {
          latch_key[k] = LAYER_RULES[i].out_key;
          return nullptr;
        }
      }
      return nullptr;  // sem regra: a tecla passa normalmente
    }
  }

  if (g_remap_enabled) {
    for (uint8_t i = 0; i < MACRO_COUNT; i++) {
      const MacroRule &m = MACRO_RULES[i];
      if (m.text && m.key == k && (modifier & m.mod) == m.mod) {
        latch_key[k] = 0;
        latch_strip[k] = m.mod;
        return m.text;
      }
    }
    for (uint8_t i = 0; i < REMAP_COUNT; i++) {
      const RemapRule &r = REMAP_RULES[i];
      if (r.key == k && (modifier & r.mod) == r.mod) {
        latch_key[k] = r.out_key;
        latch_strip[k] = r.mod;
        latch_add[k] = r.out_mod;
        return nullptr;
      }
    }
  }
  return nullptr;
}

static void process_keyboard(const hid_keyboard_report_t *r) {
  // Códigos 1..3 = erro de rollover (muitas teclas). Mantém o estado anterior.
  for (int i = 0; i < 6; i++)
    if (r->keycode[i] >= 1 && r->keycode[i] <= 3) return;

  uint32_t cur[8] = { 0 };
  for (int i = 0; i < 6; i++)
    if (r->keycode[i] >= 4) bit_set(cur, r->keycode[i]);

  const char *macro = nullptr;
  bool caps_tap = false;
  bool new_key = false;

  // Teclas recém-pressionadas
  for (int i = 0; i < 6; i++) {
    uint8_t k = r->keycode[i];
    if (k < 4 || bit_get(prev_bits, k)) continue;
    new_key = true;
    post_event(EV_KEY, k);
    const char *m = decide_latch(k, r->modifier);
    if (m) macro = m;
  }

  // Teclas soltas
  for (int w = 0; w < 8; w++) {
    uint32_t released = prev_bits[w] & ~cur[w];
    while (released) {
      uint8_t k = (uint8_t)(w * 32 + __builtin_ctz(released));
      released &= released - 1;
      if (g_layer_enabled && k == LAYER_KEY) {
        layer_held = false;
        if (!layer_used) caps_tap = true;  // toque rápido: envia a tecla original
      }
    }
  }

  if (!new_key && (r->modifier != prev_modifier || memcmp(cur, prev_bits, sizeof(cur)) != 0))
    post_event(EV_ACTIVITY);

  // Monta o report de saída a partir dos "latches"
  hid_keyboard_report_t out = {};
  uint8_t strip = 0, add = 0, n = 0;
  for (int i = 0; i < 6; i++) {
    uint8_t k = r->keycode[i];
    if (k < 4) continue;
    strip |= latch_strip[k];
    add |= latch_add[k];
    uint8_t o = latch_key[k];
    if (o == 0) continue;
    bool dup = false;
    for (uint8_t j = 0; j < n; j++) dup |= (out.keycode[j] == o);
    if (!dup && n < 6) out.keycode[n++] = o;
  }
  out.modifier = (r->modifier & ~strip) | add;

  if (memcmp(&out, &last_out, sizeof(out)) != 0) {
    queue_keyboard(out);
    last_out = out;
  }
  if (caps_tap) {
    hid_keyboard_report_t tap = out;
    if (n < 6) tap.keycode[n] = LAYER_KEY;
    queue_keyboard(tap, 2);
    queue_keyboard(out, 2);
  }
  if (macro) {
    queue_macro(macro);
    queue_keyboard(out);  // restaura o estado real depois da macro
  }

  cur_out = out;
  memcpy(prev_bits, cur, sizeof(cur));
  prev_modifier = r->modifier;
}

// ===================================================================
//  OUTRAS INTERFACES HID (teclas de mídia / encoder de volume)
// ===================================================================
struct HidIface {
  bool used;
  uint8_t dev, inst, count;
  tuh_hid_report_info_t info[4];
};
static HidIface ifaces[8];
static uint16_t last_consumer = 0;

static HidIface *find_iface(uint8_t dev, uint8_t inst) {
  for (auto &f : ifaces)
    if (f.used && f.dev == dev && f.inst == inst) return &f;
  return nullptr;
}

static void handle_consumer_usage(uint16_t usage) {
  if (usage != last_consumer && usage != 0) {
    if (usage == HID_USAGE_CONSUMER_VOLUME_INCREMENT) post_event(EV_VOLUME, 0, +1);
    else if (usage == HID_USAGE_CONSUMER_VOLUME_DECREMENT) post_event(EV_VOLUME, 0, -1);
    else if (usage == HID_USAGE_CONSUMER_MUTE) post_event(EV_MUTE);
    else post_event(EV_ACTIVITY);
  }
  last_consumer = usage;
  queue_consumer(usage);
}

static void process_other_report(uint8_t dev, uint8_t inst, const uint8_t *report, uint16_t len) {
  HidIface *f = find_iface(dev, inst);
  if (f && f->count > 0) {
    uint8_t rid = 0;
    const uint8_t *data = report;
    uint16_t dlen = len;
    if (!(f->count == 1 && f->info[0].report_id == 0)) {
      rid = report[0];
      data++;
      dlen--;
    }
    for (uint8_t i = 0; i < f->count; i++) {
      if (f->info[i].report_id != rid) continue;
      if (f->info[i].usage_page == HID_USAGE_PAGE_CONSUMER && dlen >= 1) {
        uint16_t usage = data[0] | ((dlen >= 2) ? (data[1] << 8) : 0);
        handle_consumer_usage(usage);
      }
      return;
    }
    return;
  }
  // Descritor não disponível: formato usado pelo teclado original (ID 3)
  if (len == 3 && report[0] == 0x03) handle_consumer_usage(report[1] | (report[2] << 8));
}

// ===================================================================
//  CALLBACKS DO TINYUSB HOST (executam no core 1, dentro de USBHost.task)
// ===================================================================
Adafruit_USBH_Host USBHost;

extern "C" {

void tuh_hid_mount_cb(uint8_t dev, uint8_t inst, uint8_t const *desc, uint16_t desc_len) {
  uint8_t proto = tuh_hid_interface_protocol(dev, inst);
  if (proto == HID_ITF_PROTOCOL_KEYBOARD && !kbd_present) {
    kbd_dev = dev;
    kbd_inst = inst;
    kbd_present = true;
    reset_keyboard_state();
    g_keyboard_connected = true;
    leds_sent = -1;  // reaplica Caps/Num Lock no teclado novo
    post_event(EV_KBD_MOUNT);
    LOG("Teclado montado: addr %d inst %d\n", dev, inst);
  } else if (proto != HID_ITF_PROTOCOL_KEYBOARD) {
    for (auto &f : ifaces) {
      if (f.used) continue;
      f.used = true;
      f.dev = dev;
      f.inst = inst;
      f.count = desc ? tuh_hid_parse_report_descriptor(f.info, 4, desc, desc_len) : 0;
      break;
    }
  }
  tuh_hid_receive_report(dev, inst);
}

void tuh_hid_umount_cb(uint8_t dev, uint8_t inst) {
  if (kbd_present && dev == kbd_dev && inst == kbd_inst) {
    kbd_present = false;
    g_keyboard_connected = false;
    reset_keyboard_state();
    hid_keyboard_report_t none = {};
    if (memcmp(&last_out, &none, sizeof(none)) != 0) {
      queue_keyboard(none);  // solta tudo no PC
      last_out = none;
    }
    post_event(EV_KBD_UMOUNT);
  }
  HidIface *f = find_iface(dev, inst);
  if (f) {
    f->used = false;
    if (last_consumer) {
      queue_consumer(0);
      last_consumer = 0;
    }
  }
}

void tuh_hid_report_received_cb(uint8_t dev, uint8_t inst, uint8_t const *report, uint16_t len) {
  if (kbd_present && dev == kbd_dev && inst == kbd_inst) {
    if (len >= sizeof(hid_keyboard_report_t))
      process_keyboard((const hid_keyboard_report_t *)report);
  } else {
    process_other_report(dev, inst, report, len);
  }
  tuh_hid_receive_report(dev, inst);
}

}  // extern "C"

// ===================================================================
//  CORE 1: setup / loop
// ===================================================================
static bool host_ok = false;

void bridge_host_setup() {
  uint32_t cpu_hz = clock_get_hz(clk_sys);
  if (cpu_hz % 12000000UL) {
    // PIO-USB precisa de clock múltiplo de 12 MHz (120, 240 MHz...)
    post_event(EV_CLOCK_ERROR);
    return;
  }
#if PIN_5V_EN >= 0
  pinMode(PIN_5V_EN, OUTPUT);
  digitalWrite(PIN_5V_EN, PIN_5V_EN_STATE);
#endif
  pio_usb_configuration_t pio_cfg = PIO_USB_DEFAULT_CONFIG;
  pio_cfg.pin_dp = PIN_USB_HOST_DP;
  USBHost.configure_pio_usb(1, &pio_cfg);
  USBHost.begin(1);
  host_ok = true;
}

static void flush_send_queue() {
  // Puxa o que o core 0 pediu (mouse, mídia, sincronização de volume)
  OutReport r;
  while (sendq_free() > 8 && g_outbox.pop(r)) sendq_push(r);

  if (kbd_resync && sendq_free() > 2) {
    queue_keyboard(cur_out);
    last_out = cur_out;
    kbd_resync = false;
  }

  if (sq_head == sq_tail) return;

  if (!TinyUSBDevice.mounted()) {
    sendq_clear();  // PC desligado: não acumula
    return;
  }
  if (TinyUSBDevice.suspended()) {
    // PC dormindo: mouse/mídia são descartados (não devem acordar o PC);
    // uma tecla pede para o PC acordar e espera na fila.
    uint16_t w = sq_tail;
    bool any_key = false;
    for (uint16_t i = sq_tail; i != sq_head; i = (i + 1) % SENDQ_SIZE) {
      if (sendq[i].type != OUT_KEYBOARD) continue;
      sendq[w] = sendq[i];
      w = (w + 1) % SENDQ_SIZE;
      any_key = true;
    }
    sq_head = w;
    static uint32_t last_wakeup = 0;
    if (any_key && millis() - last_wakeup > 100) {
      last_wakeup = millis();
      TinyUSBDevice.remoteWakeup();
    }
    return;
  }
  if ((uint32_t)(micros() - last_send_us) < next_gap_us) return;

  const OutReport &o = sendq[sq_tail];
  bool sent = false;
  mutex_enter_blocking(&__usb_mutex);
  if (usb_hid.ready()) sent = usb_hid.sendReport(o.type, o.data, o.len);
  mutex_exit(&__usb_mutex);

  if (sent) {
    last_send_us = micros();
    next_gap_us = (uint32_t)o.gap_ms * 1000u;
    sq_tail = (sq_tail + 1) % SENDQ_SIZE;
  }
}

void bridge_host_loop() {
  g_core1_heartbeat++;
  if (!host_ok) {
    delay(10);
    return;
  }
  USBHost.task();

  if (kbd_present && g_leds != leds_sent) {
    static uint8_t led_buf;  // precisa existir até a transferência terminar
    led_buf = g_leds;
    if (tuh_hid_set_report(kbd_dev, kbd_inst, 0, HID_REPORT_TYPE_OUTPUT, &led_buf, 1))
      leds_sent = led_buf;
  }

  flush_send_queue();
}
