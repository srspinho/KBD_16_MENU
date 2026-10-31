/*********************************************************************
 * stats.h - Estatísticas de digitação
 *
 * Conta TECLAS pressionadas (cada tecla nova conta 1; modificadores
 * como Shift/Ctrl sozinhos não contam). Não registra a ordem do que
 * foi digitado: só quantas vezes cada tecla foi usada.
 *********************************************************************/
#ifndef STATS_H
#define STATS_H

#include <Arduino.h>

#define STATS_HISTORY 60  // segundos no gráfico

void stats_begin();
void stats_on_key(uint8_t keycode);
void stats_update();                       // chamar no loop
void stats_new_day();                      // zera o contador "hoje"

uint32_t stats_session();
uint32_t stats_today();
uint32_t stats_total();
uint16_t stats_kpm();                      // teclas no último minuto
uint16_t stats_peak_kpm();
uint32_t stats_active_seconds();           // tempo digitando nesta sessão
uint32_t stats_streak_seconds();           // digitação contínua (sem pausa >= 5 min)
uint32_t stats_last_key_ms();
const uint16_t *stats_history();           // teclas/segundo, [STATS_HISTORY-1] = mais recente
bool stats_history_changed();              // true uma vez por segundo

// Ranking das teclas mais usadas (total acumulado)
uint8_t stats_top_keys(uint8_t *keys, uint32_t *counts, uint8_t n);

void stats_reset_session();
void stats_reset_all();
void stats_save_if_needed(bool force = false);

#endif
