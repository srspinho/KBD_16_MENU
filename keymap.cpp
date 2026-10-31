#include "keymap.h"
#include "config.h"

// ===================================================================
//  REMAPEAMENTOS  (ativar/desativar no menu: Teclado > Remapear)
// ===================================================================
const RemapRule REMAP_RULES[] = {
  // AltGr + R  ->  '\'  (tecla 0x64 = "\|" do ABNT2)
  { MOD_ALTGR, HID_KEY_R, 0, HID_KEY_EUROPE_2 },
  // AltGr + M  ->  Ctrl + Shift + M  (liga/desliga o microfone no Teams)
  { MOD_ALTGR, HID_KEY_M, MOD_LCTRL | MOD_LSHIFT, HID_KEY_M },
};
const uint8_t REMAP_COUNT = sizeof(REMAP_RULES) / sizeof(REMAP_RULES[0]);

// ===================================================================
//  MACROS  (digitam um texto; também seguem o "Remapear" do menu)
//  Exemplo (descomente e ajuste):
//    { MOD_ALTGR, HID_KEY_1, "meu.email@empresa.com" },
//  Cuidado para não usar combinações que o ABNT2 já usa
//  (AltGr+Q = '/', AltGr+W = '?', AltGr+E = '°' ...).
// ===================================================================
const MacroRule MACRO_RULES[] = {
  { 0, 0, nullptr }  // marcador vazio (pode apagar quando criar a sua)
};
const uint8_t MACRO_COUNT = sizeof(MACRO_RULES) / sizeof(MACRO_RULES[0]);

// ===================================================================
//  CAMADA Fn  (segure LAYER_KEY, definida em config.h)
// ===================================================================
const LayerRule LAYER_RULES[] = {
  { HID_KEY_H, HID_KEY_ARROW_LEFT },
  { HID_KEY_J, HID_KEY_ARROW_DOWN },
  { HID_KEY_K, HID_KEY_ARROW_UP },
  { HID_KEY_L, HID_KEY_ARROW_RIGHT },
  { HID_KEY_Y, HID_KEY_HOME },
  { HID_KEY_O, HID_KEY_END },
  { HID_KEY_U, HID_KEY_PAGE_UP },
  { HID_KEY_N, HID_KEY_PAGE_DOWN },
  { HID_KEY_BACKSPACE, HID_KEY_DELETE },
  { HID_KEY_1, HID_KEY_F1 }, { HID_KEY_2, HID_KEY_F2 }, { HID_KEY_3, HID_KEY_F3 },
  { HID_KEY_4, HID_KEY_F4 }, { HID_KEY_5, HID_KEY_F5 }, { HID_KEY_6, HID_KEY_F6 },
  { HID_KEY_7, HID_KEY_F7 }, { HID_KEY_8, HID_KEY_F8 }, { HID_KEY_9, HID_KEY_F9 },
  { HID_KEY_0, HID_KEY_F10 }, { HID_KEY_MINUS, HID_KEY_F11 }, { HID_KEY_EQUAL, HID_KEY_F12 },
};
const uint8_t LAYER_COUNT = sizeof(LAYER_RULES) / sizeof(LAYER_RULES[0]);

// ===================================================================
//  ASCII -> HID
//  Só caracteres que ficam na MESMA posição no ABNT2 e no US, para a
//  macro funcionar nos dois layouts: letras, números, espaço, Enter,
//  Tab e  . , - _ = + ! @ # $ % & * ( )
// ===================================================================
bool ascii_to_hid(char c, uint8_t &mod, uint8_t &key) {
  mod = 0;
  if (c >= 'a' && c <= 'z') { key = HID_KEY_A + (c - 'a'); return true; }
  if (c >= 'A' && c <= 'Z') { key = HID_KEY_A + (c - 'A'); mod = MOD_LSHIFT; return true; }
  if (c >= '1' && c <= '9') { key = HID_KEY_1 + (c - '1'); return true; }
  switch (c) {
    case '0': key = HID_KEY_0; return true;
    case ' ': key = HID_KEY_SPACE; return true;
    case '\n': key = HID_KEY_ENTER; return true;
    case '\t': key = HID_KEY_TAB; return true;
    case '.': key = HID_KEY_PERIOD; return true;
    case ',': key = HID_KEY_COMMA; return true;
    case '-': key = HID_KEY_MINUS; return true;
    case '_': key = HID_KEY_MINUS; mod = MOD_LSHIFT; return true;
    case '=': key = HID_KEY_EQUAL; return true;
    case '+': key = HID_KEY_EQUAL; mod = MOD_LSHIFT; return true;
    case '!': key = HID_KEY_1; mod = MOD_LSHIFT; return true;
    case '@': key = HID_KEY_2; mod = MOD_LSHIFT; return true;
    case '#': key = HID_KEY_3; mod = MOD_LSHIFT; return true;
    case '$': key = HID_KEY_4; mod = MOD_LSHIFT; return true;
    case '%': key = HID_KEY_5; mod = MOD_LSHIFT; return true;
    case '&': key = HID_KEY_7; mod = MOD_LSHIFT; return true;
    case '*': key = HID_KEY_8; mod = MOD_LSHIFT; return true;
    case '(': key = HID_KEY_9; mod = MOD_LSHIFT; return true;
    case ')': key = HID_KEY_0; mod = MOD_LSHIFT; return true;
  }
  return false;
}

