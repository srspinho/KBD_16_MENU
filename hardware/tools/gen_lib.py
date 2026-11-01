"""Gera a biblioteca do projeto: lib/kbd16.kicad_sym e lib/kbd16.pretty/*.kicad_mod"""
import os
import uuid
from design import ZERO_PINS, ZERO_W, ZERO_H, LCD_PINS, LCD_W, LCD_H, LCD_HEADER_FROM_BOTTOM
from sexp import q, fmt_num as f

ROOT = os.path.join(os.path.dirname(__file__), '..', 'lib')


def u():
    return str(uuid.uuid4())


# ----------------------------------------------------------------- símbolo
def zero_symbol():
    # Caixa: lado esquerdo = pinos 1..9 (de cima p/ baixo), direito = 23..15,
    # baixo = 10..14 (esquerda p/ direita). Comprimento de pino 2,54.
    # 5V e 3V3 em cima, GND embaixo, GPIOs nas laterais e na base.
    pins = []
    by_name = {p[1]: p for p in ZERO_PINS}
    left = [by_name[n] for n in ('GP29', 'GP28', 'GP27', 'GP26', 'GP15', 'GP14')]
    right = [by_name['GP%d' % i] for i in range(0, 9)]
    bottom = [by_name[n] for n in ('GP13', 'GP12', 'GP11', 'GP10', 'GP9')]
    top_y = 10.16
    for i, (num, name, etype, _, _) in enumerate(left):
        pins.append((num, name, etype, -15.24, top_y - i * 2.54, 0))
    for i, (num, name, etype, _, _) in enumerate(right):
        pins.append((num, name, etype, 15.24, top_y - i * 2.54, 180))
    for i, (num, name, etype, _, _) in enumerate(bottom):
        pins.append((num, name, etype, -2.54 + i * 2.54, -15.24, 90))
    num, name, etype, _, _ = by_name['5V']
    pins.append((num, name, etype, -2.54, 15.24, 270))
    num, name, etype, _, _ = by_name['3V3']
    pins.append((num, name, etype, 2.54, 15.24, 270))
    num, name, etype, _, _ = by_name['GND']
    pins.append((num, name, etype, -7.62, -15.24, 90))
    body = (f'      (rectangle (start -12.7 12.7) (end 12.7 -12.7)\n'
            f'        (stroke (width 0.254) (type default)) (fill (type background)))\n')
    pin_txt = ''
    for num, name, etype, x, y, a in pins:
        pin_txt += (f'      (pin {etype} line (at {f(x)} {f(y)} {a}) (length 2.54)\n'
                    f'        (name {q(name)} (effects (font (size 1.27 1.27))))\n'
                    f'        (number {q(num)} (effects (font (size 1.27 1.27)))))\n')
    return f'''  (symbol "RP2350-Zero" (pin_names (offset 1.016)) (in_bom yes) (on_board yes)
    (property "Reference" "U" (at -12.7 13.97 0) (effects (font (size 1.27 1.27)) (justify left)))
    (property "Value" "RP2350-Zero" (at 12.7 13.97 0) (effects (font (size 1.27 1.27)) (justify right)))
    (property "Footprint" "kbd16:RP2350-Zero" (at 0 -20.32 0) (effects (font (size 1.27 1.27)) hide))
    (property "Datasheet" "https://www.waveshare.com/wiki/RP2350-Zero" (at 0 -22.86 0) (effects (font (size 1.27 1.27)) hide))
    (property "Description" "Waveshare RP2350-Zero / RP2040-Zero (mesma pinagem)" (at 0 0 0) (effects (font (size 1.27 1.27)) hide))
    (symbol "RP2350-Zero_0_1"
{body}    )
    (symbol "RP2350-Zero_1_1"
{pin_txt}    )
  )
'''


def write_symbols():
    txt = ('(kicad_symbol_lib (version 20220914) (generator kbd16_gen)\n'
           + zero_symbol() + ')\n')
    with open(os.path.join(ROOT, 'kbd16.kicad_sym'), 'w') as fh:
        fh.write(txt)


# --------------------------------------------------------------- footprints
def line(x1, y1, x2, y2, layer, w):
    return (f'  (fp_line (start {f(x1)} {f(y1)}) (end {f(x2)} {f(y2)}) '
            f'(stroke (width {f(w)}) (type solid)) (layer "{layer}") (tstamp {u()}))\n')


def rect(x1, y1, x2, y2, layer, w):
    return (line(x1, y1, x2, y1, layer, w) + line(x2, y1, x2, y2, layer, w) +
            line(x2, y2, x1, y2, layer, w) + line(x1, y2, x1, y1, layer, w))


def text(kind, value, x, y, layer, size=1.0, hide=False, extra=''):
    h = ' hide' if hide else ''
    return (f'  (fp_text {kind} {q(value)} (at {f(x)} {f(y)}) (layer "{layer}"){h}\n'
            f'    (effects (font (size {f(size)} {f(size)}) (thickness {f(size * 0.15)})){extra}) (tstamp {u()}))\n')


def tht_pad(num, x, y, shape='circle', size=1.7, drill=1.0):
    return (f'  (pad {q(num)} thru_hole {shape} (at {f(x)} {f(y)}) (size {f(size)} {f(size)}) '
            f'(drill {f(drill)}) (layers "*.Cu" "*.Mask") (tstamp {u()}))\n')


