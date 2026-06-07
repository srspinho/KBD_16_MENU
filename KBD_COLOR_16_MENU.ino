/*********************************************************************
 * PROJETO: HID Remapper & System Monitor v2.5 + Pac-Man SS & Menu
 * HARDWARE: RP2040/RP2350 + TFT 160x80 (ST7735)
 *********************************************************************/

#include "usbh_helper.h"
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Adafruit_TinyUSB.h>
#include "RobotoMono12pt7b.h"
#include "Orbitron_Bold6pt7b.h"
#include "atari.h"
#include "menu.h"  // Integração com a persistência e UI do menu

// --- VARIÁVEIS EXTERNAS DO MENU ---
extern MenuLevel current_menu_level;
extern int current_item_index;

/* --- PROTÓTIPOS --- */
void set_report_callback(uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize);
void process_kbd_report(hid_keyboard_report_t const *report);
void remap_key(hid_keyboard_report_t const *original, hid_keyboard_report_t *remapped);
void update_display();

// Screensaver: Game of Life
void init_game();
void update_game();
void draw_game();

// Screensaver: Matrix
void init_matrix();
void update_matrix();
void draw_matrix();

// Screensaver: Starfield
void init_starfield();
void update_starfield();
void draw_starfield();

// Screensaver: Pac-Man
void init_pong();
void update_pong();
void draw_pong();
void init_pacman();
void update_pacman();
void draw_pacman_ss();
void drawPacManShape(int x, int y, int mouthAngle);
void drawGhostShape(int x, int y, uint16_t color, bool vulnerable);

// Screensaver: Asteroids & Atari
void init_asteroids();
void update_asteroids();
void draw_asteroids();
void init_atari();
void update_atari();
void draw_atari_logo();
uint16_t rainbowColor(int pos);
uint16_t neonPulseColor(int pos, float intensity, int phase);

/* --- VARIÁVEIS PONG --- */
float ballX, ballY, ballDX, ballDY;
int paddle1Y, paddle2Y;
int scoreL = 0, scoreR = 0;
const int paddleH = 15;
const int paddleW = 3;
bool sprite_initialized = false;

int color_offset = 0;

// Offset global para o gradiente do logo Atari
int rainbow_offset = 0;
const int logoWidth = 74;
const int logoHeight = 80;

/* --- HARDWARE --- */
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite canvas = TFT_eSprite(&tft);

/* --- VARIÁVEIS GLOBAIS --- */
volatile uint32_t g_key_count = 0;
volatile uint8_t g_volume = 70;
volatile bool g_display_dirty = true;
unsigned long last_activity_time = 0;
unsigned long teams_alert_timeout = 0;
unsigned long lastGraphUpdate = 0;
bool title_drawn = false;
int lastVolume = -1;
unsigned long lastAntiLockTime = 0;

const uint32_t SS_SWITCH_INTERVAL = 30000;  // troca a cada 30 segundos se em modo SS_TODOS
unsigned long last_ss_switch = 0;
bool is_dimmed = false;
bool is_screensaver = false;
int current_ss_type = 0;
const uint32_t DIM_TIMEOUT = 30000;
const uint32_t LIFE_TIMEOUT = 120000;  // Será sobrescrito pela config do menu no loop

bool g_em_modo_menu = false;  // Controla se o visor exibe o menu ou o monitor

// Variáveis Pac-Man
int pac_posX = -60;
bool pac_initialized = false;
int prev_pac_posX = -60;

// Game of Life Vars
#define GOL_W 80
#define GOL_H 40
uint8_t world[GOL_W][GOL_H];
uint8_t next_world[GOL_W][GOL_H];
uint8_t world_last[GOL_W][GOL_H];
uint32_t history_checksum[4];
uint32_t last_gen_time = 0;
int stable_count = 0;

// ====================== CONFIGURAÇÃO ======================A
#define MAX_FISH 6
#define MAX_SEAHORSES 1
#define MAX_BUBBLES 12


/* --- VARIÁVEIS BOUNCING BALL --- */
float ballX2 = 80, ballY2 = 40;
float ballDX2 = 2.2, ballDY2 = 1.8;

/* ==================== SCREENSAVER: RANDOM EXPLOSIONS ==================== */
#define MAX_PARTICLES 50

struct Particle {
  float x, y;
  float vx, vy;
  uint8_t life;
  uint16_t color;
  bool trail;  // para dar efeito de rastro
};

Particle particles[MAX_PARTICLES];
unsigned long lastFirework = 0;

/* ==================== SCREENSAVER: BOUNCING BALLS (3 bolas) ==================== */
// Estrutura para as bolas
struct Ball {
  float x, y;
  float dx, dy;
  uint16_t color;
};

Ball balls[3];

// Starfield Vars
struct Star {
  int x;
  int y;
  int speed;
  uint16_t color;
  int size;
};
Star stars[50];

// Asteroids Vars
struct Asteroid {
  int x, y;
  int vx, vy;
  int size;
  int points;
  int angle[10];
  int radius[10];
};
#define MAX_ASTEROIDS 20  // Limite seguro para o array fixo
Asteroid asteroids[MAX_ASTEROIDS];

// ====================== ESTRUTURAS ======================
struct Fish {
  float x, y;
  float vx, vy;
  int type;
  bool active;
};

struct Seahorse {
  float x, y;
  float vx;
  bool facingRight;
  bool active;
};

struct Bubble {
  float x, y;
  float vy;
  bool active;
};

// ====================== GLIFOS ======================
const char *fishGlyphs[] = { "><>", ">')>", "o><", "><" };
const int FISH_TYPES = 4;

Fish fishPool[MAX_FISH];
Seahorse seahorses[MAX_SEAHORSES];
Bubble bubbles[MAX_BUBBLES];

// Gráfico Vars
#define GRAPH_WIDTH 60
#define GRAPH_HEIGHT 20
int history[GRAPH_WIDTH] = { 0 };
int maxKeys = 20;
int contadorDeTeclas = 0;
bool graphNeedsUpdate = true;
static char last_buf[12] = "";
static int16_t last_xPos = 0;

// Matrix Vars
#define MATRIX_COLS 10
int8_t drop_pos[MATRIX_COLS];
uint8_t drop_speed[MATRIX_COLS];
uint32_t matrix_start_time = 0;
char matrix_last[MATRIX_COLS][8];

volatile uint8_t g_leds = 0;

// USB Vars
volatile uint8_t dev_addr_keyboard = 0;
volatile uint8_t instance_keyboard = 0;
volatile uint64_t keys_currently_pressed = 0;
#define FIFO_DISPLAY_UPDATE 1

