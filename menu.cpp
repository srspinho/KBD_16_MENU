#include "menu.h"
#include "display.h"
#include "settings.h"
#include "screensavers.h"
#include "clock.h"
#include "config.h"

// ===================================================================
//  TIPOS
// ===================================================================
enum ItemType : uint8_t {
  IT_SUBMENU,   // abre outro menu
  IT_INT,       // valor numérico editável
  IT_BOOL,      // liga/desliga
  IT_SS_LIST,   // lista de screensavers (gerada da tabela)
  IT_ACTION,    // executa uma função
  IT_CLOCK_H,   // hora do relógio
  IT_CLOCK_M,   // minuto do relógio
  IT_BACK,      // volta (na raiz: fecha)
};

struct Menu;

struct MenuItem {
  const char *label;
  ItemType type;
  int16_t *value;
  bool *flag;
  int16_t min, max, step;
  const char *unit;
  const char *zero_text;  // texto exibido quando o valor é 0
  const Menu *sub;
  void (*action)();
  bool confirm;           // pede confirmação (OK duas vezes)
};

struct Menu {
  const char *title;
  const MenuItem *items;
  uint8_t count;
};

// Construtores curtos para deixar as tabelas legíveis
static constexpr MenuItem SUB(const char *l, const Menu *m) {
  return { l, IT_SUBMENU, nullptr, nullptr, 0, 0, 1, "", nullptr, m, nullptr, false };
}
static constexpr MenuItem INT(const char *l, int16_t *v, int16_t mn, int16_t mx, int16_t st,
                              const char *u = "", const char *z = nullptr) {
  return { l, IT_INT, v, nullptr, mn, mx, st, u, z, nullptr, nullptr, false };
}
static constexpr MenuItem BOOL(const char *l, bool *f) {
  return { l, IT_BOOL, nullptr, f, 0, 0, 1, "", nullptr, nullptr, nullptr, false };
}
static constexpr MenuItem ACT(const char *l, void (*a)(), bool confirm = false) {
  return { l, IT_ACTION, nullptr, nullptr, 0, 0, 1, "", nullptr, nullptr, a, confirm };
}
static constexpr MenuItem SPECIAL(const char *l, ItemType t) {
  return { l, t, nullptr, nullptr, 0, 0, 1, "", nullptr, nullptr, nullptr, false };
}
#define COUNT(a) (uint8_t)(sizeof(a) / sizeof(a[0]))

// ===================================================================
//  TABELAS
// ===================================================================
static const MenuItem ITEMS_SS[] = {
  SPECIAL("Tipo", IT_SS_LIST),
  INT("Iniciar apos", &settings.ss_start_min, 0, 120, 1, "m", "Nunca"),
  INT("Trocar a cada", &settings.ss_switch_s, 10, 600, 10, "s"),
  INT("Velocidade", &settings.ss_speed, 1, 20, 1),
  INT("Asteroides", &settings.ss_asteroids, 1, MAX_ASTEROIDS, 1),
  INT("Cores", &settings.ss_color, 10, 100, 5, "%"),
  SPECIAL("Voltar", IT_BACK),
};
static const Menu MENU_SS = { "SCREENSAVER", ITEMS_SS, COUNT(ITEMS_SS) };

static const MenuItem ITEMS_TELA[] = {
  INT("Brilho", &settings.brightness, 10, 100, 10, "%"),
  INT("Brilho ocioso", &settings.dim_brightness, 0, 100, 10, "%"),
  INT("Escurecer apos", &settings.dim_after_s, 0, 3600, 10, "s", "Nunca"),
  BOOL("Girar 180", &settings.flip),
  SPECIAL("Voltar", IT_BACK),
};
static const Menu MENU_TELA = { "TELA", ITEMS_TELA, COUNT(ITEMS_TELA) };

