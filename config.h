/*********************************************************************
 * config.h - Configuração de HARDWARE e opções de compilação
 *
 * Tudo que depende da sua montagem física fica aqui. As preferências
 * do usuário (brilho, screensaver, etc.) ficam em settings.h e são
 * ajustadas pelo menu e gravadas na flash.
 *********************************************************************/
#ifndef CONFIG_H
#define CONFIG_H

#define FW_VERSION "3.0"

// ==================== USB HOST (PIO-USB) ====================
// D+ do teclado. O D- é obrigatoriamente o pino seguinte (D+ + 1).
#define PIN_USB_HOST_DP 0

// Pino que liga a alimentação 5V do teclado (só em placas que têm
// esse controle). No RP2040-Zero o 5V vem direto do VBUS: deixe -1.
#define PIN_5V_EN       -1
#define PIN_5V_EN_STATE 1

// ==================== JOYSTICK (5 botões, ativos em LOW) ====================
// O pino comum do joystick vai no GND.
#define JOY_UP    2
#define JOY_DOWN  3
#define JOY_OK    4
#define JOY_RIGHT 5
#define JOY_LEFT  6

// ==================== DISPLAY ====================
// Os pinos SPI do display são configurados no TFT_eSPI (User_Setup).
// Backlight: ligue o pino BLK do display neste GPIO para o brilho e o
// escurecimento automático funcionarem. Se o BLK estiver ligado direto
// no 3V3, use -1 (o display fica sempre no brilho máximo).
// Se o seu User_Setup do TFT_eSPI definir TFT_BL, ele tem prioridade.
#define PIN_BACKLIGHT 7

// ==================== RELÓGIO (opcional) ====================
// Módulo RTC DS3231 (I2C) para manter a hora mesmo sem energia.
// Sem o módulo, a hora é ajustada pelo menu e mantida enquanto o
// dispositivo estiver ligado. A detecção é automática.
#define RTC_ENABLED 1
#define PIN_RTC_SDA 26   // I2C1 SDA
#define PIN_RTC_SCL 27   // I2C1 SCL

// ==================== BUZZER (opcional) ====================
// Buzzer passivo para os alertas (Pomodoro / pausa). -1 = sem buzzer.
#define PIN_BUZZER -1

// ==================== TECLA DA CAMADA (Fn) ====================
// Segurando esta tecla, H/J/K/L viram setas etc. (ver keymap.cpp).
// Um toque rápido nela continua funcionando normalmente.
// Ativar/desativar pelo menu: Teclado > Camada Fn.
#define LAYER_KEY HID_KEY_CAPS_LOCK

// ==================== TELA ====================
#define SCREEN_W 160
#define SCREEN_H 80

// ==================== DEPURAÇÃO ====================
// Descomente para ver mensagens na porta serial (CDC).
// #define DEBUG_LOG
#ifdef DEBUG_LOG
  #define LOG(...) Serial.printf(__VA_ARGS__)
#else
  #define LOG(...) do {} while (0)
#endif

#endif