uint8_t const desc_hid_report[] = {
  TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(1)),
  TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(2)),
  TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(3))
};
Adafruit_USBD_HID usb_hid(desc_hid_report, sizeof(desc_hid_report), HID_ITF_PROTOCOL_KEYBOARD, 2, false);
bool firstDrawAfterSS = true;

/* ================================================================
   CORE 1: USB HOST
   ================================================================ */
void setup1() {
  rp2040_configure_pio_usb();
  USBHost.begin(1);
}

void loop1() {
  USBHost.task();
}

void desalocar_sprite_menu() {
  canvas.deleteSprite();

  sprite_initialized = false;
  pac_initialized = false;

  // Recria o sprite no tamanho padrão do monitor
  canvas.createSprite(160, 80);
  canvas.fillSprite(TFT_BLACK);
}

/* ================================================================
   CORE 0: DISPLAY & LOGIC
   ================================================================ */
void setup() {
  usb_hid.setReportCallback(NULL, set_report_callback);
  usb_hid.begin();

  // Sincronização de Volume Nativa
  delay(3000);
  if (usb_hid.ready()) {
    for (int i = 0; i < 50; i++) {
      uint8_t vol_down[2] = { 0xEA, 0x00 };
      usb_hid.sendReport(2, vol_down, 2);
      delay(10);
      uint8_t release[2] = { 0x00, 0x00 };
      usb_hid.sendReport(2, release, 2);
      delay(10);
    }
    for (int i = 0; i < 35; i++) {
      uint8_t vol_up[2] = { 0xE9, 0x00 };
      usb_hid.sendReport(2, vol_up, 2);
      delay(10);
      uint8_t release[2] = { 0x00, 0x00 };
      usb_hid.sendReport(2, release, 2);
      delay(10);
    }
    g_volume = 70;
  }

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  randomSeed(analogRead(26) + micros());

  setup_menu();


#ifdef TFT_BL
  analogWrite(TFT_BL, map(config.sistema_brilho, 0, 100, 0, 255));
#endif

  g_volume = 70;

  // Se a configuração de início de screensaver estiver definida como imediata (0 minutos)
  // ou se você quer que ele aplique o screensaver gravado logo no boot:
  if (config.prod_inicio == 0) {
    last_activity_time = millis() - (60ULL * 1000ULL);  // Força estouro imediato para chamar o SS configurado
  } else {
    last_activity_time = millis();
  }

  g_em_modo_menu = false;  // Começa executando o sistema limpo
  firstDrawAfterSS = true;
  g_display_dirty = true;
}


void loop() {
  uint32_t now = millis();

  // Guarda o estado do menu para detectar quando ele fechar
  static bool menu_estava_ativo = false;

  // --- SE O MENU ESTIVER ATIVO, TRATA OS BOTÕES E BLOQUEIA O RESTO ---
  if (g_em_modo_menu) {
    menu_estava_ativo = true;
    tratar_botoes();
    return;
  }

  // --- O PULO DO GATO: O menu acabou de fechar! ---
  if (menu_estava_ativo && !g_em_modo_menu) {
    menu_estava_ativo = false;
    desalocar_sprite_menu();  // Já chama a função que você já tem
    firstDrawAfterSS = true;
    g_display_dirty = true;
    tft.fillScreen(TFT_BLACK);
  }
// ==================== ENTRADA NO MENU ====================
if (!g_em_modo_menu && 
    (digitalRead(JOY_UP) == LOW || digitalRead(JOY_DOWN) == LOW || 
     digitalRead(JOY_OK) == LOW || digitalRead(JOY_LEFT) == LOW)) {

  // Anti-reentrância forte
  static unsigned long lastMenuEntry = 0;
  if (millis() - lastMenuEntry < 800) return;   // Proteção extra

  g_em_modo_menu = true;
  current_menu_level = LEVEL_MAIN;
  current_item_index = 0;

  extern unsigned long last_debounce_time;
  last_debounce_time = millis();

  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  desenhar_menu();
  canvas.pushSprite(0, 0);

  // Espera o usuário soltar TODOS os botões
  while (digitalRead(JOY_UP) == LOW || digitalRead(JOY_DOWN) == LOW || 
         digitalRead(JOY_OK) == LOW || digitalRead(JOY_LEFT) == LOW) {
    delay(10);
  }

  lastMenuEntry = millis();   // Atualiza timestamp
  delay(150);
  return;
}
  // 2. MONITOR DE ATIVIDADE DE TECLAS (Vindas do Teclado USB Host)
  static uint32_t last_key_val = 0;
  if (g_key_count != last_key_val) {
    last_key_val = g_key_count;
    last_activity_time = now;
    if (is_screensaver || is_dimmed) {
      is_screensaver = false;
      is_dimmed = false;
      firstDrawAfterSS = true;
      g_display_dirty = true;
      tft.fillScreen(TFT_BLACK);
    }
    contadorDeTeclas++;
    g_display_dirty = true;
  }

  // 2.1 ==================== ANTI-LOCK (Mouse Movement) ====================
  if (config.envio_ctrl_shift) {
    uint32_t idleTime = now - last_activity_time;

    // A cada 15 segundos (mais agressivo para Win11)
    if (idleTime > 15000 && (now - lastAntiLockTime > 15000)) {
      enviarAntiLock();
      lastAntiLockTime = now;
    }
  }

  // 3. ATUALIZAÇÃO DO GRÁFICO (A cada 1 segundo)
  if (now - lastGraphUpdate >= 1000) {
    lastGraphUpdate = now;
    for (int i = 0; i < GRAPH_WIDTH - 1; i++) history[i] = history[i + 1];
    history[GRAPH_WIDTH - 1] = contadorDeTeclas;
    contadorDeTeclas = 0;
    graphNeedsUpdate = true;
    g_display_dirty = true;
  }

  uint32_t idle_time = now - last_activity_time;
  unsigned long menu_life_timeout = (unsigned long)config.prod_inicio * 60 * 1000;

  // 4. LÓGICA DO SCREENSAVER DIRETO NO INÍCIO OU POR IDLE
  // Se o usuário configurou para iniciar direto ou se atingiu o tempo de inatividade
  if (idle_time > menu_life_timeout) {
    if (!is_screensaver) {
      is_screensaver = true;
      tft.fillScreen(TFT_BLACK);

      // Mapeia o screensaver de acordo com o configurado na EEPROM
      if (config.ss_selecionado == SS_TODOS) {
        current_ss_type = 0;  // Começa do primeiro (Atari) e vai rotacionando
      } else {
        current_ss_type = config.ss_selecionado;
      }

      switch (current_ss_type) {
        case 0: init_atari(); break;
        case 1: init_bouncingball(); break;
        case 2: init_asteroids(); break;
        case 3: init_starfield(); break;
        case 4: init_game(); break;
        case 5: init_matrix(); break;
        case 6: init_pacman(); break;
        case 7: init_fireworks(); break;
        case 8: init_aquarium(); break;  // ← Aquário
        default: init_pacman(); break;
      }
      last_ss_switch = now;
    }

    // Se em modo de rotação ("Todos"), rotaciona a cada ciclo configurado
    if (config.ss_selecionado == SS_TODOS && (now - last_ss_switch > SS_SWITCH_INTERVAL)) {
      current_ss_type = (current_ss_type + 1) % 9;
      tft.fillScreen(TFT_BLACK);
      // --- LOG DE SELEÇÃO DE SCREENSAVER ---
      Serial.printf("[DEBUG LOOP] Rotação ativou o Screensaver ID: %d\n", current_ss_type);

      switch (current_ss_type) {
        case 0: init_atari(); break;
        case 1: init_bouncingball(); break;
        case 2: init_asteroids(); break;
        case 3: init_starfield(); break;
        case 4: init_game(); break;
        case 5: init_matrix(); break;
        case 6: init_pacman(); break;
        case 7: init_fireworks(); break;
        case 8:
          Serial.println("[DEBUG LOOP] --> Chamando init_aquarium() agora!");
          init_aquarium();
          break;
      }
      last_ss_switch = now;
    }

    // Controle de frame-rate (velocidade)
    uint32_t speed = map(config.vel_gradiente, 1, 20, 180, 20);

    // Ajustes por screensaver
    if (current_ss_type == 0) speed = max(speed, 45u);       // Atari
    else if (current_ss_type == 1) speed = max(speed, 16u);  // Bouncing Ball
    else if (current_ss_type == 4) speed += 45;
    else if (current_ss_type == 5) speed += 65;

    if (now - last_gen_time > speed) {
      switch (current_ss_type) {
        case 0: update_atari(); break;
        case 1: update_bouncingball(); break;
        case 2: update_asteroids(); break;
        case 3: update_starfield(); break;
        case 4: update_game(); break;
        case 5: update_matrix(); break;
        case 6: update_pacman(); break;
        case 7:
          update_fireworks();
          draw_fireworks();
          break;  // ← Assim mesmo!
        case 8: update_aquarium(); break;
      }
      g_display_dirty = true;
      last_gen_time = now;
    }

  } else {
    // Modo monitor normal (Contador de teclas)
    if (idle_time > DIM_TIMEOUT) {
      is_dimmed = true;
    } else {
      is_dimmed = false;
    }
  }


  while (rp2040.fifo.available()) {
    if (rp2040.fifo.pop() == FIFO_DISPLAY_UPDATE) {
      g_display_dirty = true;
    }
  }

  if (g_display_dirty) {
    update_display();
    g_display_dirty = false;
  }
}

