/*********************************************************************
 * screensavers.cpp
 *
 * Regra geral: cada frame() redesenha a tela INTEIRA no canvas.
 * Assim nada "suja" a tela (alertas, menu) e não há cintilação.
 *********************************************************************/
#include "screensavers.h"
#include "display.h"
#include "settings.h"
#include "clock.h"
#include "config.h"
#include "atari.h"
#include "RobotoMono12pt7b.h"

static uint8_t color_offset = 0;

static void clear() { canvas.fillSprite(TFT_BLACK); }
static void small_font() {
  canvas.setTextFont(1);
  canvas.setTextSize(1);
  canvas.setTextDatum(TL_DATUM);
}
static float frand(float a, float b) { return a + (b - a) * (random(0, 10001) / 10000.0f); }

// ===================================================================
//  ATARI (logo com arco-íris)
// ===================================================================
static const int LOGO_W = 74, LOGO_H = 80;

static void atari_init() { color_offset = 0; }

static void atari_frame() {
  clear();
  color_offset += 3;
  const int x0 = (SCREEN_W - LOGO_W) / 2;
  for (int y = 0; y < LOGO_H; y++) {
    const unsigned short *row = &atari[y * LOGO_W];
    for (int x = 0; x < LOGO_W; x++) {
      if (row[x] != 0) canvas.drawPixel(x0 + x, y, rainbow((uint8_t)(x + y + color_offset)));
    }
  }
}

// ===================================================================
//  BOLAS (3 bolas quicando)
// ===================================================================
struct Ball { float x, y, dx, dy; };
static Ball balls[3];

static void balls_init() {
  for (auto &b : balls) {
    b.x = random(15, SCREEN_W - 15);
    b.y = random(10, SCREEN_H - 10);
    b.dx = frand(1.4f, 2.6f) * (random(2) ? 1 : -1);
    b.dy = frand(1.2f, 2.2f) * (random(2) ? 1 : -1);
  }
}

static void balls_frame() {
  clear();
  color_offset += 2;
  const float R = 5;
  for (int i = 0; i < 3; i++) {
    Ball &b = balls[i];
    b.x += b.dx;
    b.y += b.dy;
    // Quica com um pouco de variação, mas com a velocidade limitada
    // (antes ela crescia 7% a cada batida até as bolas sumirem).
    if (b.x < R || b.x > SCREEN_W - R) {
      b.dx = -b.dx * frand(0.95f, 1.08f);
      b.x = constrain(b.x, R, SCREEN_W - R);
    }
    if (b.y < R || b.y > SCREEN_H - R) {
      b.dy = -b.dy * frand(0.95f, 1.08f);
      b.y = constrain(b.y, R, SCREEN_H - R);
    }
    b.dx = constrain(fabsf(b.dx), 1.0f, 3.5f) * (b.dx < 0 ? -1 : 1);
    b.dy = constrain(fabsf(b.dy), 0.8f, 3.0f) * (b.dy < 0 ? -1 : 1);

    uint16_t cor = rainbow((uint8_t)((int)(b.x + b.y) + color_offset + i * 85));
    canvas.fillCircle((int)b.x, (int)b.y, (int)R, cor);
    canvas.fillCircle((int)b.x - 1, (int)b.y - 1, 2, TFT_WHITE);
  }
}

// ===================================================================
//  PONG (dois jogadores automáticos)
// ===================================================================
static float pong_bx, pong_by, pong_dx, pong_dy;
static float pong_p1, pong_p2;
static int pong_err1, pong_err2;
static uint8_t pong_s1, pong_s2;
static const int PADDLE_H = 16;

static void pong_serve(int dir) {
  pong_bx = SCREEN_W / 2;
  pong_by = random(20, SCREEN_H - 20);
  pong_dx = 1.8f * dir;
  pong_dy = frand(-1.3f, 1.3f);
  pong_err1 = random(-6, 7);
  pong_err2 = random(-6, 7);
}

static void pong_init() {
  pong_p1 = pong_p2 = (SCREEN_H - PADDLE_H) / 2;
  pong_s1 = pong_s2 = 0;
  pong_serve(random(2) ? 1 : -1);
}

static void pong_move_paddle(float &p, int err, bool coming) {
  float target = (coming ? pong_by : SCREEN_H / 2) - PADDLE_H / 2 + err;
  float speed = coming ? 1.9f : 0.8f;
  if (p < target - 1) p += speed;
  else if (p > target + 1) p -= speed;
  p = constrain(p, 0, SCREEN_H - PADDLE_H);
}

