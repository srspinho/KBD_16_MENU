"""
Definição única do circuito da placa KBD16 (esquemático + PCB são gerados daqui).

Coordenadas da PCB em mm, origem no canto superior esquerdo da placa, y para baixo.
"""

BOARD_W = 86.0
BOARD_H = 52.0

# ---------------------------------------------------------------------------
# Módulo Waveshare RP2350-Zero (também serve o RP2040-Zero: mesma pinagem)
# Pads numerados no sentido anti-horário a partir do 5V (canto superior
# esquerdo, com o USB-C para cima).
# ---------------------------------------------------------------------------
ZERO_PINS = [
    # (número, nome, tipo elétrico, x, y)  -- posição relativa ao centro do módulo
    ("1", "5V", "power_out", -7.62, -10.16),
    ("2", "GND", "power_in", -7.62, -7.62),
    ("3", "3V3", "power_out", -7.62, -5.08),
    ("4", "GP29", "bidirectional", -7.62, -2.54),
    ("5", "GP28", "bidirectional", -7.62, 0.0),
    ("6", "GP27", "bidirectional", -7.62, 2.54),
    ("7", "GP26", "bidirectional", -7.62, 5.08),
    ("8", "GP15", "bidirectional", -7.62, 7.62),
    ("9", "GP14", "bidirectional", -7.62, 10.16),
    ("10", "GP13", "bidirectional", -5.08, 10.16),
    ("11", "GP12", "bidirectional", -2.54, 10.16),
    ("12", "GP11", "bidirectional", 0.0, 10.16),
    ("13", "GP10", "bidirectional", 2.54, 10.16),
    ("14", "GP9", "bidirectional", 5.08, 10.16),
    ("15", "GP8", "bidirectional", 7.62, 10.16),
    ("16", "GP7", "bidirectional", 7.62, 7.62),
    ("17", "GP6", "bidirectional", 7.62, 5.08),
    ("18", "GP5", "bidirectional", 7.62, 2.54),
    ("19", "GP4", "bidirectional", 7.62, 0.0),
    ("20", "GP3", "bidirectional", 7.62, -2.54),
    ("21", "GP2", "bidirectional", 7.62, -5.08),
    ("22", "GP1", "bidirectional", 7.62, -7.62),
    ("23", "GP0", "bidirectional", 7.62, -10.16),
]
ZERO_W, ZERO_H = 18.0, 23.5

# LCD 0,96" 80x160 ST7735 (módulo com 8 pinos). Ordem impressa no módulo.
LCD_PINS = ["GND", "VCC", "SCL", "SDA", "RES", "DC", "CS", "BLK"]
LCD_W, LCD_H = 30.0, 24.0          # contorno aproximado do módulo (CONFERIR)
LCD_HEADER_FROM_BOTTOM = 1.6       # distância da linha de pinos até a borda de baixo

# Joystick 5 direções: comum + 5 contatos
JOY_PINS = ["COM", "UP", "DOWN", "LEFT", "RIGHT", "OK"]

# ---------------------------------------------------------------------------
# Componentes
#   ref, lib_id (símbolo), valor, footprint, (x, y, rot) na PCB, lado,
#   {pino: rede}, posição no esquemático (x, y), campos extras
# ---------------------------------------------------------------------------
R0805 = "Resistor_SMD:R_0805_2012Metric_Pad1.20x1.40mm_HandSolder"
C0805 = "Capacitor_SMD:C_0805_2012Metric_Pad1.18x1.45mm_HandSolder"

COMPONENTS = []


def comp(ref, lib_id, value, footprint, pcb, pins, sch, desc="", mpn="", dnp=False):
    COMPONENTS.append(dict(ref=ref, lib_id=lib_id, value=value, footprint=footprint,
                           pcb=pcb, pins=pins, sch=sch, desc=desc, mpn=mpn, dnp=dnp))