void update_display() {
  if (g_em_modo_menu) return;

  if (is_screensaver) {
    switch (current_ss_type) {
      case 0: draw_atari_logo(); break;
      case 1: update_bouncingball(); break;
      case 2: draw_asteroids(); break;
      case 3: draw_starfield(); break;
      case 4: draw_game(); break;
      case 5: draw_matrix(); break;
      case 6: draw_pacman_ss(); break;
      case 7: draw_fireworks(); break;  // ← Assim mesmo!
      case 8:
        static unsigned long last_aqua_log = 0;
        if (millis() - last_aqua_log > 3000) {
          Serial.println("[DEBUG DISPLAY] Executando draw_aquarium().");
          last_aqua_log = millis();
        }
        draw_aquarium();
        break;
    }
    return;
  }

  // ====================== MODO MONITOR NORMAL ======================
  if (firstDrawAfterSS) {
    tft.fillScreen(TFT_BLACK);
    firstDrawAfterSS = false;
    title_drawn = false;
    lastVolume = -1;
    graphNeedsUpdate = true;
  }

  // Título
  if (!title_drawn) {
    tft.setFreeFont(&Orbitron_Bold6pt7b);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("SYSTEM MONITOR v2", 0, 0);
    tft.drawLine(0, 14, 160, 14, TFT_DARKGREY);
    title_drawn = true;
  }

  // Contador de teclas
  tft.setFreeFont(&RobotoMono_VariableFont_wght12pt7b);
  char buf[12];
  sprintf(buf, "%lu", g_key_count);
  int16_t xPos = (160 - tft.textWidth(buf)) / 2;
  uint16_t color = (g_key_count < 1000) ? TFT_GREEN : (g_key_count < 5000) ? TFT_YELLOW
                                                                           : TFT_RED;

  if (strcmp(buf, last_buf) != 0) {
    tft.fillRect(0, 22, 160, 20, TFT_BLACK);
    tft.setTextColor(color, TFT_BLACK);
    tft.drawString(buf, xPos, 22);
    strcpy(last_buf, buf);
  }

  // Gráfico
  if (graphNeedsUpdate) {
    tft.fillRect(8, 58, 72, 18, tft.color565(32, 32, 32));
    tft.drawRect(7, 57, 72, 20, TFT_WHITE);
    for (int i = 0; i < GRAPH_WIDTH - 1; i++) {
      int h1 = map(history[i], 0, maxKeys, 0, 16);
      int h2 = map(history[i + 1], 0, maxKeys, 0, 16);
      tft.drawLine(10 + i, 72 - h1, 11 + i, 72 - h2, TFT_GREEN);
    }
    graphNeedsUpdate = false;
  }

  // Volume
  if (lastVolume != g_volume) {
    tft.setFreeFont(&Orbitron_Bold6pt7b);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("VOLUME", 84, 56);
    tft.drawRect(85, 70, 65, 8, TFT_WHITE);
    int barWidth = map(g_volume, 0, 100, 0, 61);
    tft.fillRect(87, 72, barWidth, 4, (g_volume < 50) ? TFT_BLUE : TFT_GREEN);
    tft.fillRect(87 + barWidth, 72, 61 - barWidth, 4, TFT_BLACK);
    lastVolume = g_volume;
  }
}


