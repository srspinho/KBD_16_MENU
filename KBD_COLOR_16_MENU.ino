/*********************************************************************
 * PROJETO: KBD Monitor - HID Remapper & System Monitor v3
 * HARDWARE: RP2040 / RP2350 + TFT ST7735 160x80 + joystick 5 botões
 *
 *  Core 0: display, menu, estatísticas, screensavers  (app.cpp)
 *  Core 1: USB host do teclado e envios para o PC      (usb_bridge.cpp)
 *
 * Configurações da placa (Arduino IDE):
 *   Placa:      Waveshare RP2350 Zero (ou Waveshare RP2040 Zero)
 *   CPU Speed:  120 MHz (múltiplo de 12 MHz, exigido pelo PIO-USB).
 *               ATENÇÃO: no RP2350 o padrão é 150 MHz, que NÃO funciona.
 *   USB Stack:  Adafruit TinyUSB
 *
 * Pinos e opções de hardware: config.h
 *********************************************************************/
// pio_usb.h precisa vir antes de qualquer include do TinyUSB: é assim que
// o Adafruit TinyUSB detecta a biblioteca Pico-PIO-USB e ativa o USB host.
#include "pio_usb.h"
#include "app.h"
#include "usb_bridge.h"

void setup() { app_setup(); }
void loop() { app_loop(); }

void setup1() { bridge_host_setup(); }
void loop1() { bridge_host_loop(); }
