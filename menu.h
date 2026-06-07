#ifndef MENU_H
#define MENU_H

#include <Arduino.h>
#include <TFT_eSPI.h>

// ==================== PINOS DO JOYSTICK ====================
#define JOY_COMMON   1     // Pino comum do joystick (geralmente ligado ao GND)

// Direções
#define JOY_UP       2
#define JOY_DOWN     3
#define JOY_LEFT     6
#define JOY_RIGHT    5
#define JOY_OK       4     // Botão central (SELECT)

// --- ENUMS DO MENU ---
enum MenuLevel { 
  LEVEL_MAIN, 
  LEVEL_SUB_SS, 
  LEVEL_SUB_CONFIG, 
  LEVEL_SUB_PROD, 
  LEVEL_SUB_SISTEMA 
};

enum ScreensaverType { 
  SS_ATARI, SS_PONG, SS_ASTEROIDS, SS_STARFIELD, SS_GOL, 
  SS_MATRIX, SS_PACMAN, SS_FIREWORKS, SS_AQUARIUM, SS_TODOS 
};

// --- ESTRUTURA DE CONFIGURAÇÃO ---
struct Config {
  uint8_t assinatura;
  uint8_t ss_selecionado;
  int vel_gradiente;
  int qtd_asteroides;
  int intensidade_cores;
  bool envio_ctrl_shift;
  int prod_intervalo;
  int prod_inicio;
  int sistema_brilho;
};

// Variáveis externas
extern Config config;
extern bool is_editing_value;
extern MenuLevel current_menu_level;
extern int current_item_index;
extern bool g_em_modo_menu;
extern void desalocar_sprite_menu();
extern bool firstDrawAfterSS;
extern volatile bool g_display_dirty;

// Protótipos
void setup_menu();
void tratar_botoes();
void desenhar_menu();
void carregar_configuracoes();
void salvar_configuracoes();

#endif