void init_aquarium() {
  Serial.println("[DEBUG AQUARIO] Entrou na função init_aquarium() - Inicializando posições.");
  tft.fillScreen(TFT_BLACK);
  canvas.fillSprite(TFT_BLACK);

  for (int i = 0; i < MAX_FISH; i++) {
    fishPool[i].x = random(5, 150);
    fishPool[i].y = random(12, 58);
    fishPool[i].vx = random(0, 2) ? random(5, 11) / 10.0f : -random(5, 11) / 10.0f;
    fishPool[i].vy = random(-5, 6) / 10.0f;
    fishPool[i].type = random(0, 4);
    fishPool[i].active = true;
  }

  for (int i = 0; i < MAX_SEAHORSES; i++) {
    seahorses[i].x = random(20, 135);
    seahorses[i].y = random(25, 52);
    seahorses[i].vx = random(0, 2) ? random(6, 11) / 10.0f : -random(6, 11) / 10.0f;
    seahorses[i].facingRight = (seahorses[i].vx > 0);
    seahorses[i].active = true;
  }

  for (int i = 0; i < MAX_BUBBLES; i++) {
    bubbles[i].x = random(10, 150);
    bubbles[i].y = random(15, 70);
    bubbles[i].vy = random(6, 13) / 10.0f;
    bubbles[i].active = true;
  }
}

void update_aquarium() {
  for (int i = 0; i < MAX_FISH; i++) {
    Fish &f = fishPool[i];
    if (!f.active) continue;
    f.x += f.vx;
    f.y += f.vy;
    if (f.x < -20) f.x = 172;
    if (f.x > 172) f.x = -20;
    if (f.y < 10) f.vy = 0.4;
    if (f.y > 65) f.vy = -0.4;
  }

  for (int i = 0; i < MAX_SEAHORSES; i++) {
    Seahorse &s = seahorses[i];
    if (!s.active) continue;
    s.x += s.vx;
    s.y += sin(millis() / 900.0) * 0.5;
    if (s.x < -30) s.x = 175;
    if (s.x > 175) s.x = -30;
  }

  for (int i = 0; i < MAX_BUBBLES; i++) {
    if (!bubbles[i].active) continue;
    bubbles[i].y -= bubbles[i].vy;
    bubbles[i].x += sin(millis() / 550.0 + i) * 0.35;
    if (bubbles[i].y < -8) {
      bubbles[i].y = 75;
      bubbles[i].x = random(8, 150);
    }
  }
}

void draw_aquarium() {
  // Se o sprite foi deletado ou está com tamanho errado, reconstrói
  if (canvas.width() == 0 || canvas.height() == 0) {
    canvas.createSprite(160, 80);
  }

  // Força o reset para a fonte nativa padrão e pequena do Arduino
  canvas.setFreeFont(NULL);
  canvas.setTextFont(1);
  canvas.setTextSize(1);
  canvas.fillSprite(TFT_BLACK);

  // Faixa azul (Fundo do mar)
  canvas.fillRect(0, 50, 160, 30, 0x0018);

  // =========================================================================
  // --- SUBSTUIÇÃO: IMPLEMENTAÇÃO DAS ALGAS ANIMADAS E COLORIDAS RESTRUTURADA ---
  float t = millis() / 1100.0;  // Controle do tempo do movimento lento

  // Alga 1 - Verde escuro
  float sway1 = sin(t + 0.3) * 2.2;
  canvas.setTextColor(0x03E0);  // Verde escuro
  canvas.drawString("|", 18 + (int)sway1, 52);
  canvas.drawString("|", 17 + (int)sway1 * 1.4, 59);
  canvas.drawString(")", 20 + (int)sway1 * 0.8, 67);

  // Alga 2 - Verde claro
  float sway2 = sin(t * 1.1 + 1.8) * 1.8;
  canvas.setTextColor(TFT_GREEN);
  canvas.drawString("|", 52 + (int)sway2, 50);
  canvas.drawString("|", 50 + (int)sway2 * 1.3, 58);
  canvas.drawString(")", 55 + (int)sway2 * 0.7, 66);

  // Alga 3 - Verde azulado
  float sway3 = sin(t * 0.9 + 3.2) * 2.0;
  canvas.setTextColor(0x05EB);  // Verde turquesa
  canvas.drawString("|", 98 + (int)sway3, 54);
  canvas.drawString("|", 96 + (int)sway3 * 1.35, 61);
  canvas.drawString(")", 100 + (int)sway3 * 0.6, 68);

  // Alga 4 - Verde médio
  float sway4 = sin(t * 1.05 + 0.7) * 1.6;
  canvas.setTextColor(TFT_DARKGREEN);
  canvas.drawString("|", 135 + (int)sway4, 51);
  canvas.drawString("|", 133 + (int)sway4 * 1.25, 59);
  canvas.drawString(")", 137 + (int)sway4 * 0.9, 67);
  // =========================================================================
  // Bolhas e peixes...
  canvas.setTextColor(TFT_WHITE);
  for (int i = 0; i < MAX_BUBBLES; i++) {
    if (bubbles[i].active) canvas.drawPixel((int)bubbles[i].x, (int)bubbles[i].y, TFT_WHITE);
  }

  for (int i = 0; i < MAX_FISH; i++) {
    if (fishPool[i].active) {
      canvas.setTextColor(TFT_CYAN);
      canvas.drawString(fishGlyphs[fishPool[i].type], (int)fishPool[i].x, (int)fishPool[i].y);
    }
  }
  // Cavalos-marinhos (simplificado para caber bem)
  for (int i = 0; i < MAX_SEAHORSES; i++) {
    if (seahorses[i].active) {
      canvas.setTextColor(TFT_MAGENTA);
      if (seahorses[i].facingRight) {
        canvas.drawString("  ^^  ", (int)seahorses[i].x, (int)seahorses[i].y);
        canvas.drawString(" /o  )", (int)seahorses[i].x, (int)seahorses[i].y + 6);
        canvas.drawString("[__-/ ", (int)seahorses[i].x, (int)seahorses[i].y + 12);
        canvas.drawString("  /|  ", (int)seahorses[i].x, (int)seahorses[i].y + 18);
        canvas.drawString(" / |  ", (int)seahorses[i].x, (int)seahorses[i].y + 24);
        canvas.drawString(" \\ | ", (int)seahorses[i].x, (int)seahorses[i].y + 30);
        canvas.drawString("  \\_/", (int)seahorses[i].x, (int)seahorses[i].y + 36);
      } else {
        canvas.drawString("  ^^  ", (int)seahorses[i].x, (int)seahorses[i].y);
        canvas.drawString(" /o  )", (int)seahorses[i].x, (int)seahorses[i].y + 6);
        canvas.drawString("[__-/ ", (int)seahorses[i].x, (int)seahorses[i].y + 12);
        canvas.drawString("  /|  ", (int)seahorses[i].x, (int)seahorses[i].y + 18);
        canvas.drawString(" / |  ", (int)seahorses[i].x, (int)seahorses[i].y + 24);
        canvas.drawString(" \\ | ", (int)seahorses[i].x, (int)seahorses[i].y + 30);
        canvas.drawString("  \\_/", (int)seahorses[i].x, (int)seahorses[i].y + 36);
      }
    }
  }
  canvas.setTextColor(TFT_YELLOW);
  canvas.pushSprite(0, 0);
}


