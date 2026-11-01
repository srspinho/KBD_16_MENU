"""Confere a netlist exportada pelo KiCad contra design.py (substitui o ERC do KiCad 7)."""
import subprocess
import sys
import os
from collections import defaultdict
from design import COMPONENTS, POWER_NETS
from sexp import parse, find, find1

HERE = os.path.dirname(os.path.abspath(__file__))
SCH = os.path.join(HERE, '..', 'kbd16.kicad_sch')
NET = '/tmp/kbd16_check.net'
subprocess.run(['kicad-cli', 'sch', 'export', 'netlist', '-o', NET, SCH], check=True, capture_output=True)
n = parse(open(NET).read())


def kicad_name(net):
    return net if net in POWER_NETS else '/' + net


# rede esperada por (ref, pino)
expected = {}
for c in COMPONENTS:
    for pin, net in c['pins'].items():
        expected[(c['ref'], pin)] = kicad_name(net) if net else None

got = {}
pins_per_net = defaultdict(list)
for net in find(find1(n, 'nets'), 'net'):
    name = find1(net, 'name')[1]
    for node in find(net, 'node'):
        ref, pin = find1(node, 'ref')[1], find1(node, 'pin')[1]
        if ref.startswith('#'):
            continue
        got[(ref, pin)] = name
        pins_per_net[name].append(f'{ref}.{pin}')

erros = 0
for key, exp in expected.items():
    g = got.get(key)
    if exp is None:
        if g is not None and not g.startswith('unconnected-'):
            print('ERRO pino deveria estar livre:', key, g); erros += 1
    elif g != exp:
        print('ERRO', key, 'esperado', exp, 'obtido', g); erros += 1
for name, pins in pins_per_net.items():
    if not name.startswith('unconnected-') and len(pins) < 2:
        print('ERRO rede com 1 pino só:', name, pins); erros += 1
comps = {find1(c, 'ref')[1]: find1(c, 'footprint')[1] for c in find(find1(n, 'components'), 'comp')}
for c in COMPONENTS:
    if comps.get(c['ref']) != c['footprint']:
        print('ERRO footprint', c['ref'], comps.get(c['ref'])); erros += 1
nets = sorted(k for k in pins_per_net if not k.startswith('unconnected-'))
print(f'{len(comps)} componentes, {len(nets)} redes, {len(got)} pinos ligados')
print('OK: esquemático confere com design.py' if not erros else f'{erros} ERROS')
sys.exit(1 if erros else 0)
