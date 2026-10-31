#include "display.h"
#include "config.h"

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);

static uint16_t lut[256];
static int8_t sin_table[256];
static int bl_pin = -1;

void display_begin(bool flip) {
#if defined(TFT_BL)
  bl_pin = TFT_BL;
#else
  bl_pin = PIN_BACKLIGHT;
#endif
  if (bl_pin >= 0) {
    pinMode(bl_pin, OUTPUT);
    analogWriteRange(255);
    analogWrite(bl_pin, 255);
  }

  tft.init();
  display_set_flip(flip);
  tft.fillScreen(TFT_BLACK);

  canvas.setColorDepth(16);
  canvas.createSprite(SCREEN_W, SCREEN_H);
  canvas.fillSprite(TFT_BLACK);

  for (int i = 0; i < 256; i++) sin_table[i] = (int8_t)lroundf(sinf(i * 2.0f * PI / 256.0f) * 127.0f);
  color_lut_build(100);
}

void display_set_flip(bool flip) { tft.setRotation(flip ? 3 : 1); }

void display_push() { canvas.pushSprite(0, 0); }

void display_set_backlight(uint8_t percent) {
  static int last = -1;
  if (bl_pin < 0 || percent == last) return;
  last = percent;
  analogWrite(bl_pin, map(constrain(percent, 0, 100), 0, 100, 0, 255));
}

void color_lut_build(uint8_t intensity_pct) {
  float k = constrain(intensity_pct, 0, 100) / 100.0f;
  for (int i = 0; i < 256; i++) {
    float a = i * 2.0f * PI / 256.0f;
    uint8_t r = (uint8_t)((sinf(a) * 127 + 128) * k);
    uint8_t g = (uint8_t)((sinf(a + 2.094f) * 127 + 128) * k);
    uint8_t b = (uint8_t)((sinf(a + 4.189f) * 127 + 128) * k);
    lut[i] = tft.color565(r, g, b);
  }
}

uint16_t rainbow(uint8_t pos) { return lut[pos]; }

int8_t isin(uint8_t a) { return sin_table[a]; }

uint16_t color_scale(uint16_t c, uint8_t f) {
  uint16_t r = ((c >> 11) & 0x1F) * f / 255;
  uint16_t g = ((c >> 5) & 0x3F) * f / 255;
  uint16_t b = (c & 0x1F) * f / 255;
  return (r << 11) | (g << 5) | b;
}