def zero_footprint():
    hw, hh = ZERO_W / 2, ZERO_H / 2
    s = (f'(footprint "RP2350-Zero" (version 20221018) (generator kbd16_gen) (layer "F.Cu")\n'
         f'  (descr "Waveshare RP2350-Zero / RP2040-Zero, 23 pinos de borda (barras 2,54 mm). USB-C para cima.")\n'
         f'  (tags "RP2350 RP2040 Zero Waveshare")\n'
         f'  (attr through_hole)\n')
    s += text('reference', 'REF**', -hw - 1.2, 0, 'F.SilkS', 1.0, extra='') if False else text('reference', 'REF**', 0, 0, 'F.Fab')
    s += text('value', 'RP2350-Zero', 0, hh + 1.5, 'F.Fab')
    s += text('user', 'USB-C', 0, -hh + 2.0, 'F.SilkS', 1.0)
    s += rect(-hw, -hh, hw, hh, 'F.Fab', 0.1)
    # contorno do conector USB-C do módulo
    s += rect(-4.5, -hh - 0.5, 4.5, -hh + 6.8, 'F.Fab', 0.1)
    s += line(hw + 0.12, -hh + 1.0, hw + 0.12, hh + 0.12, 'F.SilkS', 0.12)
    s += line(hw + 0.12, hh + 0.12, -hw - 0.12, hh + 0.12, 'F.SilkS', 0.12)
    s += line(-hw - 0.12, hh + 0.12, -hw - 0.12, -hh + 1.0, 'F.SilkS', 0.12)
    s += rect(-hw - 0.25, -hh - 0.25, hw + 0.25, hh + 0.25, 'F.CrtYd', 0.05)
    names = {}
    for num, name, _, x, y in ZERO_PINS:
        s += tht_pad(num, x, y, 'rect' if num == '1' else 'circle')
        names[num] = (name, x, y)
    # nomes dos pinos na serigrafia (dentro do contorno)
    for num, (name, x, y) in names.items():
        nm = name.replace('GP', '')
        if abs(x) > 7:
            # laterais: rótulo do lado de dentro do pad
            tx = x - 2.1 if x > 0 else x + 2.1
            if y > 9:          # cantos de baixo: um pouco acima, para não colidir com a fileira de baixo
                s += text('user', nm, tx, y - 1.4, 'F.SilkS', 0.8)
            else:
                s += text('user', nm, tx, y, 'F.SilkS', 0.8)
        else:
            # fileira de baixo: rótulo fora do contorno do módulo (fica visível com o módulo montado)
            s += text('user', nm, x, hh + 1.1, 'F.SilkS', 0.8)
    return s + ')\n'


def lcd_footprint():
    n = len(LCD_PINS)
    span = (n - 1) * 2.54
    x0 = -span / 2
    bottom = LCD_HEADER_FROM_BOTTOM
    s = (f'(footprint "LCD_0.96_80x160_Socket" (version 20221018) (generator kbd16_gen) (layer "F.Cu")\n'
         f'  (descr "Barra fêmea 1x8 2,54 mm para módulo LCD IPS 0,96 pol. 80x160 ST7735 (GND VCC SCL SDA RES DC CS BLK). '
         f'O contorno do módulo em F.Fab é aproximado: confira com o seu módulo.")\n'
         f'  (tags "LCD ST7735 0.96 80x160 SPI")\n'
         f'  (attr through_hole)\n')
    s += text('reference', 'REF**', x0 - 2.5, 0, 'F.SilkS')
    s += text('value', 'LCD 0.96 80x160', 0, -LCD_H / 2, 'F.Fab')
    s += text('user', 'LCD 0,96" (area do modulo)', 0, -LCD_H + bottom + 3, 'F.Fab', 1.0)
    # contorno do módulo (acima da barra de pinos)
    s += rect(-LCD_W / 2, -LCD_H + bottom, LCD_W / 2, bottom, 'F.Fab', 0.1)
    s += rect(-LCD_W / 2, -LCD_H + bottom, LCD_W / 2, bottom, 'Dwgs.User', 0.15)
    # barra de pinos
    s += rect(x0 - 1.27, -1.27, x0 + span + 1.27, 1.27, 'F.SilkS', 0.12)
    s += rect(x0 - 1.52, -1.52, x0 + span + 1.52, 1.52, 'F.CrtYd', 0.05)
    for i, name in enumerate(LCD_PINS):
        x = x0 + i * 2.54
        s += tht_pad(str(i + 1), x, 0, 'rect' if i == 0 else 'oval', 1.7)
        s += text('user', name, x, 2.7, 'F.SilkS', 0.8)
    return s + ')\n'


def write_footprints():
    d = os.path.join(ROOT, 'kbd16.pretty')
    os.makedirs(d, exist_ok=True)
    for name, txt in [('RP2350-Zero', zero_footprint()), ('LCD_0.96_80x160_Socket', lcd_footprint())]:
        with open(os.path.join(d, name + '.kicad_mod'), 'w') as fh:
            fh.write(txt)


if __name__ == '__main__':
    os.makedirs(ROOT, exist_ok=True)
    write_symbols()
    write_footprints()
    print('biblioteca gerada em', os.path.abspath(ROOT))
