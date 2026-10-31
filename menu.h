/*********************************************************************
 * menu.h - Menu de configurações (orientado a dados)
 *
 * Para adicionar uma opção, basta incluir uma linha numa das tabelas
 * de menu.cpp. Navegação:
 *   Cima/Baixo  move (segure para repetir)
 *   OK/Direita  entra / edita / alterna / confirma
 *   Esquerda    volta (na raiz, fecha o menu)
 *********************************************************************/
#ifndef MENU_H
#define MENU_H

#include <Arduino.h>
#include "buttons.h"

void menu_open();
void menu_close();
bool menu_is_open();
void menu_handle(const BtnEvent &ev);
void menu_update();   // fecha sozinho após inatividade
void menu_draw();     // desenha no canvas

// Ações executadas pelo menu (implementadas em app.cpp)
void action_pomo_start_pause();
void action_pomo_reset();
void action_reset_session();
void action_reset_all_stats();
void action_about();
void action_reboot();
void action_factory_defaults();
void action_bootloader();

#endif
