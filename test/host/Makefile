# Testes no PC (Linux/macOS/WSL) da lógica do teclado e das estatísticas.
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

run: $(BUILD)/test_bridge
	./$(BUILD)/test_bridge

clean:
	rm -rf $(BUILD)

.PHONY: all run clean
