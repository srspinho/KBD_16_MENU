/*********************************************************************
 * usb_bridge.h - Ponte USB: teclado (host) -> PC (device)
 *********************************************************************/
#ifndef USB_BRIDGE_H
#define USB_BRIDGE_H

#include <Arduino.h>

// Opções lidas pelo core 1 (o core 0 atualiza a partir das configurações)
extern volatile bool g_remap_enabled;
extern volatile bool g_layer_enabled;

// Core 0 (setup): registra o teclado/mídia/mouse HID no PC.
void bridge_device_begin();

// Core 1: inicializa o USB host via PIO e processa o teclado.
void bridge_host_setup();
void bridge_host_loop();

// Estado do PC (pode ser lido pelo core 0)
bool bridge_pc_connected();
bool bridge_pc_suspended();

#endif
