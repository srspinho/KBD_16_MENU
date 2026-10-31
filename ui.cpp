#include "ui.h"
#include "display.h"
#include "config.h"
#include "stats.h"
#include "clock.h"
#include "pomodoro.h"
#include "keymap.h"
#include "screensavers.h"
#include "RobotoMono12pt7b.h"
#include "Orbitron_Bold6pt7b.h"

// ===================================================================
//  Utilitários de texto
// ===================================================================
static void font_small() {
  canvas.setFreeFont(NULL);
  canvas.setTextFont(1);
  canvas.setTextSize(1);
  canvas.setTextDatum(TL_DATUM);
}
static void font_title() {
  canvas.setFreeFont(&Orbitron_Bold6pt7b);
  canvas.setTextSize(1);
  canvas.setTextDatum(TL_DATUM);
}
static void font_big() {
  canvas.setFreeFont(&RobotoMono_VariableFont_wght12pt7b);
  canvas.setTextSize(1);
  canvas.setTextDatum(TC_DATUM);
}

// 1234567 -> "1.234.567"
static void fmt_thousands(uint32_t v, char *out, size_t n) {
  char tmp[16];
  snprintf(tmp, sizeof(tmp), "%lu", (unsigned long)v);
  int len = strlen(tmp), j = 0;
  for (int i = 0; i < len && j < (int)n - 1; i++) {
    out[j++] = tmp[i];
    int rest = len - i - 1;
    if (rest > 0 && rest % 3 == 0 && j < (int)n - 1) out[j++] = '.';
  }
  out[j] = 0;
}

// 12345 -> "12.3k"
static void fmt_short(uint32_t v, char *out, size_t n) {
  if (v < 10000) snprintf(out, n, "%lu", (unsigned long)v);
  else if (v < 1000000) snprintf(out, n, "%lu.%luk", (unsigned long)(v / 1000), (unsigned long)((v % 1000) / 100));
  else snprintf(out, n, "%lu.%luM", (unsigned long)(v / 1000000), (unsigned long)((v % 1000000) / 100000));
}

static void fmt_duration(uint32_t s, char *out, size_t n) {
  if (s >= 3600) snprintf(out, n, "%luh%02lum", (unsigned long)(s / 3600), (unsigned long)((s / 60) % 60));
  else snprintf(out, n, "%lum%02lus", (unsigned long)(s / 60), (unsigned long)(s % 60));
}

static void title_bar(const char *title, uint16_t color = TFT_WHITE) {
  font_title();
  canvas.setTextColor(color);
  canvas.drawString(title, 0, 0);
  canvas.drawFastHLine(0, 13, SCREEN_W, TFT_DARKGREY);
}

static void label_value(int y, const char *label, const char *value, uint16_t vcolor = TFT_WHITE) {
  font_small();
  canvas.setTextColor(TFT_LIGHTGREY);
  canvas.drawString(label, 4, y);
  canvas.setTextColor(vcolor);
  canvas.setTextDatum(TR_DATUM);
  canvas.drawString(value, SCREEN_W - 4, y);
  canvas.setTextDatum(TL_DATUM);
}

// ===================================================================
//  PÁGINA PRINCIPAL
// ===================================================================
static void draw_led(int x, const char *ch, bool on) {
  if (on) canvas.fillRoundRect(x, 1, 9, 10, 2, TFT_GREEN);
  else canvas.drawRoundRect(x, 1, 9, 10, 2, TFT_DARKGREY);
  font_small();
  canvas.setTextColor(on ? TFT_BLACK : TFT_DARKGREY);
  canvas.drawString(ch, x + 2, 2);
}

static void draw_graph(int x, int y, int w, int h) {
  canvas.fillRect(x + 1, y + 1, w - 2, h - 2, tft.color565(24, 24, 24));
  canvas.drawRect(x, y, w, h, TFT_WHITE);
  const uint16_t *hist = stats_history();
  uint16_t peak = 8;  // escala automática (mínimo 8 teclas/s)
  for (int i = 0; i < STATS_HISTORY; i++) peak = max(peak, hist[i]);
  int gh = h - 4;
  int n = min(STATS_HISTORY, w - 4);
  int prev_y = 0;
  for (int i = 0; i < n; i++) {
    uint16_t v = hist[STATS_HISTORY - n + i];
    int py = y + h - 2 - (int)((uint32_t)v * gh / peak);
    if (i > 0) canvas.drawLine(x + 1 + i, prev_y, x + 2 + i, py, TFT_GREEN);
    prev_y = py;
  }
}

