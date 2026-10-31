/*********************************************************************
 * app.cpp - Lógica principal (CORE 0)
 *
 * Junta tudo: eventos do teclado (vindos do core 1), joystick, menu,
 * screensavers, anti-bloqueio, Pomodoro, lembrete de pausa, brilho
 * e watchdog.
 *********************************************************************/
#include "app.h"
#include "config.h"
#include "shared.h"
#include "usb_bridge.h"
#include "settings.h"
#include "stats.h"
#include "clock.h"
#include "pomodoro.h"
#include "buttons.h"
#include "display.h"
#include "screensavers.h"
#include "menu.h"
#include "ui.h"
#include "Adafruit_TinyUSB.h"

enum Mode : uint8_t { MODE_MONITOR, MODE_SCREENSAVER };

static Mode mode = MODE_MONITOR;
static uint8_t page = PAGE_MAIN;
static MonitorState mon = { 70, false, false, false, 0 };

static uint32_t last_activity = 0;      // teclado OU joystick
static uint32_t last_kbd_activity = 0;  // só teclado (anti-bloqueio)
static uint32_t last_antilock = 0;
static uint32_t last_draw = 0;
static bool redraw_now = true;
static bool clock_error = false;

static uint8_t ss_current = 0;
static uint8_t ss_rotation = 0;
static uint32_t ss_started = 0;
static uint32_t ss_last_frame = 0;

static bool vol_sync_done = false;
static uint32_t pc_seen_since = 0;
static uint32_t pc_lost_since = 0;
static uint32_t break_snooze_until = 0;
static bool applied_flip = false;

static uint32_t hb_last = 0, hb_changed_at = 0;

// ===================================================================
//  SCREENSAVER
// ===================================================================
static void ss_start(uint8_t index, uint32_t now) {
  ss_current = index % ss_count();
  color_lut_build(settings.ss_color);
  ss_get(ss_current).init();
  ss_started = now;
  ss_last_frame = 0;
}

static void enter_screensaver(uint32_t now) {
  mode = MODE_SCREENSAVER;
  if (settings.ss_type >= ss_count()) ss_start(ss_rotation, now);  // Todos
  else ss_start(settings.ss_type, now);
}

static void exit_screensaver() {
  if (mode != MODE_SCREENSAVER) return;
  mode = MODE_MONITOR;
  color_lut_build(100);
  redraw_now = true;
}

// ===================================================================
//  ATIVIDADE / EVENTOS
// ===================================================================
static void on_activity(bool from_keyboard, uint32_t now) {
  last_activity = now;
  if (from_keyboard) last_kbd_activity = now;
  exit_screensaver();
}

static void process_core_events(uint32_t now) {
  CoreEvent e;
  while (g_events.pop(e)) {
    switch (e.type) {
      case EV_KEY:
        stats_on_key(e.a);
        on_activity(true, now);
        break;
      case EV_ACTIVITY:
        on_activity(true, now);
        break;
      case EV_VOLUME:
        mon.volume = constrain(mon.volume + e.b * 2, 0, 100);  // Windows: 2% por passo
        mon.muted = false;
        on_activity(true, now);
        break;
      case EV_MUTE:
        mon.muted = !mon.muted;
        on_activity(true, now);
        break;
      case EV_KBD_MOUNT:
        ui_toast("Teclado conectado");
        on_activity(true, now);
        break;
      case EV_KBD_UMOUNT:
        ui_toast("Teclado desconectado", 2500);
        on_activity(false, now);
        break;
      case EV_CLOCK_ERROR:
        clock_error = true;
        break;
    }
  }
}

static void process_buttons(uint32_t now) {
  bool dimmed = settings.dim_after_s > 0 && now - last_activity > (uint32_t)settings.dim_after_s * 1000UL;
  BtnEvent ev;
  while (buttons_next(ev)) {
    last_activity = now;
    redraw_now = true;

    if (ui_alert_active()) {
      if (!ev.repeat) ui_alert_dismiss();
      continue;
    }
    if (menu_is_open()) {
      menu_handle(ev);
      continue;
    }
    if (mode == MODE_SCREENSAVER) {  // o primeiro toque só acorda
      exit_screensaver();
      continue;
    }
    if (dimmed) {
      dimmed = false;
      continue;
    }
    if (ev.repeat) continue;

    switch (ev.btn) {
      case BTN_OK: menu_open(); break;
      case BTN_LEFT:
        page = (page + PAGE_COUNT - 1) % PAGE_COUNT;
        ui_toast(ui_page_name(page), 800);
        break;
      case BTN_RIGHT:
        page = (page + 1) % PAGE_COUNT;
        ui_toast(ui_page_name(page), 800);
        break;
      case BTN_UP:
        if (page == PAGE_POMO) pomo_start_pause();
        break;
      case BTN_DOWN:
        if (page == PAGE_POMO) pomo_reset();
        break;
      default: break;
    }
  }
}