# --- Microcontrolador ---------------------------------------------------------
zero_nets = {
    "1": "+5V", "2": "GND", "3": "+3V3",
    "4": None, "5": None,                      # GP29, GP28 livres
    "6": "RTC_SCL", "7": "RTC_SDA",            # GP27, GP26
    "8": "LCD_RES", "9": "LCD_DC",             # GP15, GP14
    "10": "LCD_CS", "11": None,                # GP13, GP12 (MISO não usado)
    "12": "LCD_SDA", "13": "LCD_SCL",          # GP11, GP10
    "14": None,                                # GP9 livre
    "15": "BUZZER", "16": "LCD_BL",            # GP8, GP7
    "17": "JOY_LEFT", "18": "JOY_RIGHT", "19": "JOY_OK",
    "20": "JOY_DOWN", "21": "JOY_UP",          # GP6..GP2
    "22": "HOST_DM", "23": "HOST_DP",          # GP1, GP0
}
comp("U1", "kbd16:RP2350-Zero", "RP2350-Zero", "kbd16:RP2350-Zero",
     (17.0, 11.75, 0), zero_nets, (60, 90),
     desc="Waveshare RP2350-Zero (ou RP2040-Zero)", mpn="Waveshare RP2350-Zero")

# --- USB host (teclado) ---------------------------------------------------------
comp("J1", "Connector:USB_A", "USB-A Teclado", "Connector_USB:USB_A_Molex_67643_Horizontal",
     (44.0, 9.27, 180), {"1": "VBUS_KB", "2": "KB_DM", "3": "KB_DP", "4": "GND", "5": "GND"}, (170, 50),
     desc="Receptáculo USB-A horizontal THT", mpn="Molex 67643-0910")
comp("F1", "Device:Polyfuse", "500mA", "Fuse:Fuse_1206_3216Metric",
     (44.0, 15.5, 0), {"1": "+5V", "2": "VBUS_KB"}, (150, 40),
     desc="Fusível rearmável 500 mA hold", mpn="1206L050")
comp("C1", "Device:C", "10uF", C0805,
     (44.0, 19.0, 0), {"1": "VBUS_KB", "2": "GND"}, (190, 40),
     desc="Capacitor cerâmico 10 µF 10 V X5R")
comp("U2", "Power_Protection:USBLC6-2SC6", "USBLC6-2SC6", "Package_TO_SOT_SMD:SOT-23-6",
     (37.5, 16.5, 0), {"1": "KB_DP", "6": "KB_DP", "3": "KB_DM", "4": "KB_DM", "2": "GND", "5": "VBUS_KB"},
     (170, 80), desc="Proteção ESD USB", mpn="USBLC6-2SC6")
comp("R1", "Device:R", "22", R0805,
     (31.0, 14.0, 0), {"1": "HOST_DP", "2": "KB_DP"}, (130, 70),
     desc="Resistor série D+")
comp("R2", "Device:R", "22", R0805,
     (31.0, 18.0, 0), {"1": "HOST_DM", "2": "KB_DM"}, (142, 70),
     desc="Resistor série D-")

# --- LCD -----------------------------------------------------------------------
lcd_nets = {"1": "GND", "2": "+3V3", "3": "LCD_SCL", "4": "LCD_SDA",
            "5": "LCD_RES", "6": "LCD_DC", "7": "LCD_CS", "8": "LCD_BLK"}
comp("J2", "Connector_Generic:Conn_01x08", "LCD 0.96 80x160", "kbd16:LCD_0.96_80x160_Socket",
     (24.0, 46.5, 0), lcd_nets, (60, 150),
     desc="Barra fêmea 1x8 2,54 mm para o módulo LCD ST7735")
comp("R3", "Device:R", "100", R0805,
     (36.0, 40.0, 90), {"1": "LCD_BL", "2": "LCD_BLK"}, (100, 165),
     desc="Resistor série do backlight (GPIO7 -> BLK)")
comp("C2", "Device:C", "100nF", C0805,
     (12.0, 29.0, 0), {"1": "+3V3", "2": "GND"}, (30, 150),
     desc="Desacoplamento 3V3 do LCD")

# --- Joystick ------------------------------------------------------------------
joy_nets = {"1": "GND", "2": "JOY_UP", "3": "JOY_DOWN", "4": "JOY_LEFT", "5": "JOY_RIGHT", "6": "JOY_OK"}
comp("J3", "Connector_Generic:Conn_01x06", "Joystick 5 vias", "Connector_PinSocket_2.54mm:PinSocket_1x06_P2.54mm_Vertical",
     (58.0, 45.5, 90), joy_nets, (200, 150),
     desc="Barra fêmea 1x6 para o joystick (COM, CIMA, BAIXO, ESQ, DIR, OK)")
