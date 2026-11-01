"""Gera kbd16.kicad_sch a partir de design.py (rótulos de rede em cada pino)."""
import copy
import os
import uuid
from design import COMPONENTS, PWR_FLAG_NETS, POWER_NETS
from sexp import parse, dump, find, find1, Sym, q, fmt_num as f

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'kbd16.kicad_sch')
SYMDIR = '/usr/share/kicad/symbols'
PROJECT = 'kbd16'
ROOT_UUID = '6d1f1e0a-3b2f-4c55-9a51-0b0c1d16e001'

from ids import sid  # UUIDs estáveis (mesma entrada -> mesmo arquivo)


_libs = {}


def lib_symbol(lib_id):
    lib, name = lib_id.split(':')
    if lib == 'kbd16':
        path = os.path.join(HERE, '..', 'lib', 'kbd16.kicad_sym')
    else:
        path = os.path.join(SYMDIR, lib + '.kicad_sym')
    if path not in _libs:
        _libs[path] = parse(open(path).read())
    L = _libs[path]
    syms = {s[1]: s for s in find(L, 'symbol')}
    node = copy.deepcopy(syms[name])
    ext = find1(node, 'extends')
    if ext:
        parent = copy.deepcopy(syms[ext[1]])
        child_props = {p[1]: p for p in find(node, 'property')}
        out = [Sym('symbol'), lib_id]
        for item in parent[2:]:
            if isinstance(item, list) and item[0] == 'property' and item[1] in child_props:
                out.append(child_props.pop(item[1]))
            elif isinstance(item, list) and item[0] == 'symbol':
                item[1] = item[1].replace(ext[1], name, 1)
                out.append(item)
            else:
                out.append(item)
        # propriedades só do filho
        idx = max(i for i, it in enumerate(out) if isinstance(it, list) and it[0] == 'property')
        for p in child_props.values():
            idx += 1
            out.insert(idx, p)
        node = out
    else:
        node[1] = lib_id
    return node


def pins_of(symnode):
    out = []

    def walk(n):
        for x in n:
            if isinstance(x, list):
                if x and x[0] == 'pin':
                    at = find1(x, 'at')
                    out.append((find1(x, 'number')[1], find1(x, 'name')[1], x[1],
                                float(at[1]), float(at[2]), int(float(at[3]))))
                else:
                    walk(x)
    walk(symnode)
    return out


# ------------------------------------------------------------------ montagem
items = []          # textos do esquemático
lib_used = {}
pwr_count = [0]
flg_count = [0]


def use(lib_id):
    if lib_id not in lib_used:
        lib_used[lib_id] = lib_symbol(lib_id)
    return lib_used[lib_id]


def prop(name, value, x, y, hide=False, angle=0, justify=None):
    h = ' hide' if hide else ''
    j = f' (justify {justify})' if justify else ''
    return (f'    (property {q(name)} {q(value)} (at {f(x)} {f(y)} {angle})\n'
            f'      (effects (font (size 1.27 1.27)){j}{h}))\n')


def place_symbol(lib_id, ref, value, x, y, rot=0, footprint='', fields=None, uid=None,
                 in_bom=True, on_board=True, dnp=False, ref_pos=None, val_pos=None, hide_ref=False):
    sym = use(lib_id)
    uid = uid or sid('sym', ref)
    pins = pins_of(sym)
    libprops = {p[1]: find1(p, 'at') for p in find(sym, 'property')}
    def lp(name, dflt):
        a = libprops.get(name)
        return (x + float(a[1]), y - float(a[2])) if a else dflt
    rp = ref_pos or lp('Reference', (x, y - 2.54))
    vp = val_pos or lp('Value', (x, y + 2.54))
    t = (f'  (symbol (lib_id {q(lib_id)}) (at {f(x)} {f(y)} {rot}) (unit 1)\n'
         f'    (in_bom {"yes" if in_bom else "no"}) (on_board {"yes" if on_board else "no"}) (dnp {"yes" if dnp else "no"})\n'
         f'    (uuid {uid})\n')
    t += prop('Reference', ref, rp[0], rp[1], hide=hide_ref)
    t += prop('Value', value, vp[0], vp[1])
    t += prop('Footprint', footprint, x, y, hide=True)
    t += prop('Datasheet', '', x, y, hide=True)
    for k, v in (fields or {}).items():
        t += prop(k, v, x, y, hide=True)
    seen = set()
    for num, *_ in pins:
        if num in seen:
            continue
        seen.add(num)
        t += f'    (pin {q(num)} (uuid {sid("pin", ref, num)}))\n'
    t += (f'    (instances\n      (project {q(PROJECT)}\n'
          f'        (path "/{ROOT_UUID}" (reference {q(ref)}) (unit 1))))\n  )\n')
    items.append(t)
    return pins


