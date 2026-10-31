/*********************************************************************
 * screensavers.h - Tabela de screensavers
 *
 * Para criar um novo screensaver: escreva init() e frame() em
 * screensavers.cpp e adicione uma linha em SCREENSAVERS[]. O menu,
 * a rotação "Todos" e o loop principal se ajustam sozinhos.
 *********************************************************************/
#ifndef SCREENSAVERS_H
#define SCREENSAVERS_H

#include <Arduino.h>

#define MAX_ASTEROIDS 20

struct Screensaver {
  const char *name;
  void (*init)();     // prepara o estado e limpa o canvas
  void (*frame)();    // avança um quadro e desenha no canvas
  uint16_t frame_ms;  // intervalo base entre quadros (velocidade 10)
};

uint8_t ss_count();
const Screensaver &ss_get(uint8_t i);

// O relógio também é usado como página do monitor
void ss_clock_draw();

#endif
