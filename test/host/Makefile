# Testes no PC (Linux/macOS/WSL) da lógica do teclado, das estatísticas e do Pong.
# Usa os cabeçalhos HID do Adafruit TinyUSB instalado pelo core RP2040.
#
#   make            # compila e roda
#   make TINYUSB=/caminho/para/Adafruit_TinyUSB_Arduino/src
#
TINYUSB ?= $(lastword $(wildcard $(HOME)/.arduino15/packages/rp2040/hardware/rp2040/*/libraries/Adafruit_TinyUSB_Arduino/src))
BUILD := build

all: run

$(BUILD)/tusb: $(TINYUSB)
	@test -n "$(TINYUSB)" || (echo "Defina TINYUSB=.../Adafruit_TinyUSB_Arduino/src" && exit 1)
	mkdir -p $(BUILD)/tusb/class/hid
	cp -r $(TINYUSB)/common $(BUILD)/tusb/
	cp $(TINYUSB)/tusb_option.h $(BUILD)/tusb/
	cp $(TINYUSB)/class/hid/hid.h $(BUILD)/tusb/class/hid/
	cp stub/tusb_config.h $(BUILD)/tusb/

$(BUILD)/test_bridge: test_bridge.cpp $(BUILD)/tusb $(wildcard ../../*.cpp ../../*.h stub/*.h stub/*/*.h)
	g++ -std=gnu++17 -Wall -Istub -I$(BUILD)/tusb -I../.. test_bridge.cpp -o $@

$(BUILD)/test_pong: test_pong.cpp ../../pong_game.cpp ../../pong_game.h
	mkdir -p $(BUILD)
	g++ -std=gnu++17 -O2 -Wall test_pong.cpp -o $@

run: $(BUILD)/test_bridge $(BUILD)/test_pong
	./$(BUILD)/test_bridge
	./$(BUILD)/test_pong

clean:
	rm -rf $(BUILD)

.PHONY: all run clean