static void page_main(const MonitorState &st) {
  // Barra de título: nome / alerta de teclado + LEDs + hora
  if (!st.keyboard) {
    font_title();
    canvas.setTextColor((millis() / 500) % 2 ? TFT_RED : TFT_ORANGE);
    canvas.drawString("SEM TECLADO", 0, 0);
  } else {
    title_bar("MONITOR");
  }
  canvas.drawFastHLine(0, 13, SCREEN_W, TFT_DARKGREY);
  int x = SCREEN_W - 1;
  if (clock_is_set()) {
    char hb[8];
    clock_format(hb, sizeof(hb), true);
    font_title();
    canvas.setTextColor(TFT_CYAN);
    canvas.setTextDatum(TR_DATUM);
    canvas.drawString(hb, x, 0);
    canvas.setTextDatum(TL_DATUM);
    x -= 44;
  }
  draw_led(x - 10, "A", st.leds & KEYBOARD_LED_CAPSLOCK);
  draw_led(x - 21, "1", st.leds & KEYBOARD_LED_NUMLOCK);

  // Progresso do Pomodoro logo abaixo do título
  if (pomo_phase() != POMO_OFF && pomo_phase_total_s() > 0) {
    uint32_t done = pomo_phase_total_s() - pomo_remaining_s();
    int w = (int)(done * SCREEN_W / pomo_phase_total_s());
    uint16_t c = pomo_phase() == POMO_FOCUS ? TFT_RED : TFT_GREEN;
    if (pomo_paused()) c = TFT_YELLOW;
    canvas.fillRect(0, 14, w, 2, c);
  }

  // Contador grande
  uint32_t count = clock_is_set() ? stats_today() : stats_session();
  char buf[16];
  fmt_thousands(count, buf, sizeof(buf));
  uint16_t color = (count < 1000) ? TFT_GREEN : (count < 5000) ? TFT_YELLOW : TFT_RED;
  font_big();
  canvas.setTextColor(color);
  canvas.drawString(buf, SCREEN_W / 2, 19);

  font_small();
  canvas.setTextColor(TFT_DARKGREY);
  char sub[32];
  snprintf(sub, sizeof(sub), "%s  %u KPM", clock_is_set() ? "teclas hoje" : "teclas sessao", stats_kpm());
  canvas.setTextDatum(TC_DATUM);
  canvas.drawString(sub, SCREEN_W / 2, 43);
  canvas.setTextDatum(TL_DATUM);

  // Gráfico dos últimos 60 s
  draw_graph(4, 54, 74, 24);

  // Volume
  font_title();
  if (st.muted) {
    canvas.setTextColor(TFT_RED);
    canvas.drawString("MUDO", 86, 54);
  } else {
    canvas.setTextColor(TFT_YELLOW);
    canvas.drawString("VOLUME", 86, 54);
  }
  canvas.drawRect(85, 69, 71, 9, TFT_WHITE);
  int bar = map(st.volume, 0, 100, 0, 67);
  uint16_t vc = st.muted ? TFT_DARKGREY : (st.volume < 50 ? TFT_BLUE : TFT_GREEN);
  canvas.fillRect(87, 71, bar, 5, vc);
}

// ===================================================================
//  ESTATÍSTICAS
// ===================================================================
static void page_stats() {
  title_bar("ESTATISTICAS");
  char a[20], b[20], line[32];
  fmt_thousands(stats_session(), a, sizeof(a));
  label_value(17, "Sessao", a);
  if (clock_is_set()) {
    fmt_thousands(stats_today(), a, sizeof(a));
    label_value(27, "Hoje", a);
  } else {
    label_value(27, "Hoje", "(ajuste a hora)", TFT_DARKGREY);
  }
  fmt_thousands(stats_total(), a, sizeof(a));
  label_value(37, "Total", a, TFT_CYAN);
  snprintf(line, sizeof(line), "%u / %u", stats_kpm(), stats_peak_kpm());
  label_value(47, "KPM / pico", line, TFT_YELLOW);
  fmt_duration(stats_active_seconds(), a, sizeof(a));
  label_value(57, "Digitando", a);
  fmt_duration(millis() / 1000, b, sizeof(b));
  label_value(67, "Ligado ha", b, TFT_DARKGREY);
}