static void pong_frame() {
  pong_bx += pong_dx;
  pong_by += pong_dy;
  if (pong_by < 1 || pong_by > SCREEN_H - 3) pong_dy = -pong_dy;

  pong_move_paddle(pong_p1, pong_err1, pong_dx < 0);
  pong_move_paddle(pong_p2, pong_err2, pong_dx > 0);

  if (pong_dx < 0 && pong_bx <= 6 && pong_by + 3 >= pong_p1 && pong_by <= pong_p1 + PADDLE_H) {
    pong_dx = min(fabsf(pong_dx) * 1.08f, 4.0f);
    pong_dy += (pong_by - (pong_p1 + PADDLE_H / 2)) * 0.12f;
    pong_err2 = random(-10, 11);  // erro do adversário na próxima jogada
  }
  if (pong_dx > 0 && pong_bx >= SCREEN_W - 9 && pong_by + 3 >= pong_p2 && pong_by <= pong_p2 + PADDLE_H) {
    pong_dx = -min(fabsf(pong_dx) * 1.08f, 4.0f);
    pong_dy += (pong_by - (pong_p2 + PADDLE_H / 2)) * 0.12f;
    pong_err1 = random(-10, 11);
  }
  pong_dy = constrain(pong_dy, -2.2f, 2.2f);

  if (pong_bx < -4) { pong_s2++; pong_serve(1); }
  if (pong_bx > SCREEN_W + 4) { pong_s1++; pong_serve(-1); }
  if (pong_s1 > 9 || pong_s2 > 9) pong_init();

  clear();
  for (int y = 0; y < SCREEN_H; y += 8) canvas.drawFastVLine(SCREEN_W / 2, y, 4, TFT_DARKGREY);
  small_font();
  canvas.setTextSize(2);
  canvas.setTextColor(TFT_DARKGREY);
  canvas.drawNumber(pong_s1, SCREEN_W / 2 - 22, 2);
  canvas.drawNumber(pong_s2, SCREEN_W / 2 + 12, 2);
  canvas.fillRect(2, (int)pong_p1, 3, PADDLE_H, TFT_WHITE);
  canvas.fillRect(SCREEN_W - 5, (int)pong_p2, 3, PADDLE_H, TFT_WHITE);
  canvas.fillRect((int)pong_bx, (int)pong_by, 3, 3, TFT_WHITE);
}

// ===================================================================
//  ASTEROIDS (polígonos girando, atravessam as bordas)
// ===================================================================
struct Asteroid {
  float x, y, vx, vy;
  uint8_t rot;
  int8_t spin;
  uint8_t points;
  uint8_t ang[10];
  uint8_t rad[10];
};
static Asteroid asteroids[MAX_ASTEROIDS];
static uint8_t n_asteroids = 0;

static void asteroids_init() {
  n_asteroids = constrain(settings.ss_asteroids, 1, MAX_ASTEROIDS);
  for (int i = 0; i < n_asteroids; i++) {
    Asteroid &a = asteroids[i];
    a.x = random(0, SCREEN_W);
    a.y = random(0, SCREEN_H);
    do {
      a.vx = frand(-1.2f, 1.2f);
      a.vy = frand(-0.9f, 0.9f);
    } while (fabsf(a.vx) + fabsf(a.vy) < 0.4f);
    a.rot = random(256);
    a.spin = random(-3, 4);
    a.points = random(7, 11);
    uint8_t size = random(5, 13);
    for (int p = 0; p < a.points; p++) {
      a.ang[p] = (uint8_t)(p * 256 / a.points + random(-8, 9));
      a.rad[p] = max(3, size + (int)random(-2, 3));
    }
  }
}

static void asteroids_frame() {
  clear();
  color_offset += 2;
  for (int i = 0; i < n_asteroids; i++) {
    Asteroid &a = asteroids[i];
    a.x += a.vx;
    a.y += a.vy;
    a.rot += a.spin;
    if (a.x < -15) a.x += SCREEN_W + 30;
    if (a.x > SCREEN_W + 15) a.x -= SCREEN_W + 30;
    if (a.y < -15) a.y += SCREEN_H + 30;
    if (a.y > SCREEN_H + 15) a.y -= SCREEN_H + 30;

    uint16_t color = rainbow((uint8_t)(i * 23 + color_offset));
    int px0 = 0, py0 = 0, pxl = 0, pyl = 0;
    for (int p = 0; p < a.points; p++) {
      uint8_t ang = a.ang[p] + a.rot;
      int px = (int)a.x + icos(ang) * a.rad[p] / 127;
      int py = (int)a.y + isin(ang) * a.rad[p] / 127;
      if (p == 0) { px0 = px; py0 = py; }
      else canvas.drawLine(pxl, pyl, px, py, color);
      pxl = px;
      pyl = py;
    }
    canvas.drawLine(pxl, pyl, px0, py0, color);
  }
}