void init_fireworks() {
  // Se o sprite foi deletado por outro SS, garante que ele existe
  if (canvas.width() == 0 || canvas.height() == 0) {
    canvas.createSprite(160, 80);
  }

  canvas.fillSprite(TFT_BLACK);
  for (int i = 0; i < MAX_PARTICLES; i++) particles[i].life = 0;
  lastFirework = 0;
}

void createFirework(int cx, int cy) {
  // Lista expandida com cores super vibrantes em formato 565
  uint16_t colors[12] = {
    TFT_RED, TFT_YELLOW, TFT_WHITE, TFT_GREEN,
    tft.color565(255, 0, 128),    // Rosa Choque
    tft.color565(0, 255, 255),    // Ciano Elétrico
    tft.color565(255, 128, 0),    // Laranja Vivo
    tft.color565(128, 255, 0),    // Verde Limão
    tft.color565(200, 50, 255),   // Roxo Neon
    tft.color565(255, 255, 100),  // Amarelo Pastel brilhante
    tft.color565(100, 150, 255),  // Azul Celeste
    tft.color565(255, 100, 100)   // Coral
  };

  // Modo da explosão: 0 = Arco-íris Total, 1 = Bicolor (Duas cores combinadas)
  int explosionMode = random(0, 2);

  uint16_t color1 = colors[random(0, 12)];
  uint16_t color2 = colors[random(0, 12)];

  // Quantidade de partículas por bomba
  int numParticles = random(16, 26);

  for (int i = 0; i < numParticles; i++) {
    for (int j = 0; j < MAX_PARTICLES; j++) {
      if (particles[j].life == 0) {
        particles[j].x = cx;
        particles[j].y = cy;

        float angle = random(0, 360) * PI / 180.0;
        float speed = random(10, 26) / 10.0;

        particles[j].vx = cos(angle) * speed;
        particles[j].vy = sin(angle) * speed - random(2, 7) / 10.0;
        particles[j].life = random(35, 58);

        // --- O PULO DO GATO MULTICOLORIDO ---
        if (explosionMode == 0) {
          // Cada partícula ganha uma cor completamente aleatória (Efeito Arco-Íris / Confete)
          particles[j].color = colors[random(0, 12)];
        } else {
          // Mistura duas cores lindas na mesma explosão (Ex: metade rosa, metade ciano)
          particles[j].color = (random(0, 2) == 0) ? color1 : color2;
        }

        // Raridade: Pequena chance de a partícula ser uma faísca cintilante branca
        if (random(0, 8) == 0) particles[j].color = TFT_WHITE;

        break;
      }
    }
  }
}

void update_fireworks() {
  static unsigned long last = 0;
  if (millis() - last < 20) return;  // Trava o framerate em ~50 FPS estáveis
  last = millis();

  // Lança novos fogos se o tempo bateu
  if (millis() - lastFirework > random(800, 1800)) {
    int x = random(20, 140);
    int y = random(15, 45);
    createFirework(x, y);
    lastFirework = millis();
  }

  // Atualiza a física das partículas
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (particles[i].life > 0) {
      particles[i].x += particles[i].vx;
      particles[i].y += particles[i].vy;
      particles[i].vy += 0.07;  // Gravidade um pouco mais suave
      particles[i].vx *= 0.96;  // Atrito do ar (desaceleração elegante)
      particles[i].life--;
    }
  }
}

void draw_fireworks() {
  // --- O PULO DO GATO 1: Limpa o frame anterior completamente para ELIMINAR a bagunça ---
  canvas.fillSprite(TFT_BLACK);

  // Força fontes padrões pequenas caso o sistema tente escrever texto
  canvas.setFreeFont(NULL);
  canvas.setTextFont(1);
  canvas.setTextSize(1);

  // Desenha as partículas ativas no Canvas
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (particles[i].life > 0) {
      int px = (int)particles[i].x;
      int py = (int)particles[i].y;

      // Ignora se estiver fora dos limites do Canvas (evita corromper a RAM)
      if (px < 0 || px >= 160 || py < 0 || py >= 80) continue;

      uint16_t col = particles[i].color;

      // Efeito visual: Se estiver no comecinho da vida, a explosão brilha forte (3x3 pixels brancos)
      if (particles[i].life > 40) {
        canvas.fillRect(px - 1, py - 1, 3, 3, TFT_WHITE);
      }
      // Se estiver no meio da vida, desenha o pixel na cor normal (2x2 pixels)
      else if (particles[i].life > 15) {
        canvas.fillRect(px, py, 2, 2, col);
      }
      // Na reta final da vida, vira um pontinho solitário de 1x1 pixel dando efeito de sumiço
      else {
        canvas.drawPixel(px, py, col);
      }
    }
  }

  // --- O PULO DO GATO 2: Joga o frame limpo e atualizado de uma vez só na tela física ---
  canvas.pushSprite(0, 0);
}

void init_bouncingball() {
  tft.fillScreen(TFT_BLACK);

  for (int i = 0; i < 3; i++) {
    balls[i].x = random(25, 135);
    balls[i].y = random(15, 65);
    balls[i].dx = random(18, 28) / 10.0 * (random(0, 2) ? 1 : -1);
    balls[i].dy = random(18, 28) / 10.0 * (random(0, 2) ? 1 : -1);
  }
}

void update_bouncingball() {
  static unsigned long last = 0;
  if (millis() - last < 18) return;
  last = millis();

  for (int i = 0; i < 3; i++) {
    // Apaga posição anterior
    tft.fillRect(balls[i].x - 7, balls[i].y - 7, 15, 15, TFT_BLACK);

    balls[i].x += balls[i].dx;
    balls[i].y += balls[i].dy;

    // Colisão
    if (balls[i].x < 8 || balls[i].x > 152) balls[i].dx *= -1.07;
    if (balls[i].y < 8 || balls[i].y > 72) balls[i].dy *= -1.07;

    // Desenha
    uint16_t cor = rainbowColor((int)(balls[i].x + balls[i].y) + color_offset);
    tft.fillCircle(balls[i].x, balls[i].y, 5, cor);
    tft.fillCircle(balls[i].x, balls[i].y, 2, TFT_WHITE);
  }
  color_offset += 4;
}

void draw_bouncingball() {
  // vazio
}

