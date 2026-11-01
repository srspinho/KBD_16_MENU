"""Roteia kbd16.kicad_pcb (router.py), cria os planos de GND e salva."""
import os
import sys
import pcbnew
from design import COMPONENTS, BOARD_W, BOARD_H
from router import Rules, route_all
from gen_pcb import ORIGIN, OUT

mm = pcbnew.FromMM
POWER = ['+5V', '/VBUS_KB', '+3V3', 'GND', '/VBAT', '/BZ_DRV']
FIRST = ['/HOST_DP', '/HOST_DM', '/KB_DP', '/KB_DM']


def setup_netclasses(board):
    ns = board.GetDesignSettings().m_NetSettings
    d = ns.m_DefaultNetClass
    d.SetTrackWidth(mm(0.25)); d.SetClearance(mm(0.2)); d.SetViaDiameter(mm(0.8)); d.SetViaDrill(mm(0.4))
    nc = pcbnew.NETCLASS('Power')
    nc.SetTrackWidth(mm(0.6)); nc.SetClearance(mm(0.25)); nc.SetViaDiameter(mm(0.9)); nc.SetViaDrill(mm(0.45))
    ns.m_NetClasses['Power'] = nc
    ptr = ns.m_NetClasses['Power']
    for n in POWER:
        ni = board.FindNet(n)
        if ni:
            ni.SetNetClass(ptr)


def add_gnd_zones(board):
    for z in list(board.Zones()):
        board.Remove(z)
    gnd = board.FindNet('GND')
    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        z = pcbnew.ZONE(board)
        z.SetLayer(layer)
        z.SetNet(gnd)
        z.SetLocalClearance(mm(0.3))
        z.SetMinThickness(mm(0.25))
        z.SetThermalReliefGap(mm(0.4))
        z.SetThermalReliefSpokeWidth(mm(0.5))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
        z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
        z.SetZoneName('GND_' + ('F' if layer == pcbnew.F_Cu else 'B'))
        ol = z.Outline()
        ol.NewOutline()
        for x, y in [(0.3, 0.3), (BOARD_W - 0.3, 0.3), (BOARD_W - 0.3, BOARD_H - 0.3), (0.3, BOARD_H - 0.3)]:
            ol.Append(mm(ORIGIN[0] + x), mm(ORIGIN[1] + y))
        board.Add(z)
    # blindagem do USB-A: conexão sólida ao plano (pad grande junto à borda)
    for fp in board.GetFootprints():
        if fp.GetReference() == 'J1':
            for pad in fp.Pads():
                if pad.GetNumber() == '5':
                    pad.SetZoneConnection(pcbnew.ZONE_CONNECTION_FULL)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())


def main():
    board = pcbnew.LoadBoard(OUT)
    setup_netclasses(board)
    rules = Rules(power_nets=POWER)
    nets = sorted({p for c in COMPONENTS for p in c['pins'].values() if p})
    knames = [n if n in ('+5V', '+3V3', 'GND') else '/' + n for n in nets]
    order = FIRST + [n for n in POWER if n in knames and n not in FIRST and n != 'GND'] + \
        [n for n in knames if n not in FIRST and n not in POWER] + ['GND']
    holes = [(c['pcb'][0], c['pcb'][1]) for c in COMPONENTS if c['ref'].startswith('H')]
    r = route_all(board, rules, ORIGIN, (BOARD_W, BOARD_H), order, holes=holes)
    if r is None:
        print('ROTEAMENTO FALHOU')
        sys.exit(1)
    r.commit()
    add_gnd_zones(board)
    pcbnew.SaveBoard(OUT, board)
    nt = sum(len(v) for v in r.tracks.values()); nv = sum(len(v) for v in r.vias.values())
    print(f'roteado: {nt} trilhas, {nv} vias -> {OUT}')


if __name__ == '__main__':
    main()
