"""
Gera kbd16.kicad_pcb a partir de design.py: contorno, footprints posicionados,
redes nos pads e vínculo com os símbolos do esquemático (UUIDs do gen_sch.py).
O roteamento é feito depois por route.py (Freerouting).
"""
import os
import pcbnew
from design import COMPONENTS, BOARD_W, BOARD_H, POWER_NETS
from ids import sid

HERE = os.path.dirname(os.path.abspath(__file__))
HW = os.path.abspath(os.path.join(HERE, '..'))
OUT = os.path.join(HW, 'kbd16.kicad_pcb')
FPDIR = '/usr/share/kicad/footprints'
ORIGIN = (50.0, 50.0)   # canto superior esquerdo da placa na folha (mm)

mm = pcbnew.FromMM


def P(x, y):
    return pcbnew.VECTOR2I(mm(ORIGIN[0] + x), mm(ORIGIN[1] + y))


def kicad_net_name(net):
    return net if net in POWER_NETS else '/' + net


def load_fp(fpid):
    lib, name = fpid.split(':')
    path = os.path.join(HW, 'lib', 'kbd16.pretty') if lib == 'kbd16' else os.path.join(FPDIR, lib + '.pretty')
    fp = pcbnew.FootprintLoad(path, name)
    if fp is None:
        raise RuntimeError('footprint não encontrado: ' + fpid)
    fp.SetFPID(pcbnew.LIB_ID(lib, name))
    return fp


def edge_rounded_rect(board, w, h, r):
    def seg(x1, y1, x2, y2):
        s = pcbnew.PCB_SHAPE(board)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(P(x1, y1)); s.SetEnd(P(x2, y2))
        s.SetLayer(pcbnew.Edge_Cuts); s.SetWidth(mm(0.1))
        board.Add(s)

    def arc(cx, cy, sx, sy, ang):
        a = pcbnew.PCB_SHAPE(board)
        a.SetShape(pcbnew.SHAPE_T_ARC)
        a.SetCenter(P(cx, cy)); a.SetStart(P(sx, sy))
        a.SetArcAngleAndEnd(pcbnew.EDA_ANGLE(ang, pcbnew.DEGREES_T), True)
        a.SetLayer(pcbnew.Edge_Cuts); a.SetWidth(mm(0.1))
        board.Add(a)
    seg(r, 0, w - r, 0); seg(w, r, w, h - r); seg(w - r, h, r, h); seg(0, h - r, 0, r)
    arc(r, r, 0, r, 90); arc(w - r, r, w - r, 0, 90); arc(w - r, h - r, w, h - r, 90); arc(r, h - r, r, h, 90)


def silk_text(board, txt, x, y, size=1.0, layer=pcbnew.F_SilkS, bold=False, angle=0, justify=None):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(txt)
    t.SetPosition(P(x, y))
    t.SetLayer(layer)
    t.SetTextSize(pcbnew.VECTOR2I(mm(size), mm(size)))
    t.SetTextThickness(mm(size * (0.2 if bold else 0.15)))
    if angle:
        t.SetTextAngleDegrees(angle)
    if justify == 'left':
        t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_LEFT)
    if layer == pcbnew.B_SilkS:
        t.SetMirrored(True)
    board.Add(t)


def build():
    board = pcbnew.CreateEmptyBoard()
    board.SetCopperLayerCount(2)
    ds = board.GetDesignSettings()
    ds.SetAuxOrigin(P(0, 0))
    ds.SetGridOrigin(P(0, 0))
    tb = board.GetTitleBlock()
    tb.SetTitle('KBD16 - HID Remapper & System Monitor')
    tb.SetRevision('1.0')
    tb.SetDate('2026-11-01')
    tb.SetComment(0, 'Placa-base para Waveshare RP2350-Zero (ou RP2040-Zero)')
    tb.SetComment(1, 'github.com/srspinho/KBD_16_MENU')

    nets = {}

    def net(name):
        kn = kicad_net_name(name)
        if kn not in nets:
            ni = pcbnew.NETINFO_ITEM(board, kn)
            board.Add(ni)
            nets[kn] = ni
        return nets[kn]

    for c in COMPONENTS:
        fp = load_fp(c['footprint'])
        fp.SetReference(c['ref'])
        fp.SetValue(c['value'])
        x, y, rot = c['pcb']
        fp.SetPosition(P(x, y))
        fp.SetOrientationDegrees(rot)
        fp.SetPath(pcbnew.KIID_PATH('/' + sid('sym', c['ref'])))
        fp.SetProperty('Sheetname', '')
        fp.SetProperty('Sheetfile', 'kbd16.kicad_sch')
        if c['desc']:
            fp.SetProperty('Description', c['desc'])
        if c['mpn']:
            fp.SetProperty('MPN', c['mpn'])
        if c['ref'].startswith('H') or c['ref'] == 'J1':
            fp.Reference().SetVisible(False)   # J1 é identificado pelo texto TECLADO
        board.Add(fp)
        for pad in fp.Pads():
            num = pad.GetNumber()
            if not num:
                continue
            n = c['pins'].get(num)
            if n:
                pad.SetNet(net(n))   # pads sem conexão ficam sem rede

    edge_rounded_rect(board, BOARD_W, BOARD_H, 2.0)
    return board


def add_texts(board):
    silk_text(board, 'PC', 6.2, 12.0, 1.0, bold=True, angle=90)
    silk_text(board, 'TECLADO', 40.5, 12.65, 0.8, bold=True)
    for i, t in enumerate(['GND', 'CIM', 'BAI', 'ESQ', 'DIR', 'OK']):
        silk_text(board, t, 58.0 + i * 2.54, 48.3, 0.8)
    silk_text(board, 'CR1220', 69.6, 13.0, 0.8, angle=90)
    silk_text(board, 'KBD16 Monitor v1.0', BOARD_W / 2, BOARD_H - 1.6, 1.2, pcbnew.B_SilkS, bold=True)
    silk_text(board, 'github.com/srspinho/KBD_16_MENU', BOARD_W / 2, BOARD_H - 3.4, 0.9, pcbnew.B_SilkS)


if __name__ == '__main__':
    b = build()
    add_texts(b)
    pcbnew.SaveBoard(OUT, b)
    print('PCB salva em', OUT)