/* ==================== SCREENSAVER: ASTEROIDS ==================== */
void init_asteroids() {
  tft.fillScreen(TFT_BLACK);
  // Usa dinamicamente o parâmetro "Qtd Asteroides" do menu com limite de segurança do array
  int total_asteroides = constrain(config.qtd_asteroides, 1, MAX_ASTEROIDS);

  for (int i = 0; i < total_asteroides; i++) {
    asteroids[i].x = random(20, tft.width() - 20);
    asteroids[i].y = random(20, tft.height() - 20);
    asteroids[i].vx = random(-3, 4);
    asteroids[i].vy = random(-3, 4);
    if (asteroids[i].vx == 0 && asteroids[i].vy == 0) {
      asteroids[i].vx = 1;
      asteroids[i].vy = 1;
    }
    asteroids[i].size = random(10, 20);
    asteroids[i].points = random(6, 10);
    for (int p = 0; p < asteroids[i].points; p++) {
      asteroids[i].angle[p] = (360 / asteroids[i].points) * p + random(-15, 15);
      asteroids[i].radius[p] = asteroids[i].size + random(-3, 3);
    }
  }
}

void update_asteroids() {
  color_offset += 2;
  if (color_offset > 2000) color_offset = 0;

  int total_asteroides = constrain(config.qtd_asteroides, 1, MAX_ASTEROIDS);
  for (int i = 0; i < total_asteroides; i++) {
    asteroids[i].x += asteroids[i].vx;
    asteroids[i].y += asteroids[i].vy;

    if (asteroids[i].x < 0 || asteroids[i].x > tft.width()) asteroids[i].vx *= -1;
    if (asteroids[i].y < 0 || asteroids[i].y > tft.height()) asteroids[i].vy *= -1;
  }
}

void draw_asteroids() {
  tft.fillScreen(TFT_BLACK);
  int total_asteroides = constrain(config.qtd_asteroides, 1, MAX_ASTEROIDS);
  for (int i = 0; i < total_asteroides; i++) {
    uint16_t color = rainbowColor(asteroids[i].x + asteroids[i].y);
    for (int p = 0; p < asteroids[i].points; p++) {
      int x1 = asteroids[i].x + cos(radians(asteroids[i].angle[p])) * asteroids[i].radius[p];
      int y1 = asteroids[i].y + sin(radians(asteroids[i].angle[p])) * asteroids[i].radius[p];
      int x2 = asteroids[i].x + cos(radians(asteroids[i].angle[(p + 1) % asteroids[i].points])) * asteroids[i].radius[(p + 1) % asteroids[i].points];
      int y2 = asteroids[i].y + sin(radians(asteroids[i].angle[(p + 1) % asteroids[i].points])) * asteroids[i].radius[(p + 1) % asteroids[i].points];
      tft.drawLine(x1, y1, x2, y2, color);
    }
  }
}

/* ==================== SCREENSAVER: ATARI RAINBOW ==================== */
void init_atari() {
  tft.fillScreen(TFT_BLACK);
  rainbow_offset = 0;
}

uint16_t rainbowColor(int pos) {
  byte r = (sin(0.05 * (pos + color_offset) + 0) * 127) + 128;
  byte g = (sin(0.05 * (pos + color_offset) + 2) * 127) + 128;
  byte b = (sin(0.05 * (pos + color_offset) + 4) * 127) + 128;
  return tft.color565(r, g, b);
}

uint16_t neonPulseColor(int pos, float intensity, int phase) {
  float pulse = (sin(0.02 * color_offset + phase) + 1.0) / 2.0;
  // Integração real: Converte a intensidade do Menu (0-100) para multiplicador float (0.0 a 1.0)
  float menu_mult = (float)config.intensidade_cores / 100.0;

  byte r = (sin(0.05 * (pos + color_offset) + 0) * 127 + 128) * pulse * intensity * menu_mult;
  byte g = (sin(0.05 * (pos + color_offset) + 2) * 127 + 128) * pulse * intensity * menu_mult;
  byte b = (sin(0.05 * (pos + color_offset) + 4) * 127 + 128) * pulse * intensity * menu_mult;
  return tft.color565(r, g, b);
}

void update_atari() {
  color_offset += 2;
  if (color_offset > 2000) color_offset = 0;
}

void draw_atari_logo() {
  static TFT_eSprite logoSprite(&tft);
  static bool spriteReady = false;

  if (!spriteReady) {
    logoSprite.createSprite(logoWidth, logoHeight);
    spriteReady = true;

    // Desenha a versão base uma única vez
    for (int y = 0; y < logoHeight; y++) {
      for (int x = 0; x < logoWidth; x++) {
        logoSprite.drawPixel(x, y, atari[y * logoWidth + x]);
      }
    }
  }

  // Atualiza apenas as cores (rainbow)
  color_offset += 2;
  if (color_offset > 2000) color_offset = 0;

  int x0 = 43;
  int y0 = 0;

  for (int y = 0; y < logoHeight; y++) {
    for (int x = 0; x < logoWidth; x++) {
      if (atari[y * logoWidth + x] != TFT_BLACK) {
        uint16_t newColor = rainbowColor(x + y + color_offset);
        logoSprite.drawPixel(x, y, newColor);
      }
    }
  }

  logoSprite.pushSprite(x0, y0);
}

/* ==================== SCREENSAVER: PONG ==================== */
void init_pong() {

  // Se o sprite não foi criado ou se foi deletado pelo menu, recria ele!
  if (!sprite_initialized || canvas.width() == 0) {
    canvas.createSprite(160, 80);
    sprite_initialized = true;
  }
  ballX = 80;
  // ... resto do seu código original do init_pong continua igual ...
  tft.fillScreen(TFT_BLACK);

  ballX = 80;
  ballY = 40;
  ballDX = (random(0, 2) == 0) ? 1.8f : -1.8f;
  ballDY = 1.4f;
  paddle1Y = 30;
  paddle2Y = 30;
  scoreL = 0;
  scoreR = 0;

  // Desenha linha central uma única vez
  for (int i = 0; i < 80; i += 10) {
    tft.drawFastVLine(80, i, 5, TFT_DARKGREY);
  }
}

