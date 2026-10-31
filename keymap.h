/*********************************************************************
 * keymap.h - Remapeamentos, macros e camada Fn
 *
 * Para adicionar/alterar atalhos, edite as tabelas em keymap.cpp.
 *********************************************************************/
#ifndef KEYMAP_H
#define KEYMAP_H

#include <Arduino.h>
#include "Adafruit_TinyUSB.h"

// Atalhos de modificadores (bits do byte "modifier" do report HID)
#define MOD_LCTRL  KEYBOARD_MODIFIER_LEFTCTRL
#define MOD_LSHIFT KEYBOARD_MODIFIER_LEFTSHIFT
#define MOD_LALT   KEYBOARD_MODIFIER_LEFTALT
#define MOD_LGUI   KEYBOARD_MODIFIER_LEFTGUI
#define MOD_RCTRL  KEYBOARD_MODIFIER_RIGHTCTRL
#define MOD_RSHIFT KEYBOARD_MODIFIER_RIGHTSHIFT
#define MOD_ALTGR  KEYBOARD_MODIFIER_RIGHTALT
#define MOD_RGUI   KEYBOARD_MODIFIER_RIGHTGUI

// Remapeamento: quando (modificador + tecla) é pressionado, envia
// (out_mod + out_key) e remove o modificador de origem.
struct RemapRule {
  uint8_t mod;      // modificador exigido (ex.: MOD_ALTGR)
  uint8_t key;      // tecla física
  uint8_t out_mod;  // modificadores adicionados na saída
  uint8_t out_key;  // tecla enviada ao PC
};

// Macro: (modificador + tecla) digita um texto.
struct MacroRule {
  uint8_t mod;
  uint8_t key;
  const char *text;  // ver ascii_to_hid() para os caracteres aceitos
};

// Camada Fn: com LAYER_KEY pressionada, "key" vira "out_key".
struct LayerRule {
  uint8_t key;
  uint8_t out_key;
};

extern const RemapRule REMAP_RULES[];
extern const uint8_t REMAP_COUNT;
extern const MacroRule MACRO_RULES[];
extern const uint8_t MACRO_COUNT;
extern const LayerRule LAYER_RULES[];
extern const uint8_t LAYER_COUNT;

// Converte um caractere em (modificador, keycode). Retorna false se o
// caractere não for suportado.
bool ascii_to_hid(char c, uint8_t &mod, uint8_t &key);

// Nome curto da tecla para exibição (padrão ABNT2).
const char *key_name(uint8_t keycode, char *buf, size_t buflen);

#endif