static const MenuItem ITEMS_PROD[] = {
  BOOL("Anti-bloqueio", &settings.antilock),
  INT("Intervalo", &settings.antilock_s, 5, 600, 5, "s"),
  ACT("Pomodoro >/||", action_pomo_start_pause),
  ACT("Pomodoro zerar", action_pomo_reset),
  INT("Foco", &settings.pomo_focus_min, 1, 120, 1, "m"),
  INT("Pausa", &settings.pomo_break_min, 1, 60, 1, "m"),
  INT("Lembrete pausa", &settings.break_remind_min, 0, 240, 5, "m", "Off"),
  SPECIAL("Voltar", IT_BACK),
};
static const Menu MENU_PROD = { "PRODUTIVIDADE", ITEMS_PROD, COUNT(ITEMS_PROD) };

static const MenuItem ITEMS_TECLADO[] = {
  BOOL("Remapear", &settings.remap),
  BOOL("Camada Fn", &settings.layer),
  BOOL("Sinc. volume", &settings.vol_sync),
  INT("Volume inicial", &settings.vol_sync_level, 0, 100, 2, "%"),
  SPECIAL("Voltar", IT_BACK),
};
static const Menu MENU_TECLADO = { "TECLADO", ITEMS_TECLADO, COUNT(ITEMS_TECLADO) };

static const MenuItem ITEMS_RELOGIO[] = {
  SPECIAL("Hora", IT_CLOCK_H),
  SPECIAL("Minuto", IT_CLOCK_M),
  SPECIAL("Voltar", IT_BACK),
};
static const Menu MENU_RELOGIO = { "RELOGIO", ITEMS_RELOGIO, COUNT(ITEMS_RELOGIO) };

static const MenuItem ITEMS_STATS[] = {
  ACT("Zerar sessao", action_reset_session, true),
  ACT("Zerar tudo", action_reset_all_stats, true),
  SPECIAL("Voltar", IT_BACK),
};
static const Menu MENU_STATS = { "ESTATISTICAS", ITEMS_STATS, COUNT(ITEMS_STATS) };

static const MenuItem ITEMS_SISTEMA[] = {
  ACT("Sobre", action_about),
  ACT("Reiniciar", action_reboot, true),
  ACT("Restaurar padrao", action_factory_defaults, true),
  ACT("Modo gravacao", action_bootloader, true),
  SPECIAL("Voltar", IT_BACK),
};
static const Menu MENU_SISTEMA = { "SISTEMA", ITEMS_SISTEMA, COUNT(ITEMS_SISTEMA) };

static const MenuItem ITEMS_MAIN[] = {
  SUB("Screensaver", &MENU_SS),
  SUB("Tela", &MENU_TELA),
  SUB("Produtividade", &MENU_PROD),
  SUB("Teclado", &MENU_TECLADO),
  SUB("Relogio", &MENU_RELOGIO),
  SUB("Estatisticas", &MENU_STATS),
  SUB("Sistema", &MENU_SISTEMA),
  SPECIAL("Sair", IT_BACK),
};
static const Menu MENU_MAIN = { "CONFIGURACOES", ITEMS_MAIN, COUNT(ITEMS_MAIN) };

// ===================================================================
//  ESTADO
// ===================================================================
#define VISIBLE_ROWS 5
#define MENU_TIMEOUT_MS 30000UL

struct Level {
  const Menu *menu;  // nullptr = lista de screensavers
  uint8_t index;
  uint8_t top;
};

static Level stack[4];
static int8_t depth = -1;   // -1 = menu fechado
static bool editing = false;
static int8_t confirm_index = -1;
static uint32_t last_input = 0;

static Level &cur() { return stack[depth]; }
static bool in_ss_list() { return cur().menu == nullptr; }
static uint8_t item_count() { return in_ss_list() ? ss_count() + 1 : cur().menu->count; }

static void scroll_to_index(Level &l) {
  if (l.index < l.top) l.top = l.index;
  if (l.index >= l.top + VISIBLE_ROWS) l.top = l.index - VISIBLE_ROWS + 1;
}

void menu_open() {
  depth = 0;
  stack[0] = { &MENU_MAIN, 0, 0 };
  editing = false;
  confirm_index = -1;
  last_input = millis();
}