for i, (net, x) in enumerate([("JOY_UP", 51.0), ("JOY_DOWN", 54.5), ("JOY_LEFT", 58.0), ("JOY_RIGHT", 61.5), ("JOY_OK", 65.0)]):
    comp(f"R{4 + i}", "Device:R", "10k", R0805, (x, 40.5, 90), {"1": "+3V3", "2": net}, (150 + i * 12, 130),
         desc="Pull-up do joystick")

# --- RTC DS3231 (opcional) -------------------------------------------------------
comp("U3", "Timer_RTC:DS3231M", "DS3231", "Package_SO:SOIC-16W_7.5x10.3mm_P1.27mm",
     (62.0, 10.0, 0),
     {"1": None, "2": "+3V3", "3": None, "4": None, "5": "GND", "6": "GND", "7": "GND", "8": "GND",
      "9": "GND", "10": "GND", "11": "GND", "12": "GND", "13": "GND", "14": "VBAT",
      "15": "RTC_SDA", "16": "RTC_SCL"}, (250, 60),
     desc="RTC DS3231SN ou DS3231M (opcional)", mpn="DS3231SN#", dnp=False)
comp("BT1", "Device:Battery_Cell", "CR1220", "Battery:BatteryHolder_Keystone_3000_1x12mm",
     (78.5, 19.0, 90), {"1": "VBAT", "2": "GND"}, (280, 60),
     desc="Suporte de bateria CR1220", mpn="Keystone 3000")
comp("C3", "Device:C", "100nF", C0805, (62.0, 18.6, 0), {"1": "+3V3", "2": "GND"}, (230, 40),
     desc="Desacoplamento do RTC")
comp("R9", "Device:R", "4.7k", R0805, (52.5, 7.5, 90), {"1": "+3V3", "2": "RTC_SDA"}, (225, 70),
     desc="Pull-up I2C SDA")
comp("R10", "Device:R", "4.7k", R0805, (52.5, 12.5, 90), {"1": "+3V3", "2": "RTC_SCL"}, (215, 70),
     desc="Pull-up I2C SCL")

# --- Buzzer (opcional) -------------------------------------------------------------
comp("BZ1", "Device:Buzzer", "Buzzer", "Buzzer_Beeper:Buzzer_12x9.5RM7.6",
     (54.0, 30.0, 0), {"1": "+5V", "2": "BZ_DRV"}, (270, 140),
     desc="Buzzer magnético passivo 12 mm, 5 V", mpn="TMB12A05 (passivo)")
comp("Q1", "Transistor_BJT:MMBT3904", "MMBT3904", "Package_TO_SOT_SMD:SOT-23",
     (69.0, 30.0, 0), {"1": "BZ_B", "2": "GND", "3": "BZ_DRV"}, (260, 165),
     desc="Driver do buzzer", mpn="MMBT3904")
comp("R11", "Device:R", "1k", R0805, (69.0, 26.0, 0), {"1": "BUZZER", "2": "BZ_B"}, (240, 165),
     desc="Resistor de base")
comp("R12", "Device:R", "10k", R0805, (69.0, 34.0, 0), {"1": "BZ_B", "2": "GND"}, (252, 165),
     desc="Pull-down de base (buzzer mudo no boot)")
comp("D1", "Device:D", "1N4148W", "Diode_SMD:D_SOD-123", (66.5, 22.0, 0), {"1": "+5V", "2": "BZ_DRV"}, (290, 140),
     desc="Diodo de roda livre do buzzer", mpn="1N4148W")

# --- Furos de fixação M3 -------------------------------------------------------------
for i, (x, y) in enumerate([(4.0, 4.0), (BOARD_W - 4.0, 4.0), (4.0, BOARD_H - 4.0), (BOARD_W - 4.0, BOARD_H - 4.0)]):
    comp(f"H{i + 1}", "Mechanical:MountingHole", "M3", "MountingHole:MountingHole_3.2mm_M3",
         (x, y, 0), {}, (30 + i * 15, 190), desc="Furo de fixação M3")

# Redes que recebem PWR_FLAG no esquemático (alimentadas por componente passivo)
PWR_FLAG_NETS = ["GND", "VBUS_KB"]
POWER_NETS = {"+5V": "power:+5V", "+3V3": "power:+3V3", "GND": "power:GND"}
WIDE_NETS = ["+5V", "VBUS_KB", "GND", "+3V3"]
