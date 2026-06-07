# RP2040 HID Remapper & System Monitor ⌨️📺

Este projeto transforma um **Raspberry Pi Pico (RP2040)** em um remapeador de teclado USB inteligente e monitor de sistema. Ele atua como um "man-in-the-middle", interceptando comandos de um teclado físico e remapeando teclas específicas enquanto exibe estatísticas em tempo real em um display OLED.

## 🚀 Funcionalidades

- **Remapeamento de Teclas:** Converte a combinação `AltGr + R` para a tecla `\` (barra invertida), facilitando o uso de teclados internacionais no padrão ABNT2.
- **Contador de Teclas:** Um "odômetro" de digitação que conta quantos pressionamentos de tecla foram realizados.
- **Monitor de Volume:** Exibe uma barra de volume visual e arredondada que responde aos comandos do Rotary Encoder do teclado.
- **Anti Screen Block:** Depois de 15 segundos de inatividade, o sistema envia um movimento rápido de mouse para evitar bloqueio de tela.
- **Menu de Configurações:**
  - Screensavers
  - Configurações gerais
  - Produtividade
  - Reset
- **Dual-Core Processing:** - **Core 0:** Gerencia a pilha USB Device (comunicação com o PC).
  - **Core 1:** Gerencia o USB Host (leitura do teclado) e a atualização do Display OLED.
- **Filtro de Estabilidade:** Implementação de *debounce* de software para evitar disparos acidentais no controle de volume.
- **Screensavers :**
  - Logo Atari
  - Bouncing Balls
  - Asteroids
  - Starfield
  - Fire Works
  - Matrix
  - ASCII Aquarium

## 🛠️ Hardware Necessário

- **Microcontrolador:** Raspberry Pi Pico (ou qualquer placa RP2040 ou RP-2350).
- **Display:** TFT ST7735 160x80 (SPI).
- **Conexão USB:** Cabo OTG ou adaptador para conectar o teclado físico ao Pico via PIO-USB.
- **Joystick 4 Botões:** Joystick de 4 botões _ botão de ação para manuseio do menu

## 📁 Estrutura de Conexão (Pinout)

   #define TFT_RST   15
   #define TFT_BL     9 // Se o BLK estiver no 9 como sugerido antes



| Componente | Pino Pico (GPIO)  |
|------------|-------------------|
| TFT_MISO   | GPIO 12           |
| TFT_MOSI   | GPIO 11           |
| TFT_SCLK   | GPIO 10           |
| TFT_CS     | GPIO 13           |
| TFT_DC     | GPIO 14           |
| TFT_RST    | GPIO 15           |
| USB Host D+| GPIO 0 (ou conforme config) |
| USB Host D-| GPIO 1 (ou conforme config) |

## 💻 Bibliotecas Utilizadas

- [Adafruit_TinyUSB](https://github.com/adafruit/Adafruit_TinyUSB_Arduino) - Para as funções de USB Host e Device.
- [Pico-PIO-USB](https://github.com/sekigon-gonnoc/Pico-PIO-USB) - Para habilitar USB Host via hardware PIO.
- TFT_eSPI - Use the Setup43_ST7735.h <br> 
- Adafruit GFX Library<br>

## 📝 Como usar

1. Configure o ambiente Arduino para o Raspberry Pi Pico.
2. Certifique-se de que a velocidade da CPU está em **120MHz** ou **240MHz** (necessário para USB Host).
3. Instale as bibliotecas listadas acima.
4. Carregue o código e conecte seu teclado na porta USB Host.

<p align="center">
  <img src="https://github.com/srspinho/Key_Counter_volume/blob/main/IMG_20260313_153528.jpg" width="400">
</p>




