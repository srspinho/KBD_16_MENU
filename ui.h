/*********************************************************************
 * ui.h - Telas do monitor, alertas e avisos rápidos
 *
 * Páginas (Esquerda/Direita no joystick):
 *   0 Principal   contador grande, gráfico de 60 s, volume, LEDs, hora
 *   1 Estatísticas sessão / hoje / total / KPM / pico / tempo ativo
 *   2 Top teclas  as 5 teclas mais usadas
 *   3 Pomodoro    Cima = iniciar/pausar, Baixo = zerar
 *   4 Relógio     hora grande
 *********************************************************************/
#ifndef UI_H
#define UI_H

#include <Arduino.h>

enum Page : uint8_t { PAGE_MAIN, PAGE_STATS, PAGE_TOP, PAGE_POMO, PAGE_CLOCK, PAGE_COUNT };

struct MonitorState {
  int16_t volume;          // volume estimado do PC (0..100)
  bool muted;
  bool keyboard;           // teclado conectado
  bool pc;                 // PC conectado
  uint8_t leds;            // bits: 0 Num, 1 Caps, 2 Scroll
};

void ui_draw_page(uint8_t page, const MonitorState &st);
const char *ui_page_name(uint8_t page);

// Alerta em destaque (some sozinho ou com qualquer botão)
void ui_alert(const char *title, const char *line1, const char *line2, uint16_t color, uint32_t ms);
bool ui_alert_active();
void ui_alert_dismiss();

// Aviso rápido no rodapé
void ui_toast(const char *text, uint32_t ms = 1200);

// Desenha alerta/aviso por cima do que estiver no canvas
void ui_draw_overlays();

void ui_draw_splash();
void ui_draw_error(const char *line1, const char *line2, const char *line3);

#endif