// ===================================================================
//  ESTRELAS (viagem em "dobra espacial")
// ===================================================================
#define N_STARS 70
struct Star { int16_t x, y; uint16_t z; };
static Star stars[N_STARS];

static void star_spawn(Star &s, bool anywhere) {
  s.x = random(-800, 801);
  s.y = random(-400, 401);
  s.z = anywhere ? random(20, 512) : 511;
}

static void stars_init() {
  for (auto &s : stars) star_spawn(s, true);
}

static void stars_frame() {
  clear();
  for (auto &s : stars) {
    s.z = (s.z > 8) ? s.z - 8 : 0;
    if (s.z < 10) { star_spawn(s, false); continue; }
    int px = SCREEN_W / 2 + (int32_t)s.x * 64 / s.z;
    int py = SCREEN_H / 2 + (int32_t)s.y * 64 / s.z;
    if (px < 0 || px >= SCREEN_W || py < 0 || py >= SCREEN_H) { star_spawn(s, false); continue; }
    uint8_t b = 255 - (s.z / 2);
    uint16_t c = tft.color565(b, b, min(255, b + 40));
    if (s.z < 120) canvas.fillRect(px, py, 2, 2, c);
    else canvas.drawPixel(px, py, c);
  }
}

// ===================================================================
//  VIDA (Game of Life, 80x40 células de 2x2)
// ===================================================================
#define GOL_W 80
#define GOL_H 40
static uint8_t world[GOL_W][GOL_H];
static uint8_t next_world[GOL_W][GOL_H];
static uint32_t gol_hist[4];
static uint8_t gol_hist_pos = 0;
static uint8_t gol_stable = 0;
static uint16_t gol_gen = 0;

static void gol_init() {
  for (int x = 0; x < GOL_W; x++)
    for (int y = 0; y < GOL_H; y++) world[x][y] = random(100) < 33;
  for (int i = 0; i < 4; i++) gol_hist[i] = 0xFFFFFFFF - i;
  gol_stable = 0;
  gol_gen = 0;
  color_offset = random(256);
}

static void gol_frame() {
  uint32_t pop = 0, cksum = 0;
  for (int x = 0; x < GOL_W; x++) {
    int xl = (x + GOL_W - 1) % GOL_W, xr = (x + 1) % GOL_W;
    for (int y = 0; y < GOL_H; y++) {
      int yu = (y + GOL_H - 1) % GOL_H, yd = (y + 1) % GOL_H;
      int n = world[xl][yu] + world[x][yu] + world[xr][yu] + world[xl][y] + world[xr][y] +
              world[xl][yd] + world[x][yd] + world[xr][yd];
      uint8_t alive = world[x][y] ? (n == 2 || n == 3) : (n == 3);
      next_world[x][y] = alive;
      if (alive) {
        pop++;
        cksum = cksum * 31 + (x * GOL_H + y);
      }
    }
  }
  memcpy(world, next_world, sizeof(world));

  // Detecta padrões que se repetem (período até 4) ou população mínima
  bool repeat = false;
  for (int i = 0; i < 4; i++) repeat |= (gol_hist[i] == cksum);
  gol_hist[gol_hist_pos] = cksum;
  gol_hist_pos = (gol_hist_pos + 1) % 4;
  gol_stable = repeat ? gol_stable + 1 : 0;
  if (gol_stable > 25 || pop < 8 || ++gol_gen > 1500) gol_init();

  clear();
  for (int x = 0; x < GOL_W; x++)
    for (int y = 0; y < GOL_H; y++)
      if (world[x][y]) canvas.fillRect(x * 2, y * 2, 2, 2, rainbow((uint8_t)(x + y + color_offset)));
}

