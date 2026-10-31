/*********************************************************************
 * shared.h - Comunicação entre os dois núcleos
 *
 * Core 0: display, menu, estatísticas, configurações.
 * Core 1: USB host (teclado) e TODOS os envios para o PC.
 *
 * Os núcleos conversam somente por estas duas filas (cada uma com um
 * único produtor e um único consumidor) e por algumas variáveis
 * volatile simples. Assim nenhuma pilha USB é usada pelos dois
 * núcleos ao mesmo tempo.
 *********************************************************************/
#ifndef SHARED_H
#define SHARED_H

#include <Arduino.h>
#include <hardware/sync.h>

// Fila lock-free com 1 produtor e 1 consumidor (um em cada núcleo).
template <typename T, uint16_t N>
class SpscQueue {
public:
  bool push(const T &v) {
    uint16_t h = head;
    uint16_t next = (h + 1) % N;
    if (next == tail) return false;  // cheia
    buf[h] = v;
    __dmb();
    head = next;
    return true;
  }
  bool pop(T &v) {
    uint16_t t = tail;
    if (t == head) return false;  // vazia
    __dmb();
    v = buf[t];
    __dmb();
    tail = (t + 1) % N;
    return true;
  }
  bool empty() const { return head == tail; }
  uint16_t freeSpace() const { return (uint16_t)((tail + N - head - 1) % N); }

private:
  T buf[N];
  volatile uint16_t head = 0;
  volatile uint16_t tail = 0;
};

// ---------- Core 1 -> Core 0: eventos do teclado ----------
enum EventType : uint8_t {
  EV_KEY,           // tecla pressionada (a = keycode físico)
  EV_ACTIVITY,      // qualquer mudança no teclado (ex.: só modificador)
  EV_VOLUME,        // b = +1 / -1 (passos de volume)
  EV_MUTE,          // botão de mudo
  EV_KBD_MOUNT,     // teclado conectado
  EV_KBD_UMOUNT,    // teclado desconectado
  EV_CLOCK_ERROR,   // CPU fora de múltiplo de 12 MHz (USB host não funciona)
};

struct CoreEvent {
  EventType type;
  uint8_t a;
  int8_t b;
};

// ---------- Core 0 -> Core 1: relatórios para enviar ao PC ----------
enum OutType : uint8_t {
  OUT_KEYBOARD = 1,  // mesmo número do Report ID
  OUT_CONSUMER = 2,
  OUT_MOUSE = 3,
};

struct OutReport {
  OutType type;
  uint8_t len;
  uint8_t gap_ms;   // espera mínima antes do PRÓXIMO envio (0 = nenhuma)
  uint8_t data[8];
};

extern SpscQueue<CoreEvent, 128> g_events;   // core1 -> core0
extern SpscQueue<OutReport, 256> g_outbox;   // core0 -> core1

// Estado compartilhado simples
extern volatile uint8_t g_leds;              // LEDs (Caps/Num/Scroll) vindos do PC
extern volatile bool g_keyboard_connected;
extern volatile uint32_t g_core1_heartbeat;  // incrementado pelo core 1 (watchdog)

// Funções de conveniência para o core 0
void post_consumer_click(uint16_t usage);    // aperta e solta uma tecla de mídia
void post_mouse_nudge();                     // move o mouse 1px e volta

#endif