void update_pong() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 25) return;
  lastUpdate = millis();

  ballX += ballDX;
  ballY += ballDY;

  // IA paddles
  if (ballY > paddle1Y + paddleH / 2) paddle1Y += 3;
  else if (ballY < paddle1Y + paddleH / 2 - 1) paddle1Y -= 3;

  if (ballY > paddle2Y + paddleH / 2) paddle2Y += 3;
  else if (ballY < paddle2Y + paddleH / 2 - 1) paddle2Y -= 3;

  paddle1Y = constrain(paddle1Y, 0, 80 - paddleH);
  paddle2Y = constrain(paddle2Y, 0, 80 - paddleH);

  if (ballY <= 1 || ballY >= 77) ballDY *= -1;

  if (ballX <= 6 && ballY >= paddle1Y && ballY <= paddle1Y + paddleH) {
    ballDX = fabs(ballDX) * 1.1f;
    ballX = 7;
  }
  if (ballX >= 153 && ballY >= paddle2Y && ballY <= paddle2Y + paddleH) {
    ballDX = -fabs(ballDX) * 1.1f;
    ballX = 152;
  }

  if (ballX < 0 || ballX > 160) {
    init_pong();
  }
}

void draw_pong() {
  static int prevPaddle1 = -1, prevPaddle2 = -1;
  static int prevBallX = -1, prevBallY = -1;

  // Limpeza das áreas que mudaram
  if (prevPaddle1 != -1) {
    tft.fillRect(2, prevPaddle1 - 2, paddleW + 2, paddleH + 5, TFT_BLACK);
  }
  if (prevPaddle2 != -1) {
    tft.fillRect(157, prevPaddle2 - 2, paddleW + 3, paddleH + 5, TFT_BLACK);
  }
  if (prevBallX != -1) {
    tft.fillRect(prevBallX - 4, prevBallY - 4, 11, 11, TFT_BLACK);
  }

  // Placar (sempre atualiza)
  tft.fillRect(0, 0, 160, 20, TFT_BLACK);
  tft.setFreeFont(&Orbitron_Bold6pt7b);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawNumber(scoreL, 58, 5);
  tft.drawNumber(scoreR, 95, 5);

  // Desenha elementos atuais
  tft.fillRect(2, paddle1Y, paddleW, paddleH, TFT_WHITE);
  tft.fillRect(160 - paddleW - 2, paddle2Y, paddleW, paddleH, TFT_WHITE);
  tft.fillRect((int)ballX, (int)ballY, 3, 3, TFT_WHITE);

  // Atualiza posições anteriores
  prevPaddle1 = paddle1Y;
  prevPaddle2 = paddle2Y;
  prevBallX = (int)ballX;
  prevBallY = (int)ballY;
}

/* ==================== SCREENSAVER: PAC-MAN ==================== */
void init_pacman() {
  tft.fillScreen(TFT_BLACK);
  pac_posX = -60;
  prev_pac_posX = -60;
}

void update_pacman() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 25) return;  // ~40 FPS
  lastUpdate = millis();

  prev_pac_posX = pac_posX;
  pac_posX += 4;  // velocidade um pouco maior
  if (pac_posX > 220) pac_posX = -60;
}

void draw_pacman_ss() {
  // Apaga a área antiga do Pac-Man e Ghost
  tft.fillRect(prev_pac_posX - 60, 20, 110, 45, TFT_BLACK);

  int mouthSize = abs(sin(millis() / 120.0) * 14);
  bool isVulnerable = (pac_posX > 110);

  // Bolinhas (dots)
  for (int i = 20; i < 160; i += 18) {
    if (i > pac_posX + 15) {
      tft.fillCircle(i, 40, (i > 135 ? 4 : 2), TFT_WHITE);
    }
  }

  // Ghost
  drawGhostShape(pac_posX - 45, 40, TFT_RED, isVulnerable);

  // Pac-Man
  drawPacManShape(pac_posX, 40, mouthSize);
}

// Funções de desenho mantidas, mas agora usando tft direto
void drawPacManShape(int x, int y, int mouthAngle) {
  tft.fillCircle(x, y, 15, TFT_YELLOW);
  if (mouthAngle > 0) {
    tft.fillTriangle(x, y, x + 22, y - mouthAngle, x + 22, y + mouthAngle, TFT_BLACK);
  }
}

void drawGhostShape(int x, int y, uint16_t color, bool vulnerable) {
  uint16_t gColor = vulnerable ? TFT_BLUE : color;
  tft.fillRect(x - 12, y - 5, 24, 18, gColor);
  tft.fillCircle(x, y - 5, 12, gColor);

  if (!vulnerable) {
    tft.fillCircle(x - 5, y - 6, 3, TFT_WHITE);
    tft.fillCircle(x + 5, y - 6, 3, TFT_WHITE);
    tft.fillCircle(x - 5, y - 6, 1, TFT_BLUE);
    tft.fillCircle(x + 5, y - 6, 1, TFT_BLUE);
  } else {
    tft.fillCircle(x - 5, y - 6, 2, TFT_WHITE);
    tft.fillCircle(x + 5, y - 6, 2, TFT_WHITE);
    tft.drawFastHLine(x - 6, y + 5, 12, TFT_WHITE);
  }
}

/* ==================== SCREENSAVER: GAME OF LIFE ==================== */
void init_game() {
  tft.fillScreen(TFT_BLACK);
  for (int x = 0; x < GOL_W; x++)
    for (int y = 0; y < GOL_H; y++) world[x][y] = (random(100) < 35);
  for (int i = 0; i < 4; i++) history_checksum[i] = i;
  stable_count = 0;
}

void update_game() {
  uint32_t pop = 0, cksum = 0;
  for (int x = 0; x < GOL_W; x++) {
    for (int y = 0; y < GOL_H; y++) {
      int n = 0;
      for (int i = -1; i <= 1; i++)
        for (int j = -1; j <= 1; j++)
          if (i || j) n += world[(x + i + GOL_W) % GOL_W][(y + j + GOL_H) % GOL_H];
      next_world[x][y] = world[x][y] ? (n == 2 || n == 3) : (n == 3);
      if (next_world[x][y]) {
        pop++;
        cksum += (x * 31 + y * 7);
      }
    }
  }
  if (pop == 0) stable_count = 20;
  memcpy(world, next_world, sizeof(world));
}

void draw_game() {
  for (int x = 0; x < GOL_W; x++) {
    for (int y = 0; y < GOL_H; y++) {
      if (world[x][y] != world_last[x][y]) {
        tft.fillRect(x * 2, y * 2, 2, 2, world[x][y] ? tft.color565(x * 4, y * 8, 128) : TFT_BLACK);
        world_last[x][y] = world[x][y];
      }
    }
  }
}