// ===================================================================
//  MATRIX (chuva de caracteres com rastro)
// ===================================================================
#define MX_COLS (SCREEN_W / 6)
#define MX_ROWS (SCREEN_H / 8)
static char mx_char[MX_COLS][MX_ROWS];
static uint8_t mx_bright[MX_COLS][MX_ROWS];
static int8_t mx_head[MX_COLS];
static uint8_t mx_speed[MX_COLS], mx_count[MX_COLS], mx_len[MX_COLS];

static char mx_rand_char() { return (char)random(33, 127); }

static void matrix_init() {
  memset(mx_bright, 0, sizeof(mx_bright));
  for (int c = 0; c < MX_COLS; c++) {
    mx_head[c] = -random(0, 15);
    mx_speed[c] = random(1, 4);
    mx_count[c] = 0;
    mx_len[c] = random(4, 12);
  }
}

static void matrix_frame() {
  for (int c = 0; c < MX_COLS; c++)
    for (int r = 0; r < MX_ROWS; r++) mx_bright[c][r] = mx_bright[c][r] > 28 ? mx_bright[c][r] - 28 : 0;

  for (int c = 0; c < MX_COLS; c++) {
    if (++mx_count[c] < mx_speed[c]) continue;
    mx_count[c] = 0;
    mx_head[c]++;
    if (mx_head[c] >= 0 && mx_head[c] < MX_ROWS) {
      mx_char[c][mx_head[c]] = mx_rand_char();
      mx_bright[c][mx_head[c]] = 255;
    }
    if (mx_head[c] > MX_ROWS + mx_len[c]) {
      mx_head[c] = -random(0, 10);
      mx_speed[c] = random(1, 4);
      mx_len[c] = random(4, 12);
    }
  }
  // Pequenas "falhas" aleatórias nos caracteres
  int gc = random(MX_COLS), gr = random(MX_ROWS);
  if (mx_bright[gc][gr]) mx_char[gc][gr] = mx_rand_char();

  clear();
  small_font();
  uint8_t gmax = map(settings.ss_color, 10, 100, 90, 255);
  for (int c = 0; c < MX_COLS; c++) {
    for (int r = 0; r < MX_ROWS; r++) {
      uint8_t b = mx_bright[c][r];
      if (!b) continue;
      uint16_t col = (b == 255) ? tft.color565(gmax * 3 / 4, gmax, gmax * 3 / 4) : tft.color565(0, b * gmax / 255, 0);
      canvas.drawChar(c * 6 + 2, r * 8, mx_char[c][r], col, col, 1);
    }
  }
}

// ===================================================================
//  PAC-MAN (perseguição em duas fases)
// ===================================================================
static float pac_x, ghost_x;
static int8_t pac_dir;          // +1 direita, -1 esquerda
static uint8_t pac_phase;       // 0 = fantasma persegue; 1 = Pac-Man persegue
static bool ghost_eaten;
static uint32_t eaten_at;
static bool dots[12];
static const int PAC_Y = 42, DOT_X0 = 10, DOT_DX = 13, POWER_DOT = 10;

static void pac_init() {
  pac_phase = 0;
  pac_dir = 1;
  pac_x = -20;
  ghost_x = pac_x - 40;
  ghost_eaten = false;
  for (auto &d : dots) d = true;
}

static void draw_pac(int x, int y, int8_t dir) {
  int mouth = abs((int)isin((uint8_t)(millis() / 4))) * 9 / 127;
  canvas.fillCircle(x, y, 10, TFT_YELLOW);
  if (mouth > 0) canvas.fillTriangle(x, y, x + dir * 14, y - mouth, x + dir * 14, y + mouth, TFT_BLACK);
}

static void draw_ghost(int x, int y, bool vulnerable, int8_t look) {
  uint16_t c = vulnerable ? TFT_BLUE : TFT_RED;
  if (vulnerable && (millis() / 200) % 2 && pac_phase == 1 && pac_x - ghost_x < 40) c = TFT_WHITE;
  canvas.fillCircle(x, y - 2, 10, c);
  canvas.fillRect(x - 10, y - 2, 21, 11, c);
  for (int i = 0; i < 3; i++) canvas.fillTriangle(x - 10 + i * 7, y + 9, x - 7 + i * 7, y + 5, x - 3 + i * 7, y + 9, TFT_BLACK);
  if (vulnerable) {
    canvas.fillRect(x - 5, y - 5, 3, 3, TFT_WHITE);
    canvas.fillRect(x + 3, y - 5, 3, 3, TFT_WHITE);
    for (int i = -6; i < 6; i += 3) canvas.drawLine(x + i, y + 3, x + i + 1, y + 2, TFT_WHITE);
  } else {
    canvas.fillCircle(x - 4, y - 4, 3, TFT_WHITE);
    canvas.fillCircle(x + 4, y - 4, 3, TFT_WHITE);
    canvas.fillCircle(x - 4 + look, y - 4, 1, TFT_BLUE);
    canvas.fillCircle(x + 4 + look, y - 4, 1, TFT_BLUE);
  }
}