void menu_close() {
  depth = -1;
  editing = false;
  confirm_index = -1;
}

bool menu_is_open() { return depth >= 0; }

static void push(const Menu *m) {
  if (depth >= 3) return;
  depth++;
  stack[depth] = { m, 0, 0 };
  if (m == nullptr) {  // lista de screensavers: começa no item selecionado
    stack[depth].index = settings.ss_type;
    scroll_to_index(stack[depth]);
  }
}

// Volta um nível. O cursor do nível anterior continua onde estava.
static void pop() {
  editing = false;
  if (depth <= 0) menu_close();
  else depth--;
}

static void clock_adjust(ItemType t, int dir) {
  uint8_t h = 12, m = 0, s = 0;
  if (clock_is_set()) clock_get(h, m, s);
  if (t == IT_CLOCK_H) h = (h + 24 + dir) % 24;
  else m = (m + 60 + dir) % 60;
  clock_set(h, m, 0);
}

static void adjust(const MenuItem &it, int dir) {
  if (it.type == IT_CLOCK_H || it.type == IT_CLOCK_M) {
    clock_adjust(it.type, dir);
    return;
  }
  int v = *it.value + dir * it.step;
  *it.value = constrain(v, it.min, it.max);
  settings_mark_dirty();
}

// IMPORTANTE: o índice do item NÃO é zerado ao editar/alternar
// (era o bug da versão anterior, em que só o 1º item podia ser editado).
static void select_item() {
  uint8_t i = cur().index;
  if (in_ss_list()) {
    settings.ss_type = i;
    settings_mark_dirty();
    return;
  }
  const MenuItem &it = cur().menu->items[i];
  switch (it.type) {
    case IT_SUBMENU: push(it.sub); break;
    case IT_SS_LIST: push(nullptr); break;
    case IT_INT:
    case IT_CLOCK_H:
    case IT_CLOCK_M: editing = true; break;
    case IT_BOOL:
      *it.flag = !*it.flag;
      settings_mark_dirty();
      break;
    case IT_ACTION:
      if (it.confirm && confirm_index != (int8_t)i) {
        confirm_index = i;  // pede confirmação: OK de novo executa
        return;
      }
      confirm_index = -1;
      if (it.action) it.action();
      break;
    case IT_BACK: pop(); break;
  }
}

void menu_handle(const BtnEvent &ev) {
  if (!menu_is_open()) return;
  last_input = millis();

  if (editing) {
    const MenuItem &it = cur().menu->items[cur().index];
    if (ev.btn == BTN_UP) adjust(it, +1);
    else if (ev.btn == BTN_DOWN) adjust(it, -1);
    else if (!ev.repeat) editing = false;  // OK / Esquerda / Direita confirmam
    return;
  }

  // Qualquer botão que não seja OK/Direita cancela a confirmação pendente
  bool was_confirming = confirm_index >= 0;
  if (ev.btn != BTN_OK && ev.btn != BTN_RIGHT) confirm_index = -1;

  uint8_t n = item_count();
  Level &l = cur();
  switch (ev.btn) {
    case BTN_UP:
      // Na repetição (botão segurado) para no topo em vez de dar a volta
      l.index = (l.index == 0) ? (ev.repeat ? 0 : n - 1) : l.index - 1;
      break;
    case BTN_DOWN:
      l.index = (l.index + 1 >= n) ? (ev.repeat ? n - 1 : 0) : l.index + 1;
      break;
    case BTN_LEFT:
      if (!ev.repeat && !was_confirming) pop();
      break;
    case BTN_OK:
    case BTN_RIGHT:
      if (!ev.repeat) select_item();
      break;
    default: break;
  }
  if (menu_is_open()) scroll_to_index(cur());
}

void menu_update() {
  if (menu_is_open() && millis() - last_input > MENU_TIMEOUT_MS) menu_close();
}