// ===================================================================
//  Nomes das teclas (ABNT2) para a tela "Top teclas"
// ===================================================================
const char *key_name(uint8_t k, char *buf, size_t n) {
  if (k >= HID_KEY_A && k <= HID_KEY_Z) { snprintf(buf, n, "%c", 'A' + (k - HID_KEY_A)); return buf; }
  if (k >= HID_KEY_1 && k <= HID_KEY_9) { snprintf(buf, n, "%c", '1' + (k - HID_KEY_1)); return buf; }
  if (k >= HID_KEY_F1 && k <= HID_KEY_F12) { snprintf(buf, n, "F%d", 1 + (k - HID_KEY_F1)); return buf; }
  if (k >= HID_KEY_KEYPAD_1 && k <= HID_KEY_KEYPAD_9) { snprintf(buf, n, "Num%d", 1 + (k - HID_KEY_KEYPAD_1)); return buf; }
  const char *s = nullptr;
  switch (k) {
    case HID_KEY_0: s = "0"; break;
    case HID_KEY_ENTER: s = "Enter"; break;
    case HID_KEY_ESCAPE: s = "Esc"; break;
    case HID_KEY_BACKSPACE: s = "Bksp"; break;
    case HID_KEY_TAB: s = "Tab"; break;
    case HID_KEY_SPACE: s = "Espaco"; break;
    case HID_KEY_MINUS: s = "-"; break;
    case HID_KEY_EQUAL: s = "="; break;
    case HID_KEY_BRACKET_LEFT: s = "Agudo"; break;   // tecla do acento agudo
    case HID_KEY_BRACKET_RIGHT: s = "["; break;
    case HID_KEY_BACKSLASH: s = "]"; break;
    case HID_KEY_EUROPE_1: s = "]"; break;
    case HID_KEY_SEMICOLON: s = "\x80"; break;      // Ç (fonte GLCD)
    case HID_KEY_APOSTROPHE: s = "~"; break;
    case HID_KEY_GRAVE: s = "'"; break;
    case HID_KEY_COMMA: s = ","; break;
    case HID_KEY_PERIOD: s = "."; break;
    case HID_KEY_SLASH: s = ";"; break;
    case HID_KEY_CAPS_LOCK: s = "Caps"; break;
    case HID_KEY_PRINT_SCREEN: s = "PrtSc"; break;
    case HID_KEY_SCROLL_LOCK: s = "ScrLk"; break;
    case HID_KEY_PAUSE: s = "Pause"; break;
    case HID_KEY_INSERT: s = "Ins"; break;
    case HID_KEY_HOME: s = "Home"; break;
    case HID_KEY_PAGE_UP: s = "PgUp"; break;
    case HID_KEY_DELETE: s = "Del"; break;
    case HID_KEY_END: s = "End"; break;
    case HID_KEY_PAGE_DOWN: s = "PgDn"; break;
    case HID_KEY_ARROW_RIGHT: s = "Dir"; break;
    case HID_KEY_ARROW_LEFT: s = "Esq"; break;
    case HID_KEY_ARROW_DOWN: s = "Baixo"; break;
    case HID_KEY_ARROW_UP: s = "Cima"; break;
    case HID_KEY_NUM_LOCK: s = "NumLk"; break;
    case HID_KEY_KEYPAD_DIVIDE: s = "Num/"; break;
    case HID_KEY_KEYPAD_MULTIPLY: s = "Num*"; break;
    case HID_KEY_KEYPAD_SUBTRACT: s = "Num-"; break;
    case HID_KEY_KEYPAD_ADD: s = "Num+"; break;
    case HID_KEY_KEYPAD_ENTER: s = "NEnter"; break;
    case HID_KEY_KEYPAD_0: s = "Num0"; break;
    case HID_KEY_KEYPAD_DECIMAL: s = "Num,"; break;
    case HID_KEY_EUROPE_2: s = "\\"; break;
    case HID_KEY_APPLICATION: s = "Menu"; break;
    case 0x87: s = "/"; break;  // Intl1 (/? do ABNT2)
    case 0x85: s = "Num."; break;
  }
  if (s) return s;
  snprintf(buf, n, "0x%02X", k);
  return buf;
}