static void pac_frame() {
  if (pac_phase == 0) {
    pac_x += 2.0f;
    ghost_x = pac_x - 38;
    for (int i = 0; i < 12; i++)
      if (dots[i] && pac_x >= DOT_X0 + i * DOT_DX) dots[i] = false;
    if (pac_x > SCREEN_W + 70) {  // volta: agora o fantasma foge
      pac_phase = 1;
      pac_dir = -1;
      ghost_x = SCREEN_W + 15;
      pac_x = ghost_x + 50;
    }
  } else {
    if (!ghost_eaten) {
      ghost_x -= 1.2f;
      pac_x -= 2.0f;
      if (pac_x - ghost_x < 6) {
        ghost_eaten = true;
        eaten_at = millis();
      }
    } else if (millis() - eaten_at > 900) {
      pac_x -= 2.0f;
    }
    if (pac_x < -30) pac_init();
  }

  clear();
  canvas.drawFastHLine(0, PAC_Y - 15, SCREEN_W, TFT_NAVY);
  canvas.drawFastHLine(0, PAC_Y + 14, SCREEN_W, TFT_NAVY);
  for (int i = 0; i < 12; i++) {
    if (!dots[i]) continue;
    int dx = DOT_X0 + i * DOT_DX;
    if (i == POWER_DOT) {
      if ((millis() / 250) % 2) canvas.fillCircle(dx, PAC_Y, 4, TFT_WHITE);
    } else {
      canvas.fillRect(dx - 1, PAC_Y - 1, 2, 2, TFT_WHITE);
    }
  }
  if (pac_phase == 1 && ghost_eaten) {
    if (millis() - eaten_at < 900) {
      small_font();
      canvas.setTextColor(TFT_CYAN);
      canvas.drawString("200", (int)ghost_x - 8, PAC_Y - 4);
    }
  } else {
    bool vulnerable = pac_phase == 1 || pac_x >= DOT_X0 + POWER_DOT * DOT_DX;
    draw_ghost((int)ghost_x, PAC_Y, vulnerable, pac_phase == 0 ? 1 : -1);
  }
  if (!(pac_phase == 1 && ghost_eaten && millis() - eaten_at < 900))
    draw_pac((int)pac_x, PAC_Y, pac_dir);
}

// ===================================================================
//  FOGOS (foguetes que sobem e explodem)
// ===================================================================
#define MAX_PARTICLES 90
#define MAX_ROCKETS 3
struct Particle { float x, y, vx, vy; uint8_t life; uint16_t color; };
struct Rocket { float x, y, vy; float target_y; bool active; };
static Particle particles[MAX_PARTICLES];
static Rocket rockets[MAX_ROCKETS];
static uint32_t next_launch = 0;

static const uint16_t FW_COLORS[] = {
  TFT_RED, TFT_YELLOW, TFT_GREEN, 0xF810 /*rosa*/, 0x07FF /*ciano*/,
  0xFC00 /*laranja*/, 0x87E0 /*limão*/, 0xC99F /*roxo*/, 0x5D1F /*azul*/, 0xFB2C /*coral*/
};
static const int FW_NCOLORS = sizeof(FW_COLORS) / sizeof(FW_COLORS[0]);

static void fireworks_init() {
  for (auto &p : particles) p.life = 0;
  for (auto &r : rockets) r.active = false;
  next_launch = 0;
}

static void explode(float cx, float cy) {
  bool two = random(2);
  uint16_t c1 = FW_COLORS[random(FW_NCOLORS)], c2 = FW_COLORS[random(FW_NCOLORS)];
  int n = random(18, 30);
  for (auto &p : particles) {
    if (n == 0) break;
    if (p.life) continue;
    n--;
    uint8_t ang = random(256);
    float sp = frand(0.6f, 2.2f);
    p.x = cx;
    p.y = cy;
    p.vx = icos(ang) / 127.0f * sp;
    p.vy = isin(ang) / 127.0f * sp - 0.3f;
    p.life = random(30, 55);
    p.color = two ? (random(2) ? c1 : c2) : FW_COLORS[random(FW_NCOLORS)];
    if (random(8) == 0) p.color = TFT_WHITE;
  }
}