// ===================================================================
//  RECURSOS
// ===================================================================
static void apply_settings() {
  g_remap_enabled = settings.remap;
  g_layer_enabled = settings.layer;
  if (settings.flip != applied_flip) {
    applied_flip = settings.flip;
    display_set_flip(applied_flip);
  }
}

static void update_antilock(uint32_t now) {
  if (!settings.antilock || !bridge_pc_connected() || bridge_pc_suspended()) return;
  uint32_t interval = (uint32_t)settings.antilock_s * 1000UL;
  if (now - last_kbd_activity >= interval && now - last_antilock >= interval) {
    post_mouse_nudge();
    last_antilock = now;
  }
}

// Leva o volume do PC a um valor conhecido (o PC não informa o volume
// por HID): baixa tudo e sobe até o nível configurado.
static void update_volume_sync(uint32_t now) {
  if (!bridge_pc_connected()) {
    pc_seen_since = 0;
    if (pc_lost_since == 0) pc_lost_since = now;
    if (now - pc_lost_since > 3000) vol_sync_done = false;  // PC reiniciou: sincroniza de novo
    return;
  }
  pc_lost_since = 0;
  if (vol_sync_done || !settings.vol_sync || bridge_pc_suspended()) return;
  if (pc_seen_since == 0) pc_seen_since = now;
  if (now - pc_seen_since < 1500) return;
  if (g_outbox.freeSpace() < 180) return;

  for (int i = 0; i < 50; i++) post_consumer_click(HID_USAGE_CONSUMER_VOLUME_DECREMENT);
  int steps = settings.vol_sync_level / 2;
  for (int i = 0; i < steps; i++) post_consumer_click(HID_USAGE_CONSUMER_VOLUME_INCREMENT);
  mon.volume = steps * 2;
  mon.muted = false;
  vol_sync_done = true;
}

static void update_productivity(uint32_t now) {
  switch (pomo_update()) {
    case POMO_EV_FOCUS_DONE: {
      char l2[32];
      snprintf(l2, sizeof(l2), "Pausa de %d min", settings.pomo_break_min);
      ui_alert("POMODORO", "Fim do foco!", l2, TFT_GREEN, 15000);
      exit_screensaver();
      break;
    }
    case POMO_EV_BREAK_DONE:
      ui_alert("POMODORO", "Fim da pausa.", "Hora de focar!", TFT_RED, 15000);
      exit_screensaver();
      break;
    default: break;
  }

  if (settings.break_remind_min > 0) {
    uint32_t streak = stats_streak_seconds();
    if (streak == 0) break_snooze_until = 0;
    else if (streak >= (uint32_t)settings.break_remind_min * 60UL && (int32_t)(now - break_snooze_until) >= 0) {
      char l1[32];
      snprintf(l1, sizeof(l1), "Digitando ha %lu min", (unsigned long)(streak / 60));
      ui_alert("PAUSA", l1, "Levante e alongue-se :)", TFT_ORANGE, 20000);
      break_snooze_until = now + 10UL * 60UL * 1000UL;  // lembra de novo em 10 min
    }
  }
#if PIN_BUZZER >= 0
  static bool beeped = false;
  if (ui_alert_active() && !beeped) {
    tone(PIN_BUZZER, 2000, 150);
    beeped = true;
  } else if (!ui_alert_active()) {
    beeped = false;
  }
#endif
}

static void update_backlight(uint32_t now) {
  uint8_t level = settings.brightness;
  bool idle = settings.dim_after_s > 0 && now - last_activity > (uint32_t)settings.dim_after_s * 1000UL;
  if (idle && mode == MODE_MONITOR && !menu_is_open() && !ui_alert_active())
    level = min<int>(settings.dim_brightness, settings.brightness);
  display_set_backlight(level);
}

static void update_watchdog(uint32_t now) {
  // Só "alimenta" o watchdog se o core 1 também estiver vivo
  uint32_t hb = g_core1_heartbeat;
  if (hb != hb_last) {
    hb_last = hb;
    hb_changed_at = now;
  }
  if (now - hb_changed_at < 2000) rp2040.wdt_reset();
}

