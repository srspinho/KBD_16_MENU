/*********************************************************************
 * pomodoro.h - Temporizador Pomodoro (foco / pausa)
 *********************************************************************/
#ifndef POMODORO_H
#define POMODORO_H

#include <Arduino.h>

enum PomoPhase : uint8_t { POMO_OFF, POMO_FOCUS, POMO_BREAK };
enum PomoEvent : uint8_t { POMO_EV_NONE, POMO_EV_FOCUS_DONE, POMO_EV_BREAK_DONE };

void pomo_start_pause();    // inicia / pausa / continua
void pomo_reset();
PomoEvent pomo_update();    // chamar no loop; informa troca de fase

PomoPhase pomo_phase();
bool pomo_paused();
uint32_t pomo_remaining_s();
uint32_t pomo_phase_total_s();
uint16_t pomo_cycles();     // focos concluídos

#endif