static void fireworks_frame() {
  uint32_t now = millis();
  if ((int32_t)(now - next_launch) >= 0) {
    for (auto &r : rockets) {
      if (r.active) continue;
      r = { (float)random(20, SCREEN_W - 20), (float)SCREEN_H, frand(-2.6f, -1.8f), (float)random(12, 40), true };
      break;
    }
    next_launch = now + random(500, 1500);
  }
  for (auto &r : rockets) {
    if (!r.active) continue;
    r.y += r.vy;
    if (r.y <= r.target_y) {
      r.active = false;
      explode(r.x, r.y);
    }
  }
  for (auto &p : particles) {
    if (!p.life) continue;
    p.x += p.vx;
    p.y += p.vy;
    p.vy += 0.06f;
    p.vx *= 0.97f;
    p.life--;
  }

  clear();
  for (auto &r : rockets)
    if (r.active) {
      canvas.drawPixel((int)r.x, (int)r.y, TFT_WHITE);
      canvas.drawPixel((int)r.x, (int)r.y + 2, TFT_ORANGE);
    }
  for (auto &p : particles) {
    if (!p.life) continue;
    int px = (int)p.x, py = (int)p.y;
    if (p.life > 45) canvas.fillRect(px - 1, py - 1, 3, 3, TFT_WHITE);
    else if (p.life > 15) canvas.fillRect(px, py, 2, 2, p.color);
    else canvas.drawPixel(px, py, color_scale(p.color, p.life * 17));
  }
}

// ===================================================================
//  AQUÁRIO (ASCII)
// ===================================================================
#define MAX_FISH 6
#define MAX_BUBBLES 12
struct Fish { float x, y, vx, vy; uint8_t type; uint16_t color; };
struct Bubble { float x, y, vy; };
static Fish fish[MAX_FISH];
static Bubble bubbles[MAX_BUBBLES];
static float seahorse_x, seahorse_y, seahorse_vx;

// Cada peixe tem a versão virada para a direita e para a esquerda
static const char *FISH_R[] = { "><>", "><)'>", ">=>", "}<>" };
static const char *FISH_L[] = { "<><", "<'(><", "<=<", "<>{" };
static const uint16_t FISH_COLORS[] = { TFT_CYAN, TFT_ORANGE, TFT_YELLOW, 0xF810, TFT_GREENYELLOW };

static void aquarium_init() {
  for (auto &f : fish) {
    f.x = random(5, 150);
    f.y = random(4, 48);
    f.vx = frand(0.4f, 1.1f) * (random(2) ? 1 : -1);
    f.vy = frand(-0.3f, 0.3f);
    f.type = random(4);
    f.color = FISH_COLORS[random(5)];
  }
  for (auto &b : bubbles) {
    b.x = random(10, 150);
    b.y = random(0, SCREEN_H);
    b.vy = frand(0.4f, 1.2f);
  }
  seahorse_x = random(20, 120);
  seahorse_y = 8;
  seahorse_vx = frand(0.3f, 0.6f) * (random(2) ? 1 : -1);
}