def pin_point(x, y, rot, px, py):
    # símbolo sem rotação: y da biblioteca é para cima, y do esquemático para baixo
    assert rot == 0
    return round(x + px, 4), round(y - py, 4)


def outward(angle):
    return (angle + 180) % 360


def label(name, x, y, direction):
    just = {0: 'left bottom', 90: 'left bottom', 180: 'right bottom', 270: 'right bottom'}[direction]
    items.append(f'  (label {q(name)} (at {f(x)} {f(y)} {direction}) (fields_autoplaced)\n'
                 f'    (effects (font (size 1.27 1.27)) (justify {just}))\n'
                 f'    (uuid {sid("label", name, x, y)}))\n')


def power_symbol(net, x, y, direction):
    lib_id = POWER_NETS[net]
    body = 270 if net == 'GND' else 90      # direção do corpo do símbolo sem rotação
    rot = (direction - body) % 360
    pwr_count[0] += 1
    ref = f'#PWR{pwr_count[0]:02d}'
    use(lib_id)
    # posição do texto do valor, afastado na direção do corpo
    import math
    dx = math.cos(math.radians(direction)) * 3.8
    dy = -math.sin(math.radians(direction)) * 3.8
    items.append(
        f'  (symbol (lib_id {q(lib_id)}) (at {f(x)} {f(y)} {rot}) (unit 1)\n'
        f'    (in_bom yes) (on_board yes) (dnp no)\n'
        f'    (uuid {sid("pwr", net, x, y)})\n'
        + prop('Reference', ref, x, y, hide=True)
        + prop('Value', net, x + dx, y + dy)
        + prop('Footprint', '', x, y, hide=True)
        + prop('Datasheet', '', x, y, hide=True)
        + f'    (pin "1" (uuid {sid("pwrpin", net, x, y)}))\n'
        + f'    (instances\n      (project {q(PROJECT)}\n'
          f'        (path "/{ROOT_UUID}" (reference {q(ref)}) (unit 1))))\n  )\n')


def pwr_flag(x, y):
    flg_count[0] += 1
    ref = f'#FLG{flg_count[0]:02d}'
    use('power:PWR_FLAG')
    items.append(
        f'  (symbol (lib_id "power:PWR_FLAG") (at {f(x)} {f(y)} 0) (unit 1)\n'
        f'    (in_bom yes) (on_board yes) (dnp no)\n'
        f'    (uuid {sid("flag", x, y)})\n'
        + prop('Reference', ref, x, y, hide=True)
        + prop('Value', 'PWR_FLAG', x, y - 3.8)
        + prop('Footprint', '', x, y, hide=True)
        + prop('Datasheet', '', x, y, hide=True)
        + f'    (pin "1" (uuid {sid("flagpin", x, y)}))\n'
        + f'    (instances\n      (project {q(PROJECT)}\n'
          f'        (path "/{ROOT_UUID}" (reference {q(ref)}) (unit 1))))\n  )\n')


def wire(x1, y1, x2, y2):
    items.append(f'  (wire (pts (xy {f(x1)} {f(y1)}) (xy {f(x2)} {f(y2)}))\n'
                 f'    (stroke (width 0) (type default))\n'
                 f'    (uuid {sid("wire", x1, y1, x2, y2)}))\n')


def no_connect(x, y):
    items.append(f'  (no_connect (at {f(x)} {f(y)}) (uuid {sid("nc", x, y)}))\n')


def text(t, x, y, size=2.0):
    items.append(f'  (text {q(t)} (at {f(x)} {f(y)} 0)\n'
                 f'    (effects (font (size {f(size)} {f(size)}) bold) (justify left bottom))\n'
                 f'    (uuid {sid("text", t)}))\n')


