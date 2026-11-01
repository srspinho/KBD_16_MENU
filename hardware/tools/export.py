"""Gera os arquivos de fabricação em hardware/fab/ (Gerbers, furação, BOM, posição, PDF, imagens)."""
import csv
import os
import shutil
import subprocess
import zipfile
from collections import OrderedDict
from design import COMPONENTS

HERE = os.path.dirname(os.path.abspath(__file__))
HW = os.path.abspath(os.path.join(HERE, '..'))
PCB = os.path.join(HW, 'kbd16.kicad_pcb')
SCH = os.path.join(HW, 'kbd16.kicad_sch')
FAB = os.path.join(HW, 'fab')
GERB = os.path.join(FAB, 'gerbers')
EXTRAS = [
    [2, '(U1)', '1x9 fêmea 2,54 mm', 'Barras para encaixar o RP2350-Zero (ou solde o módulo direto com barras macho)', '', ''],
    [1, '(U1)', '1x5 fêmea 2,54 mm', 'Barra da fileira de baixo do RP2350-Zero (GP9-GP13)', '', ''],
    [1, '(BT1)', 'CR1220', 'Bateria do RTC (opcional)', '', ''],
    [4, '(H1-H4)', 'M3', 'Parafusos M3 + espaçadores de 10 mm', '', ''],
]
LAYERS = 'F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts'


def run(*args):
    r = subprocess.run(args, capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f'falhou: {" ".join(args)}\n{r.stdout}\n{r.stderr}')


def main():
    shutil.rmtree(FAB, ignore_errors=True)
    os.makedirs(GERB)
    run('kicad-cli', 'pcb', 'export', 'gerbers', '-l', LAYERS, '--subtract-soldermask',
        '--use-drill-file-origin', '-o', GERB + '/', PCB)
    run('kicad-cli', 'pcb', 'export', 'drill', '--drill-origin', 'plot', '--excellon-separate-th',
        '--generate-map', '--map-format', 'pdf', '-o', GERB + '/', PCB)
    with zipfile.ZipFile(os.path.join(FAB, 'kbd16_gerbers.zip'), 'w', zipfile.ZIP_DEFLATED) as z:
        for f in sorted(os.listdir(GERB)):
            if not f.endswith('.pdf'):
                z.write(os.path.join(GERB, f), f)
    run('kicad-cli', 'pcb', 'export', 'pos', '--side', 'both', '--format', 'csv', '--units', 'mm',
        '--use-drill-file-origin', '-o', os.path.join(FAB, 'kbd16_posicao.csv'), PCB)
    run('kicad-cli', 'sch', 'export', 'pdf', '-o', os.path.join(FAB, 'kbd16_esquematico.pdf'), SCH)
    # placa em escala 1:1 (imprima em A4 "tamanho real" para conferir as peças)
    run('kicad-cli', 'pcb', 'export', 'pdf', '-l', 'F.Cu,F.Silkscreen,F.Fab,Edge.Cuts',
        '-o', os.path.join(FAB, 'kbd16_placa_1x1.pdf'), PCB)

    # BOM agrupada por valor + footprint
    groups = OrderedDict()
    for c in COMPONENTS:
        if c['ref'].startswith('H'):
            continue
        g = groups.setdefault((c['value'], c['footprint']), {'refs': [], 'desc': [], 'mpn': c['mpn']})
        g['refs'].append(c['ref'])
        if c['desc'] and c['desc'] not in g['desc']:
            g['desc'].append(c['desc'])
    with open(os.path.join(FAB, 'kbd16_bom.csv'), 'w', newline='') as fh:
        w = csv.writer(fh)
        w.writerow(['Qtd', 'Referências', 'Valor', 'Descrição', 'Peça sugerida', 'Footprint'])
        for (value, fp), g in groups.items():
            w.writerow([len(g['refs']), ' '.join(g['refs']), value, ' / '.join(g['desc']), g['mpn'], fp.split(':')[1]])
        for row in EXTRAS:
            w.writerow(row)
    print('arquivos em', FAB)
    for f in sorted(os.listdir(FAB)):
        print('  ', f)


if __name__ == '__main__':
    main()
