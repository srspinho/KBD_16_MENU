/*********************************************************************
 * pong_game.h - Simulação do Pong (só a física e a "IA", sem desenho)
 *
 * Independente de Arduino/display para poder ser testada no PC
 * (test/host/test_pong.cpp). O desenho fica em screensavers.cpp.
 *********************************************************************/
#ifndef PONG_GAME_H
#define PONG_GAME_H

#include <stdint.h>

class PongGame {
public:
  static constexpr int W = 160, H = 80;           // campo (pixels)
  static constexpr int PADDLE_W = 3, PADDLE_H = 14;
  static constexpr int BALL = 3;                   // bola quadrada, como no original
  static constexpr int LEFT_X = 4;                 // x da raquete esquerda
  static constexpr int RIGHT_X = W - 4 - PADDLE_W; // x da raquete direita
  static constexpr int WIN_SCORE = 11;

  enum State : uint8_t { SERVE, PLAY, POINT, GAME_OVER };

  void reset(uint32_t seed);
  void step();                       // avança um quadro

  State state() const { return st; }
  float ball_x() const { return bx; }
  float ball_y() const { return by; }
  float paddle_y(int side) const { return py[side]; }   // 0 = esquerda, 1 = direita
  uint8_t score(int side) const { return sc[side]; }
  int winner() const { return sc[0] >= WIN_SCORE ? 0 : (sc[1] >= WIN_SCORE ? 1 : -1); }
  bool ball_visible() const;
  uint16_t timer() const { return tm; }
  uint8_t rally() const { return hits; }

private:
  float bx = 0, by = 0, vx = 0, vy = 0;
  float py[2] = { 0, 0 };
  float aim[2] = { 0, 0 };   // erro de mira de cada jogador (pixels)
  uint8_t sc[2] = { 0, 0 };
  State st = SERVE;
  uint16_t tm = 0;
  uint8_t hits = 0;
  uint32_t rng = 1;

  float rnd(float a, float b);
  void serve(int toward);
  void new_aim(int side);
  float predict_y(int side) const;
  void move_paddle(int side);
  bool hit_paddle(int side, float prev_x);
};

#endif
