"""Roda o DRC do KiCad (API) e resume. Sai com erro se houver violações de severidade 'error'."""
import re
import sys
import pcbnew
from gen_pcb import OUT
from route import setup_netclasses

IGNORE = {'lib_footprint_issues', 'lib_footprint_mismatch'}   # tabelas de biblioteca não carregadas no modo script
REPORT = sys.argv[1] if len(sys.argv) > 1 else '/tmp/kbd16_drc.rpt'

b = pcbnew.LoadBoard(OUT)
setup_netclasses(b)   # o KiCad 7 em script não lê as classes do .kicad_pro
pcbnew.WriteDRCReport(b, REPORT, pcbnew.EDA_UNITS_MILLIMETRES, True)
txt = open(REPORT).read()
blocks = re.split(r'\n(?=\[)', txt)
counts = {}
errors = []
for blk in blocks:
    m = re.match(r'\[(\w+)\]', blk)
    if not m:
        continue
    kind = m.group(1)
    sev = 'error' if 'Severity: error' in blk else 'warning'
    if kind in IGNORE:
        continue
    counts[(kind, sev)] = counts.get((kind, sev), 0) + 1
    if sev == 'error':
        errors.append(blk.strip())
unc = re.search(r'Found (\d+) unconnected', txt)
print('Resumo do DRC:')
for (k, s), n in sorted(counts.items()):
    print(f'  {s:7s} {k:28s} {n}')
print('  pads desconectados:', unc.group(1) if unc else '?')
for e in errors[:15]:
    print('---\n' + e)

# Esquemático x PCB: cada pad deve estar na mesma rede da netlist do esquemático
import subprocess
from sexp import parse, find, find1
SCH = OUT.replace('.kicad_pcb', '.kicad_sch')
subprocess.run(['kicad-cli', 'sch', 'export', 'netlist', '-o', '/tmp/kbd16_sync.net', SCH], check=True, capture_output=True)
nl = parse(open('/tmp/kbd16_sync.net').read())
sch_net = {}
for net in find(find1(nl, 'nets'), 'net'):
    name = find1(net, 'name')[1]
    for node in find(net, 'node'):
        sch_net[(find1(node, 'ref')[1], find1(node, 'pin')[1])] = name
diverg = 0
for fp in b.GetFootprints():
    for pad in fp.Pads():
        if not pad.GetNumber():
            continue
        key = (fp.GetReference(), pad.GetNumber())
        s_net = sch_net.get(key, '')
        if s_net.startswith('unconnected-'):
            s_net = ''
        if pad.GetNetname() != s_net:
            print('DIVERGE', key, 'pcb=', pad.GetNetname(), 'sch=', s_net)
            diverg += 1
print('Esquemático x PCB:', 'sincronizados' if not diverg else f'{diverg} divergências')
sys.exit(1 if errors or diverg else 0)
