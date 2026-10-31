/*********************************************************************
 * display.h - Display, sprite de tela cheia e utilitários de cor
 *
 * Tudo é desenhado no "canvas" (sprite 160x80 criado UMA vez) e
 * enviado inteiro para o display com display_push(): sem cintilação.
 *********************************************************************/
#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <TFT_eSPI.h>

extern TFT_eSPI tft;
extern TFT_eSprite canvas;

void display_begin(bool flip);
void display_set_flip(bool flip);
void display_push();
void display_set_backlight(uint8_t percent);  // 0..100

// Arco-íris pré-calculado (256 cores). Muito mais rápido que sin() por pixel.
void color_lut_build(uint8_t intensity_pct);
uint16_t rainbow(uint8_t pos);

// Seno inteiro: isin(0..255) -> -127..127 (256 = volta completa)
int8_t isin(uint8_t a);
inline int8_t icos(uint8_t a) { return isin(a + 64); }

// Escala uma cor RGB565 por 0..255
uint16_t color_scale(uint16_t c, uint8_t f);

#endif