# ------------------------------------------------------------------ circuito
for c in COMPONENTS:
    x, y = c['sch']
    lib_id = c['lib_id']
    fields = {}
    if c['desc']:
        fields['Description'] = c['desc']
    if c['mpn']:
        fields['MPN'] = c['mpn']
    is_hole = lib_id.startswith('Mechanical:')
    pins = place_symbol(lib_id, c['ref'], c['value'], x, y, 0, c['footprint'], fields,
                        uid=sid('sym', c['ref']), in_bom=not is_hole, dnp=c['dnp'],
                        ref_pos=(x + 2.0, y - 1.0) if lib_id in ('Device:R', 'Device:C', 'Device:Polyfuse') else None,
                        val_pos=(x + 2.0, y + 1.5) if lib_id in ('Device:R', 'Device:C', 'Device:Polyfuse') else None)
    done = set()
    power_pins = []
    for num, name, etype, px, py, a in pins:
        pt = pin_point(x, y, 0, px, py)
        if pt in done:
            continue
        done.add(pt)
        net = c['pins'].get(num)
        d = outward(a)
        if net is None:
            no_connect(*pt)
        elif net in POWER_NETS:
            power_pins.append((net, d, pt))
        else:
            label(net, pt[0], pt[1], d)

    # Alimentação: pinos vizinhos da mesma rede na mesma borda viram um fio
    # só; em pinos laterais os símbolos ficam escalonados para não sobrepor.
    groups = {}
    for net, d, pt in power_pins:
        key = (net, d, pt[1] if d in (90, 270) else pt[0])
        groups.setdefault(key, []).append(pt)
    side_index = {}
    for (net, d, _), pts in sorted(groups.items(), key=lambda kv: (kv[1][0][1], kv[1][0][0])):
        pts.sort()
        for a_, b_ in zip(pts, pts[1:]):
            wire(a_[0], a_[1], b_[0], b_[1])
        px_, py_ = pts[0]
        if d in (0, 180):
            k = side_index.get(d, 0)
            side_index[d] = k + 1
            stub = 2.54 + 5.08 * k
            ex = px_ + (stub if d == 0 else -stub)
            wire(px_, py_, ex, py_)
            power_symbol(net, ex, py_, d)
        else:
            power_symbol(net, px_, py_, d)

# PWR_FLAG nas redes alimentadas só por componentes passivos
fx = 30
for net in PWR_FLAG_NETS:
    pwr_flag(fx, 230)
    if net in POWER_NETS:
        power_symbol(net, fx, 230, 270)
    else:
        label(net, fx, 230, 270)
    fx += 20

text('KBD16 - Placa-base para RP2350-Zero (HID Remapper & System Monitor)', 20, 20, 3.0)
text('USB host (teclado)', 125, 30)
text('LCD 0,96" 80x160 ST7735', 20, 135)
text('Joystick 5 vias (pull-ups externos: errata RP2350-E9)', 140, 118)
text('RTC DS3231 (opcional)', 205, 30)
text('Buzzer (opcional)', 225, 125)
text('Furos M3', 20, 180)

lib_txt = '  (lib_symbols\n'
for lid, node in lib_used.items():
    lib_txt += '    ' + dump(node, 2) + '\n'
lib_txt += '  )\n'

sch = (f'(kicad_sch (version 20230121) (generator kbd16_gen)\n'
       f'  (uuid {ROOT_UUID})\n'
       f'  (paper "A3")\n'
       f'  (title_block\n    (title "KBD16 - HID Remapper & System Monitor")\n'
       f'    (date "2026-11-01")\n    (rev "1.0")\n'
       f'    (comment 1 "Placa-base para Waveshare RP2350-Zero (ou RP2040-Zero)")\n'
       f'    (comment 2 "github.com/srspinho/KBD_16_MENU")\n  )\n'
       + lib_txt + ''.join(items) +
       f'  (sheet_instances\n    (path "/" (page "1"))\n  )\n)\n')

with open(OUT, 'w') as fh:
    fh.write(sch)
print('esquemático:', os.path.abspath(OUT), len(items), 'itens')