// ===================================================================
//  DESENHO
// ===================================================================
static void value_text(const MenuItem &it, char *buf, size_t n) {
  buf[0] = 0;
  uint8_t h, m, s;
  switch (it.type) {
    case IT_INT:
      if (*it.value == 0 && it.zero_text) snprintf(buf, n, "%s", it.zero_text);
      else snprintf(buf, n, "%d%s", *it.value, it.unit);
      break;
    case IT_BOOL: snprintf(buf, n, "%s", *it.flag ? "ON" : "OFF"); break;
    case IT_SS_LIST:
      snprintf(buf, n, "%s", settings.ss_type >= ss_count() ? "Todos" : ss_get(settings.ss_type).name);
      break;
    case IT_CLOCK_H:
    case IT_CLOCK_M:
      if (!clock_is_set()) {
        snprintf(buf, n, "--");
      } else {
        clock_get(h, m, s);
        snprintf(buf, n, "%02u", it.type == IT_CLOCK_H ? h : m);
      }
      break;
    case IT_SUBMENU: snprintf(buf, n, ">"); break;
    default: break;
  }
}

void menu_draw() {
  if (!menu_is_open()) return;
  canvas.fillSprite(TFT_BLACK);
  canvas.setTextFont(1);
  canvas.setTextSize(1);
  canvas.setTextDatum(TL_DATUM);

  const char *title = in_ss_list() ? "SCREENSAVER: TIPO" : cur().menu->title;
  canvas.setTextColor(TFT_YELLOW);
  canvas.drawString(title, 4, 2);
  if (clock_is_set()) {
    char hb[8];
    clock_format(hb, sizeof(hb));
    canvas.setTextColor(TFT_DARKGREY);
    canvas.setTextDatum(TR_DATUM);
    canvas.drawString(hb, SCREEN_W - 3, 2);
    canvas.setTextDatum(TL_DATUM);
  }
  canvas.drawFastHLine(0, 12, SCREEN_W, TFT_DARKGREY);

  uint8_t n = item_count();
  const Level &l = cur();
  int y = 15;
  for (uint8_t i = l.top; i < n && i < l.top + VISIBLE_ROWS; i++, y += 13) {
    bool sel = (i == l.index);
    bool confirming = sel && confirm_index == (int8_t)i;
    uint16_t bg = TFT_BLACK;
    if (sel) {
      bg = confirming ? TFT_MAROON : (editing ? TFT_RED : TFT_BLUE);
      canvas.fillRoundRect(1, y - 2, SCREEN_W - 7, 12, 3, bg);
    }
    canvas.setTextColor(TFT_WHITE, bg);

    char val[16];
    const char *label;
    if (in_ss_list()) {
      label = (i == ss_count()) ? "Todos" : ss_get(i).name;
      canvas.drawCircle(SCREEN_W - 16, y + 3, 3, sel ? TFT_WHITE : TFT_DARKGREY);
      if (settings.ss_type == i) canvas.fillCircle(SCREEN_W - 16, y + 3, 2, sel ? TFT_WHITE : TFT_YELLOW);
      val[0] = 0;
    } else {
      const MenuItem &it = l.menu->items[i];
      label = confirming ? "Confirmar? OK" : it.label;
      value_text(it, val, sizeof(val));
    }
    canvas.drawString(label, 6, y);
    if (val[0]) {
      canvas.setTextDatum(TR_DATUM);
      canvas.setTextColor(sel ? TFT_WHITE : TFT_CYAN, bg);
      canvas.drawString(val, SCREEN_W - 10, y);
      canvas.setTextDatum(TL_DATUM);
    }
  }

  // Barra de rolagem
  if (n > VISIBLE_ROWS) {
    int track = SCREEN_H - 15;
    int h = max(6, track * VISIBLE_ROWS / n);
    int pos = 14 + (track - h) * l.top / (n - VISIBLE_ROWS);
    canvas.drawFastVLine(SCREEN_W - 3, 14, track, TFT_DARKGREY);
    canvas.fillRect(SCREEN_W - 4, pos, 3, h, TFT_LIGHTGREY);
  }
}
