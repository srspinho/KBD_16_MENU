/*********************************************************************
 * clock.h - Relógio (hora do dia)
 *
 * Não há como o dispositivo obter a hora do PC sem um programa
 * rodando no PC (o protocolo USB HID não transporta data/hora).
 * Por isso há duas fontes:
 *   1. Ajuste manual pelo menu (Relogio > Hora / Minuto). A hora é
 *      mantida enquanto o dispositivo estiver energizado.
 *   2. Módulo RTC DS3231 opcional (I2C, com bateria): detectado
 *      automaticamente; mantém a hora mesmo desligado.
 *********************************************************************/
#ifndef CLOCK_H
#define CLOCK_H

#include <Arduino.h>

void clock_begin();
void clock_update();             // chamar no loop
bool clock_is_set();
bool clock_has_rtc();
uint32_t clock_seconds_of_day(); // 0..86399
void clock_get(uint8_t &h, uint8_t &m, uint8_t &s);
void clock_set(uint8_t h, uint8_t m, uint8_t s);
bool clock_day_changed();        // true uma vez após a meia-noite
void clock_format(char *buf, size_t n, bool blink_colon = false);

#endif
