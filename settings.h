/*********************************************************************
 * settings.h - Preferências do usuário (ajustadas pelo menu)
 *
 * Gravadas na flash com número de versão + CRC: se a estrutura mudar
 * ou os dados estiverem corrompidos, os valores padrão são usados.
 *********************************************************************/
#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

struct Settings {
  // Screensaver
  int16_t ss_type;          // índice em SCREENSAVERS[] ou ss_count() = "Todos"
  int16_t ss_start_min;     // minutos ocioso até o screensaver (0 = nunca)
  int16_t ss_switch_s;      // troca automática no modo "Todos"
  int16_t ss_speed;         // 1..20
  int16_t ss_asteroids;     // 1..MAX_ASTEROIDS
  int16_t ss_color;         // intensidade das cores 10..100 %
  // Tela
  int16_t brightness;       // 10..100 %
  int16_t dim_brightness;   // 0..100 % (brilho quando ocioso)
  int16_t dim_after_s;      // segundos até escurecer (0 = nunca)
  bool flip;                // gira a tela 180°
  // Produtividade
  bool antilock;            // move o mouse quando ocioso
  int16_t antilock_s;       // intervalo do anti-bloqueio (s)
  int16_t pomo_focus_min;
  int16_t pomo_break_min;
  int16_t break_remind_min; // lembrete de pausa (0 = desligado)
  // Teclado
  bool remap;               // remapeamentos e macros
  bool layer;               // camada Fn
  bool vol_sync;            // sincroniza o volume do PC no boot
  int16_t vol_sync_level;   // volume aplicado no boot (%)
};

extern Settings settings;

void settings_defaults(Settings &s);
void settings_validate(Settings &s);
void settings_load();
void settings_mark_dirty();
bool settings_is_dirty();
void settings_save_if_dirty();  // grava somente se algo mudou

// ---------- Armazenamento genérico (EEPROM emulada na flash) ----------
// Cada bloco tem cabeçalho com assinatura, versão, tamanho e CRC16.
bool storage_read(uint16_t offset, uint8_t version, void *data, uint16_t len);
void storage_write(uint16_t offset, uint8_t version, const void *data, uint16_t len);
void storage_commit();

#define STORAGE_SIZE        2048
#define STORAGE_OFS_SETTINGS 0
#define STORAGE_OFS_STATS    128

#endif