static void aquarium_frame() {
  float t = millis() / 1000.0f;
  for (auto &f : fish) {
    f.x += f.vx;
    f.y += f.vy;
    if (f.x < -30) f.x = SCREEN_W + 5;
    if (f.x > SCREEN_W + 5) f.x = -30;
    if (f.y < 2) f.vy = fabsf(f.vy) + 0.05f;
    if (f.y > 52) f.vy = -fabsf(f.vy) - 0.05f;
    if (random(60) == 0) f.vy = frand(-0.3f, 0.3f);
  }
  for (int i = 0; i < MAX_BUBBLES; i++) {
    Bubble &b = bubbles[i];
    b.y -= b.vy;
    b.x += sinf(t * 2 + i) * 0.3f;
    if (b.y < -3) {
      b.y = SCREEN_H + 2;
      b.x = random(8, 152);
    }
  }
  seahorse_x += seahorse_vx;
  if (seahorse_x < -30) seahorse_x = SCREEN_W + 5;
  if (seahorse_x > SCREEN_W + 5) seahorse_x = -30;
  float sh_y = seahorse_y + sinf(t * 1.1f) * 3;

  clear();
  small_font();
  canvas.fillRect(0, SCREEN_H - 8, SCREEN_W, 8, 0x8B26);  // areia
  // Algas balançando
  const int ALGA_X[] = { 14, 48, 98, 136 };
  const uint16_t ALGA_C[] = { 0x03E0, TFT_GREEN, 0x05EB, TFT_DARKGREEN };
  for (int a = 0; a < 4; a++) {
    canvas.setTextColor(ALGA_C[a]);
    for (int s = 0; s < 3; s++) {
      int sway = (int)(sinf(t * 0.9f + a * 1.7f + s * 0.6f) * (1 + s));
      canvas.drawString(s == 2 ? ")" : "(", ALGA_X[a] + sway, SCREEN_H - 14 - s * 7);
    }
  }
  for (auto &b : bubbles) canvas.drawCircle((int)b.x, (int)b.y, 1, 0x9E7F);
  for (auto &f : fish) {
    canvas.setTextColor(f.color);
    canvas.drawString(f.vx >= 0 ? FISH_R[f.type] : FISH_L[f.type], (int)f.x, (int)f.y);
  }
  // Cavalo-marinho
  canvas.setTextColor(TFT_MAGENTA);
  const char *SH_R[] = { " _", "(o>", " |)", " /", " \\_/" };
  const char *SH_L[] = { " _", "<o)", "(|", "  \\", "\\_/" };
  const char **sh = seahorse_vx >= 0 ? SH_R : SH_L;
  for (int i = 0; i < 5; i++) canvas.drawString(sh[i], (int)seahorse_x, (int)sh_y + i * 7);
}

// ===================================================================
//  RELÓGIO
// ===================================================================
static void clock_init() {}

void ss_clock_draw() {
  clear();
  char buf[8];
  uint8_t drift = millis() / 100;
  int dx = isin(drift) / 32;   // move devagar alguns pixels
  int dy = icos(drift / 2) / 64;
  uint16_t col = clock_is_set() ? rainbow((uint8_t)(millis() / 60)) : TFT_DARKGREY;
  canvas.setTextColor(col, TFT_BLACK);
  canvas.setTextDatum(MC_DATUM);
#ifdef LOAD_FONT7
  // A fonte 7 (7 segmentos) só tem dígitos, ':' e '.': sem "--" nem piscar.
  if (clock_is_set()) {
    clock_format(buf, sizeof(buf), false);
    canvas.drawString(buf, SCREEN_W / 2 + dx, SCREEN_H / 2 + dy, 7);
  } else
#endif
  {
    clock_format(buf, sizeof(buf), true);
    canvas.setFreeFont(&RobotoMono_VariableFont_wght12pt7b);
    canvas.setTextSize(2);
    canvas.drawString(buf, SCREEN_W / 2 + dx, SCREEN_H / 2 + dy);
    canvas.setTextSize(1);
    canvas.setFreeFont(NULL);
  }
  canvas.setTextDatum(TL_DATUM);
  if (!clock_is_set()) {
    small_font();
    canvas.setTextColor(TFT_DARKGREY);
    canvas.setTextDatum(BC_DATUM);
    canvas.drawString("Ajuste em Menu > Relogio", SCREEN_W / 2, SCREEN_H);
    canvas.setTextDatum(TL_DATUM);
  }
}

// ===================================================================
//  TABELA
// ===================================================================
static const Screensaver SCREENSAVERS[] = {
  { "Atari", atari_init, atari_frame, 40 },
  { "Bolas", balls_init, balls_frame, 20 },
  { "Pong", pong_init, pong_frame, 20 },
  { "Asteroides", asteroids_init, asteroids_frame, 35 },
  { "Estrelas", stars_init, stars_frame, 30 },
  { "Vida", gol_init, gol_frame, 90 },
  { "Matrix", matrix_init, matrix_frame, 70 },
  { "Pac-Man", pac_init, pac_frame, 30 },
  { "Fogos", fireworks_init, fireworks_frame, 25 },
  { "Aquario", aquarium_init, aquarium_frame, 40 },
  { "Relogio", clock_init, ss_clock_draw, 250 },
};

uint8_t ss_count() { return sizeof(SCREENSAVERS) / sizeof(SCREENSAVERS[0]); }
const Screensaver &ss_get(uint8_t i) { return SCREENSAVERS[i % ss_count()]; }