// ===================================================================
//  TOP TECLAS
// ===================================================================
static void page_top() {
  title_bar("TOP TECLAS");
  uint8_t keys[5];
  uint32_t counts[5];
  uint8_t n = stats_top_keys(keys, counts, 5);
  font_small();
  if (n == 0) {
    canvas.setTextColor(TFT_DARKGREY);
    canvas.drawString("Nenhuma tecla ainda", 4, 36);
    return;
  }
  for (uint8_t i = 0; i < n; i++) {
    int y = 17 + i * 12;
    char nb[8], cb[12];
    const char *name = key_name(keys[i], nb, sizeof(nb));
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString(name, 4, y);
    int w = (int)((uint64_t)counts[i] * 78 / counts[0]);
    canvas.fillRect(44, y, max(1, w), 7, rainbow((uint8_t)(i * 40)));
    fmt_short(counts[i], cb, sizeof(cb));
    canvas.setTextColor(TFT_LIGHTGREY);
    canvas.setTextDatum(TR_DATUM);
    canvas.drawString(cb, SCREEN_W - 2, y);
    canvas.setTextDatum(TL_DATUM);
  }
}

// ===================================================================
//  POMODORO
// ===================================================================
static void page_pomo() {
  const char *state;
  uint16_t color;
  switch (pomo_phase()) {
    case POMO_FOCUS: state = "FOCO"; color = TFT_RED; break;
    case POMO_BREAK: state = "PAUSA"; color = TFT_GREEN; break;
    default: state = "PARADO"; color = TFT_DARKGREY; break;
  }
  if (pomo_phase() != POMO_OFF && pomo_paused()) {
    state = "PAUSADO";
    color = TFT_YELLOW;
  }
  title_bar("POMODORO");
  font_title();
  canvas.setTextColor(color);
  canvas.setTextDatum(TR_DATUM);
  canvas.drawString(state, SCREEN_W - 1, 0);
  canvas.setTextDatum(TL_DATUM);

  uint32_t rem = pomo_phase() == POMO_OFF ? 0 : pomo_remaining_s();
  char buf[12];
  snprintf(buf, sizeof(buf), "%02lu:%02lu", (unsigned long)(rem / 60), (unsigned long)(rem % 60));
  font_big();
  canvas.setTextColor(color == TFT_DARKGREY ? TFT_WHITE : color);
  canvas.drawString(buf, SCREEN_W / 2, 20);

  if (pomo_phase() != POMO_OFF && pomo_phase_total_s() > 0) {
    uint32_t done = pomo_phase_total_s() - rem;
    canvas.drawRect(10, 46, SCREEN_W - 20, 7, TFT_DARKGREY);
    canvas.fillRect(12, 48, (int)(done * (SCREEN_W - 24) / pomo_phase_total_s()), 3, color);
  }
  font_small();
  canvas.setTextColor(TFT_DARKGREY);
  char info[32];
  snprintf(info, sizeof(info), "Ciclos: %u", pomo_cycles());
  canvas.drawString(info, 4, 58);
  canvas.drawString("Cima: >/||   Baixo: zerar", 4, 70);
}

// ===================================================================
//  PÁGINAS
// ===================================================================
static const char *PAGE_NAMES[PAGE_COUNT] = { "Principal", "Estatisticas", "Top teclas", "Pomodoro", "Relogio" };

const char *ui_page_name(uint8_t page) { return PAGE_NAMES[page % PAGE_COUNT]; }

void ui_draw_page(uint8_t page, const MonitorState &st) {
  if (page == PAGE_CLOCK) {
    ss_clock_draw();
    return;
  }
  canvas.fillSprite(TFT_BLACK);
  switch (page) {
    case PAGE_MAIN: page_main(st); break;
    case PAGE_STATS: page_stats(); break;
    case PAGE_TOP: page_top(); break;
    case PAGE_POMO: page_pomo(); break;
  }
  font_small();
}

