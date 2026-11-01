"""
Imagens "realistas" da placa (frente e verso) em hardware/img/.

Cada camada é exportada pelo kicad-cli em SVG preto e branco e recolorida
(máscara verde, cobre, pads dourados, serigrafia branca). Os furos são
desenhados por cima a partir dos dados da própria placa (API pcbnew).
"""
import os
import re
import subprocess
import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
HW = os.path.abspath(os.path.join(HERE, '..'))
PCB = os.path.join(HW, 'kbd16.kicad_pcb')
IMG = os.path.join(HW, 'img')
TMP = '/tmp/kbd16_render'

BOARD = '#1f5f30'   # máscara sobre fibra
COPPER = '#2f8042'  # máscara sobre cobre
PAD = '#d9b45a'     # pads (acabamento HASL/ENIG)
SILK = '#f2f2f2'
HOLE = '#151515'


def layer_svg(layer, mirror):
    out = os.path.join(TMP, f'{layer}{"_m" if mirror else ""}.svg')
    args = ['kicad-cli', 'pcb', 'export', 'svg', '--exclude-drawing-sheet', '--page-size-mode', '2',
            '--black-and-white', '--drill-shape-opt', '0', '-l', layer + ',Edge.Cuts', '-o', out, PCB]
    if mirror:
        args.insert(4, '--mirror')
    subprocess.run(args, check=True, capture_output=True)
    return open(out).read()


def body(svg, color):
    b = re.sub(r'^.*?<svg[^>]*>', '', svg, flags=re.S).rsplit('</svg>', 1)[0]
    b = re.sub(r'<title>.*?</title>|<desc>.*?</desc>', '', b, flags=re.S)
    return re.sub(r'(fill|stroke):#[0-9A-Fa-f]{6}', lambda m: f'{m.group(1)}:{color}', b)


def build(side, board):
    mirror = side == 'verso'
    p = 'B' if mirror else 'F'
    edge = layer_svg('Edge.Cuts', mirror)
    vb = re.search(r'viewBox="([^"]+)"', edge).group(1)
    pts = [(float(a), float(b)) for a, b in re.findall(r'M\s*(-?[\d.]+)[ ,](-?[\d.]+)', edge)]
    bx0, by0 = min(x for x, _ in pts), min(y for _, y in pts)
    bx1, by1 = max(x for x, _ in pts), max(y for _, y in pts)

    # posição da placa na folha (mm) -> coordenada do SVG
    bb = board.GetBoardEdgesBoundingBox()
    px0, py0 = pcbnew.ToMM(bb.GetLeft()), pcbnew.ToMM(bb.GetTop())
    px1 = pcbnew.ToMM(bb.GetRight())

    def to_svg(x, y):
        sx = (bx0 + (px1 - x)) if mirror else (bx0 + (x - px0))
        return sx, by0 + (y - py0)

    holes = []
    for fp in board.GetFootprints():
        for pad in fp.Pads():
            d = pad.GetDrillSize()
            if d.x > 0:
                x, y = to_svg(pcbnew.ToMM(pad.GetPosition().x), pcbnew.ToMM(pad.GetPosition().y))
                holes.append((x, y, pcbnew.ToMM(min(d.x, d.y)) / 2))
    for t in board.GetTracks():
        if t.GetClass() == 'PCB_VIA':
            x, y = to_svg(pcbnew.ToMM(t.GetPosition().x), pcbnew.ToMM(t.GetPosition().y))
            holes.append((x, y, pcbnew.ToMM(t.GetDrillValue()) / 2))
    g_holes = ''.join(f'<circle cx="{x:.3f}" cy="{y:.3f}" r="{r:.3f}" fill="{HOLE}"/>' for x, y, r in holes)

    svg = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="{vb}" width="1600">'
           f'<defs><clipPath id="placa"><rect x="{bx0}" y="{by0}" width="{bx1 - bx0}" height="{by1 - by0}" rx="2"/></clipPath></defs>'
           f'<rect x="{bx0}" y="{by0}" width="{bx1 - bx0}" height="{by1 - by0}" rx="2" fill="{BOARD}"/>'
           f'<g>{body(layer_svg(p + ".Cu", mirror), COPPER)}</g>'
           f'<g>{body(layer_svg(p + ".Mask", mirror), PAD)}</g>'
           f'<g clip-path="url(#placa)">{body(layer_svg(p + ".Silkscreen", mirror), SILK)}</g>'
           f'{g_holes}'
           f'<g>{body(edge, "#0b2a14")}</g></svg>')
    path = os.path.join(TMP, f'{side}.svg')
    open(path, 'w').write(svg)
    out = os.path.join(IMG, f'placa_{side}.png')
    subprocess.run(['node', os.path.join(HERE, 'svg2png.js'), path, out, '1600'], check=True)
    print('imagem:', out)


if __name__ == '__main__':
    os.makedirs(TMP, exist_ok=True)
    os.makedirs(IMG, exist_ok=True)
    b = pcbnew.LoadBoard(PCB)
    for side in ('frente', 'verso'):
        build(side, b)