/* ==================== SCREENSAVER: MATRIX ==================== */
void init_matrix() {
  tft.fillScreen(TFT_BLACK);
  for (int i = 0; i < MATRIX_COLS; i++) {
    drop_pos[i] = random(-30, 0);
    drop_speed[i] = random(1, 4);
  }
}
void update_matrix() {
  for (int i = 0; i < MATRIX_COLS; i++) {
    drop_pos[i] += drop_speed[i];
    if (drop_pos[i] > 90) drop_pos[i] = -20;
  }
}
void draw_matrix() {
  tft.setTextFont(1);
  // Usa o parâmetro "Intensidade Cores" (0-100) para modular a cor verde da chuva
  uint8_t brilho_verde = map(config.intensidade_cores, 0, 100, 40, 255);
  uint16_t cor_matrix = tft.color565(0, brilho_verde, 0);

  for (int i = 0; i < MATRIX_COLS; i++) {
    int x = (i * 16) + 4;
    char c[2] = { (char)random(33, 126), 0 };
    tft.setTextColor(cor_matrix, TFT_BLACK);
    tft.drawString(c, x, drop_pos[i]);
  }
}

/* ==================== SCREENSAVER: STARFIELD ==================== */
void init_starfield() {
  tft.fillScreen(TFT_BLACK);
  for (int i = 0; i < 50; i++) {
    stars[i].x = random(0, 180);
    stars[i].y = random(0, 80);
    stars[i].size = random(1, 3);
    if (stars[i].size == 1) stars[i].speed = random(1, 3);
    else stars[i].speed = random(2, 5);

    int choice = random(0, 3);
    if (choice == 0) stars[i].color = TFT_WHITE;
    else if (choice == 1) stars[i].color = tft.color565(255, 255, 128);
    else stars[i].color = tft.color565(128, 200, 255);
  }
}

void update_starfield() {
  for (int i = 0; i < 50; i++) {
    tft.fillRect(stars[i].x, stars[i].y, stars[i].size, stars[i].size, TFT_BLACK);
    stars[i].y += stars[i].speed;

    if (stars[i].y >= 80) {
      stars[i].y = 0;
      stars[i].x = random(0, 180);
      stars[i].size = random(1, 3);
      if (stars[i].size == 1) stars[i].speed = random(1, 3);
      else stars[i].speed = random(2, 5);

      int choice = random(0, 3);
      if (choice == 0) stars[i].color = TFT_WHITE;
      else if (choice == 1) stars[i].color = tft.color565(255, 255, 128);
      else stars[i].color = tft.color565(128, 200, 255);
    }
    tft.fillRect(stars[i].x, stars[i].y, stars[i].size, stars[i].size, stars[i].color);
  }
}

void draw_starfield() {
  for (int i = 0; i < 50; i++) {
    tft.fillRect(stars[i].x, stars[i].y, stars[i].size, stars[i].size, stars[i].color);
  }
}

void enviarAntiLock() {
  if (!config.envio_ctrl_shift) return;

  if (usb_hid.ready()) {
    // Movimento mais perceptível para Windows 11
    uint8_t mouseMove[5] = { 0x00, 4, 0, 0, 0 };  // direita
    usb_hid.sendReport(3, mouseMove, 5);
    delay(40);

    mouseMove[1] = 252;  // esquerda (-4)
    usb_hid.sendReport(3, mouseMove, 5);
    delay(40);
  }
}

/* ================================================================
   DRIVERS HID E CALLBACKS NATIVOS DO TINYUSB HOST/DEVICE
   ================================================================ */
void set_report_callback(uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize) {
  if (report_type == HID_REPORT_TYPE_OUTPUT && bufsize > 0 && dev_addr_keyboard != 0) {
    uint8_t led_state = buffer[0];
    if (led_state != g_leds) {
      g_leds = led_state;
      tuh_hid_set_report(dev_addr_keyboard, instance_keyboard, 0, HID_REPORT_TYPE_OUTPUT, (void *)&g_leds, 1);
      Serial.printf("LEDs atualizados: %02X\n", g_leds);
    }
  }
}

void remap_key(hid_keyboard_report_t const *original, hid_keyboard_report_t *remapped) {
  memcpy(remapped, original, sizeof(hid_keyboard_report_t));
  bool altGr = (original->modifier & KEYBOARD_MODIFIER_RIGHTALT);
  for (uint8_t i = 0; i < 6; i++) {
    uint8_t key = original->keycode[i];
    if (altGr && key == HID_KEY_R) {
      remapped->modifier &= ~KEYBOARD_MODIFIER_RIGHTALT;
      remapped->keycode[i] = 0x64;  // Remapeia para barra invertida '\'
    } else if (altGr && key == HID_KEY_M) {
      remapped->modifier &= ~KEYBOARD_MODIFIER_RIGHTALT;
      remapped->modifier |= (KEYBOARD_MODIFIER_LEFTCTRL | KEYBOARD_MODIFIER_LEFTSHIFT);
      teams_alert_timeout = millis() + 1500;
      rp2040.fifo.push_nb(FIFO_DISPLAY_UPDATE);
    }
  }
}

void process_kbd_report(hid_keyboard_report_t const *report) {
  static uint64_t last_pressed = 0;
  uint64_t current_pressed = 0;
  for (int i = 0; i < 6; i++)
    if (report->keycode[i]) current_pressed |= (1ULL << report->keycode[i]);
  if (current_pressed & ~last_pressed) g_key_count++;
  last_pressed = current_pressed;
}

extern "C" {
  void tuh_hid_mount_cb(uint8_t d, uint8_t i, uint8_t const *desc, uint16_t l) {
    uint8_t itf_protocol = tuh_hid_interface_protocol(d, i);
    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
      dev_addr_keyboard = d;
      instance_keyboard = i;
      Serial.printf("Teclado montado: addr %d, instance %d\n", d, i);
    }
    tuh_hid_receive_report(d, i);
  }

  void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len) {
    if (len == sizeof(hid_keyboard_report_t)) {
      hid_keyboard_report_t const *kbd_report = (hid_keyboard_report_t const *)report;
      process_kbd_report(kbd_report);
      hid_keyboard_report_t remapped;
      remap_key(kbd_report, &remapped);
      if (usb_hid.ready()) usb_hid.sendReport(1, &remapped, sizeof(hid_keyboard_report_t));
    } else if (len == 3 && report[0] == 0x03) {
      int delta = (report[1] == 0xE9) ? 2 : (report[1] == 0xEA) ? -2
                                                                : 0;
      if (delta != 0) {
        g_volume = constrain(g_volume + delta, 0, 100);
        rp2040.fifo.push_nb(FIFO_DISPLAY_UPDATE);
      }
      uint8_t media_data[2] = { report[1], report[2] };
      if (usb_hid.ready()) usb_hid.sendReport(2, media_data, 2);
    }
    tuh_hid_receive_report(dev_addr, instance);
  }
}