#include "pong_game.h"
#include <math.h>

static const float SERVE_SPEED = 1.5f;   // velocidade horizontal no saque (px/quadro)
static const float MAX_SPEED = 3.6f;
static const float SPEEDUP = 1.07f;      // cada rebatida acelera 7%
static const float MAX_VY = 2.0f;        // ângulo máximo (rebatida na ponta da raquete)
static const float PADDLE_SPEED = 2.1f;  // velocidade máxima da raquete
static const uint16_t SERVE_FRAMES = 40;
static const uint16_t POINT_FRAMES = 45;
static const uint16_t GAME_OVER_FRAMES = 160;

float PongGame::rnd(float a, float b) {
  // xorshift32: rápido e igual em qualquer plataforma
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return a + (b - a) * (float)(rng & 0xFFFF) / 65535.0f;
}

void PongGame::reset(uint32_t seed) {
  rng = seed ? seed : 0x1234567u;
  sc[0] = sc[1] = 0;
  py[0] = py[1] = (H - PADDLE_H) / 2.0f;
  serve(rnd(0, 1) < 0.5f ? 0 : 1);
}

// Erro de mira: cresce com a duração da troca de bolas e, de vez em
// quando, o jogador "erra feio". É isso que faz os pontos acontecerem.
void PongGame::new_aim(int side) {
  float spread = 2.0f + hits * 0.7f;
  if (rnd(0, 1) < 0.12f) spread += 9.0f;
  aim[side] = rnd(-spread, spread);
}

void PongGame::serve(int toward) {
  bx = (W - BALL) / 2.0f;
  by = rnd(14, H - 14 - BALL);
  vx = toward == 0 ? -SERVE_SPEED : SERVE_SPEED;
  vy = rnd(0.35f, 1.1f) * (rnd(0, 1) < 0.5f ? -1 : 1);
  hits = 0;
  new_aim(0);
  new_aim(1);
  st = SERVE;
  tm = SERVE_FRAMES;
}

bool PongGame::ball_visible() const {
  if (st == POINT || st == GAME_OVER) return false;
  if (st == SERVE) return (tm / 6) % 2 == 0;   // bola pisca antes do saque
  return true;
}

// Onde a bola vai cruzar a raquete do lado "side" (com reflexões nas paredes)
float PongGame::predict_y(int side) const {
  float face = side == 0 ? LEFT_X + PADDLE_W : RIGHT_X - BALL;
  if (vx == 0) return by;
  float t = (face - bx) / vx;
  if (t < 0) t = 0;
  float span = H - BALL;
  float y = fmodf(by + vy * t, 2 * span);
  if (y < 0) y += 2 * span;
  return y > span ? 2 * span - y : y;
}

void PongGame::move_paddle(int side) {
  bool coming = (st == PLAY || st == SERVE) && (side == 0 ? vx < 0 : vx > 0);
  float target, speed;
  if (coming) {
    target = predict_y(side) + BALL / 2.0f - PADDLE_H / 2.0f + aim[side];
    speed = PADDLE_SPEED;
  } else {
    target = (H - PADDLE_H) / 2.0f;   // volta devagar para o meio
    speed = 0.7f;
  }
  float d = target - py[side];
  if (d > speed) d = speed;
  if (d < -speed) d = -speed;
  py[side] += d;
  if (py[side] < 0) py[side] = 0;
  if (py[side] > H - PADDLE_H) py[side] = H - PADDLE_H;
}

// A bola acabou de cruzar a face da raquete? Rebate com ângulo conforme o ponto de contato.
bool PongGame::hit_paddle(int side, float prev_x) {
  float face = side == 0 ? LEFT_X + PADDLE_W : RIGHT_X - BALL;
  bool crossed = side == 0 ? (prev_x >= face && bx < face) : (prev_x <= face && bx > face);
  if (!crossed) return false;
  if (by + BALL <= py[side] || by >= py[side] + PADDLE_H) return false;
  float center = py[side] + PADDLE_H / 2.0f;
  float off = ((by + BALL / 2.0f) - center) / (PADDLE_H / 2.0f + BALL / 2.0f);   // -1..1
  float speed = fabsf(vx) * SPEEDUP;
  if (speed > MAX_SPEED) speed = MAX_SPEED;
  vx = side == 0 ? speed : -speed;
  vy = off * MAX_VY;
  bx = face;
  if (hits < 255) hits++;
  new_aim(1 - side);
  return true;
}

void PongGame::step() {
  move_paddle(0);
  move_paddle(1);

  switch (st) {
    case SERVE:
      if (--tm == 0) st = PLAY;
      break;

    case PLAY: {
      float prev_x = bx;
      bx += vx;
      by += vy;
      if (by < 0) { by = -by; vy = -vy; }
      if (by > H - BALL) { by = 2 * (H - BALL) - by; vy = -vy; }
      if (!hit_paddle(0, prev_x)) hit_paddle(1, prev_x);
      int scorer = -1;
      if (bx + BALL < 0) scorer = 1;
      else if (bx > W) scorer = 0;
      if (scorer >= 0) {
        sc[scorer]++;
        if (sc[scorer] >= WIN_SCORE) {
          st = GAME_OVER;
          tm = GAME_OVER_FRAMES;
        } else {
          st = POINT;
          tm = POINT_FRAMES;
          vx = scorer == 0 ? -1 : 1;   // o próximo saque vai para quem perdeu o ponto
        }
      }
      break;
    }

    case POINT:
      if (--tm == 0) serve(vx < 0 ? 0 : 1);
      break;

    case GAME_OVER:
      if (--tm == 0) {
        sc[0] = sc[1] = 0;
        serve(rnd(0, 1) < 0.5f ? 0 : 1);
      }
      break;
  }
}
