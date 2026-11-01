"""
Regenera a placa inteira a partir de design.py:
  biblioteca -> esquemático -> conferência -> PCB -> roteamento -> DRC -> fabricação -> imagens

Uso (na pasta hardware/tools):  python3 build_all.py
Requer KiCad 7 (kicad-cli e o módulo Python pcbnew). As imagens precisam de
Node + Playwright (opcional).
"""
import subprocess
import sys

STEPS = [
    ('Biblioteca do projeto', ['gen_lib.py']),
    ('Esquemático', ['gen_sch.py']),
    ('Conferência esquemático x design.py', ['check_sch.py']),
    ('PCB (posicionamento)', ['gen_pcb.py']),
    ('Roteamento + planos de GND', ['route.py']),
    ('DRC', ['drc.py']),
    ('Arquivos de fabricação', ['export.py']),
    ('Imagens', ['render.py']),
]

for title, args in STEPS:
    print(f'\n=== {title} ===', flush=True)
    r = subprocess.run([sys.executable] + args)
    if r.returncode != 0:
        if title == 'Imagens':
            print('(imagens não geradas: instale Node + Playwright)')
            continue
        sys.exit(f'falhou em: {title}')
print('\nPronto.')
