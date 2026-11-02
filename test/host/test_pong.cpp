// Teste de host da simulação do Pong: joga milhares de partidas e confere a física.
#include <cstdio>
#include <cstdlib>
#include "../../pong_game.cpp"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("  FALHOU linha %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main() {
  PongGame g;
  g.reset(12345);
  long frames = 0, points = 0, games = 0, hits_total = 0, max_point_frames = 0, cur_point_frames = 0;
  long wins[2] = { 0, 0 }, pts[2] = { 0, 0 };
  int prev_score[2] = { 0, 0 };
  int max_rally = 0, bad_ball = 0, bad_paddle = 0;
  PongGame::State prev = g.state();
  const long N = 3000000;   // ~28 h de screensaver a 30 quadros/s
  for (frames = 0; frames < N; frames++) {
    g.step();
    cur_point_frames++;
    if (g.state() == PongGame::PLAY) {
      if (g.ball_y() < -0.01f || g.ball_y() > PongGame::H - PongGame::BALL + 0.01f) bad_ball++;
      if (g.rally() > max_rally) max_rally = g.rally();
    }
    for (int s = 0; s < 2; s++)
      if (g.paddle_y(s) < 0 || g.paddle_y(s) > PongGame::H - PongGame::PADDLE_H) bad_paddle++;
    if (prev == PongGame::PLAY && (g.state() == PongGame::POINT || g.state() == PongGame::GAME_OVER)) {
      points++;
      hits_total += g.rally();
      for (int s = 0; s < 2; s++) if (g.score(s) > prev_score[s]) pts[s]++;
      if (cur_point_frames > max_point_frames) max_point_frames = cur_point_frames;
      cur_point_frames = 0;
    }
    if (prev == PongGame::PLAY && g.state() == PongGame::GAME_OVER) {
      games++;
      int w = g.winner();
      if (w == 0 || w == 1) wins[w]++;
      CHECK(g.winner() >= 0 && g.score(g.winner()) == PongGame::WIN_SCORE);
    }
    prev_score[0] = g.score(0); prev_score[1] = g.score(1);
    prev = g.state();
  }
  double avg_hits = points ? (double)hits_total / points : 0;
  double secs_per_point = points ? (frames / 30.0) / points : 0;
  printf("quadros=%ld pontos=%ld partidas=%ld (vitorias %ld x %ld, pontos %ld x %ld)\n",
         frames, points, games, wins[0], wins[1], pts[0], pts[1]);
  printf("rebatidas por ponto: media %.1f, maxima %d; ponto mais longo %ld quadros; %.1f s por ponto a 30 fps\n",
         avg_hits, max_rally, max_point_frames, secs_per_point);
  CHECK(bad_ball == 0);
  CHECK(bad_paddle == 0);
  CHECK(points > 1000);
  CHECK(games > 50);
  CHECK(wins[0] > games / 4 && wins[1] > games / 4);     // ninguém ganha sempre
  CHECK(avg_hits >= 3 && avg_hits <= 20);                 // trocas de bola com graça, sem ser infinitas
  CHECK(max_point_frames < 30 * 120);                     // nenhum ponto passa de 2 minutos
  printf("%s (%d falhas)\n", fails ? "FALHOU" : "TODOS OS TESTES PASSARAM", fails);
  return fails != 0;
}