// ===================================================================
//  DESENHO
// ===================================================================
static void render(uint32_t now) {
  if (clock_error) {
    if (now - last_draw < 1000) return;
    ui_draw_error("CPU precisa ser multiplo", "de 12 MHz para o USB host.", "Ferramentas > CPU: 120MHz");
    display_push();
    last_draw = now;
    return;
  }

  if (menu_is_open()) {
    if (!redraw_now && now - last_draw < 500) return;
    menu_draw();
  } else if (mode == MODE_SCREENSAVER) {
    const Screensaver &ss = ss_get(ss_current);
    uint32_t interval = max<uint32_t>(10, (uint32_t)ss.frame_ms * 10 / settings.ss_speed);
    if (now - ss_last_frame < interval) return;
    ss_last_frame = now;
    ss.frame();
  } else {
    if (!redraw_now && now - last_draw < 100) return;  // ~10 quadros/s
    mon.keyboard = g_keyboard_connected;
    mon.pc = bridge_pc_connected();
    mon.leds = g_leds;
    ui_draw_page(page, mon);
  }
  ui_draw_overlays();
  display_push();
  last_draw = now;
  redraw_now = false;
}

// ===================================================================
//  AÇÕES DO MENU
// ===================================================================
static void save_everything() {
  settings_save_if_dirty();
  stats_save_if_needed(true);
}

void action_pomo_start_pause() {
  pomo_start_pause();
  ui_toast(pomo_paused() ? "Pomodoro pausado" : "Pomodoro rodando");
}

void action_pomo_reset() {
  pomo_reset();
  ui_toast("Pomodoro zerado");
}

void action_reset_session() {
  stats_reset_session();
  ui_toast("Sessao zerada");
}

void action_reset_all_stats() {
  stats_reset_all();
  ui_toast("Estatisticas zeradas");
}

void action_about() {
  char l1[32], l2[32];
  snprintf(l1, sizeof(l1), "CPU %lu MHz  RTC: %s", (unsigned long)(rp2040.f_cpu() / 1000000), clock_has_rtc() ? "sim" : "nao");
  snprintf(l2, sizeof(l2), "Teclado: %s", g_keyboard_connected ? "conectado" : "ausente");
  ui_alert("KBD MONITOR " FW_VERSION, l1, l2, TFT_CYAN, 6000);
}

void action_reboot() {
  save_everything();
  ui_draw_error("Reiniciando...", "", "");
  display_push();
  delay(300);
  rp2040.reboot();
}

void action_factory_defaults() {
  settings_defaults(settings);
  settings_mark_dirty();
  settings_save_if_dirty();
  ui_toast("Padroes restaurados");
}

void action_bootloader() {
  save_everything();
  ui_draw_error("Modo gravacao (BOOTSEL)", "Copie o arquivo .uf2 para", "a unidade RPI-RP2");
  display_push();
  delay(300);
  rp2040.rebootToBootloader();
}

// ===================================================================
//  SETUP / LOOP
// ===================================================================
void app_setup() {
  bridge_device_begin();
  settings_load();
  display_begin(settings.flip);
  applied_flip = settings.flip;
  display_set_backlight(settings.brightness);
  ui_draw_splash();
  display_push();

  buttons_begin();
  clock_begin();
  stats_begin();
  randomSeed(rp2040.hwrand32());
  apply_settings();

  uint32_t now = millis();
  last_activity = last_kbd_activity = now;
  settings_save_if_dirty();  // grava os padrões na 1ª execução
  rp2040.wdt_begin(5000);
}

void app_loop() {
  uint32_t now = millis();
  update_watchdog(now);

  process_core_events(now);
  buttons_update();
  bool menu_was_open = menu_is_open();
  process_buttons(now);
  menu_update();
  if (menu_was_open && !menu_is_open()) {
    settings_save_if_dirty();  // grava ao sair do menu (e só se algo mudou)
    redraw_now = true;
  }

  apply_settings();
  clock_update();
  if (clock_day_changed()) stats_new_day();
  stats_update();
  update_productivity(now);
  update_antilock(now);
  update_volume_sync(now);

  // Screensaver por inatividade / rotação no modo "Todos"
  if (mode == MODE_MONITOR && !menu_is_open() && !ui_alert_active() && settings.ss_start_min > 0 &&
      now - last_activity >= (uint32_t)settings.ss_start_min * 60000UL) {
    enter_screensaver(now);
  }
  if (mode == MODE_SCREENSAVER && settings.ss_type >= ss_count() &&
      now - ss_started >= (uint32_t)settings.ss_switch_s * 1000UL) {
    ss_rotation = (ss_rotation + 1) % ss_count();
    ss_start(ss_rotation, now);
  }

  update_backlight(now);
  render(now);
}