// ===================================================================
//  ALERTAS E AVISOS
// ===================================================================
static char alert_title[24], alert_l1[32], alert_l2[32];
static uint16_t alert_color;
static uint32_t alert_until = 0;
static bool alert_on = false;

static char toast_text[28];
static uint32_t toast_until = 0;

void ui_alert(const char *title, const char *l1, const char *l2, uint16_t color, uint32_t ms) {
  strncpy(alert_title, title ? title : "", sizeof(alert_title) - 1);
  strncpy(alert_l1, l1 ? l1 : "", sizeof(alert_l1) - 1);
  strncpy(alert_l2, l2 ? l2 : "", sizeof(alert_l2) - 1);
  alert_title[sizeof(alert_title) - 1] = alert_l1[sizeof(alert_l1) - 1] = alert_l2[sizeof(alert_l2) - 1] = 0;
  alert_color = color;
  alert_until = millis() + ms;
  alert_on = true;
}

bool ui_alert_active() {
  if (alert_on && (int32_t)(millis() - alert_until) >= 0) alert_on = false;
  return alert_on;
}

void ui_alert_dismiss() { alert_on = false; }

void ui_toast(const char *text, uint32_t ms) {
  strncpy(toast_text, text, sizeof(toast_text) - 1);
  toast_text[sizeof(toast_text) - 1] = 0;
  toast_until = millis() + ms;
}

void ui_draw_overlays() {
  if (ui_alert_active()) {
    bool blink = (millis() / 400) % 2;
    canvas.fillRoundRect(6, 8, SCREEN_W - 12, SCREEN_H - 16, 6, TFT_BLACK);
    canvas.drawRoundRect(6, 8, SCREEN_W - 12, SCREEN_H - 16, 6, blink ? alert_color : TFT_DARKGREY);
    canvas.drawRoundRect(7, 9, SCREEN_W - 14, SCREEN_H - 18, 5, blink ? alert_color : TFT_DARKGREY);
    font_title();
    canvas.setTextColor(alert_color);
    canvas.setTextDatum(TC_DATUM);
    canvas.drawString(alert_title, SCREEN_W / 2, 15);
    font_small();
    canvas.setTextDatum(TC_DATUM);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString(alert_l1, SCREEN_W / 2, 36);
    canvas.drawString(alert_l2, SCREEN_W / 2, 48);
    canvas.setTextDatum(TL_DATUM);
  }
  if ((int32_t)(millis() - toast_until) < 0) {
    font_small();
    int w = canvas.textWidth(toast_text) + 10;
    int x = (SCREEN_W - w) / 2;
    canvas.fillRoundRect(x, SCREEN_H - 14, w, 13, 4, TFT_NAVY);
    canvas.drawRoundRect(x, SCREEN_H - 14, w, 13, 4, TFT_CYAN);
    canvas.setTextColor(TFT_WHITE);
    canvas.drawString(toast_text, x + 5, SCREEN_H - 11);
  }
}

void ui_draw_splash() {
  canvas.fillSprite(TFT_BLACK);
  font_title();
  canvas.setTextColor(TFT_CYAN);
  canvas.setTextDatum(MC_DATUM);
  canvas.drawString("KBD MONITOR", SCREEN_W / 2, 28);
  font_small();
  canvas.setTextDatum(MC_DATUM);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawString("versao " FW_VERSION, SCREEN_W / 2, 46);
  canvas.drawString("iniciando...", SCREEN_W / 2, 60);
  canvas.setTextDatum(TL_DATUM);
}

void ui_draw_error(const char *l1, const char *l2, const char *l3) {
  canvas.fillSprite(TFT_BLACK);
  font_title();
  canvas.setTextColor(TFT_RED);
  canvas.drawString("ERRO", 4, 2);
  font_small();
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString(l1, 4, 24);
  canvas.drawString(l2, 4, 38);
  canvas.setTextColor(TFT_YELLOW);
  canvas.drawString(l3, 4, 56);
}
