/*********************************************************************
 * buttons.h - Joystick de 5 botões com debounce e auto-repetição
 *********************************************************************/
#ifndef BUTTONS_H
#define BUTTONS_H

#include <Arduino.h>

enum Btn : uint8_t { BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_OK, BTN_COUNT };

struct BtnEvent {
  Btn btn;
  bool repeat;  // true = repetição por manter pressionado
};

void buttons_begin();
void buttons_update();             // chamar a cada volta do loop
bool buttons_next(BtnEvent &ev);   // retira o próximo evento
bool buttons_any_down();

#endif
