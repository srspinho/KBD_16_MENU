"""
Roteador automático simples (A* em grade, 2 camadas) para a placa KBD16.

Não depende de programas externos: lê a placa com a API do KiCad (pcbnew),
roteia cada rede respeitando largura e folga da sua classe e grava trilhas
e vias. A verificação final é o DRC do próprio KiCad (drc.py).

Estratégia:
  - grade de 0,2 mm, movimentos em 8 direções e troca de camada por via;
  - cada rede é ligada pad a pad (árvore crescente, pad mais próximo primeiro);
  - obstáculos = pads/trilhas/vias de outras redes, inflados por
    (meia largura própria + folga + meia largura do obstáculo + margem);
  - se uma rede falha, ela vai para o início da fila e tudo é refeito
    (rip-up & reroute), e redes de potência podem cair para trilha mais fina.
"""
import heapq
import math
import sys
import time
import numpy as np
import pcbnew

G = 0.2                 # passo da grade (mm)
MARGIN = 0.06           # folga extra para compensar a discretização
EDGE_CLEAR = 0.4        # distância mínima cobre-borda (mm)
HOLE_KEEPOUT = 3.3      # raio livre em volta dos furos M3 (cabeça do parafuso)
VIA_COST = 12.0
WRONG_DIR_COST = 0.25   # leve preferência: F.Cu horizontal, B.Cu vertical

mm = pcbnew.FromMM
to_mm = pcbnew.ToMM


class Rules:
    def __init__(self, signal_w=0.25, power_w=0.6, clearance=0.2, power_clear=0.25,
                 via_d=0.8, via_drill=0.4, power_via_d=0.9, power_via_drill=0.45, power_nets=()):
        self.signal_w, self.power_w = signal_w, power_w
        self.clearance, self.power_clear = clearance, power_clear
        self.via_d, self.via_drill = via_d, via_drill
        self.power_via_d, self.power_via_drill = power_via_d, power_via_drill
        self.power_nets = set(power_nets)

    def width(self, net):
        return self.power_w if net in self.power_nets else self.signal_w

    def clear(self, net):
        return self.power_clear if net in self.power_nets else self.clearance

    def via(self, net):
        return (self.power_via_d, self.power_via_drill) if net in self.power_nets else (self.via_d, self.via_drill)


class Obstacle:
    """Forma de cobre: 'rect' (x0,y0,x1,y1), 'circle' (cx,cy,r) ou 'seg' (x0,y0,x1,y1,hw)."""
    __slots__ = ('net', 'layers', 'kind', 'g', 'clear')

    def __init__(self, net, layers, kind, g, clear):
        self.net, self.layers, self.kind, self.g, self.clear = net, layers, kind, g, clear

    def bbox(self):
        k, g = self.kind, self.g
        if k == 'rect':
            return g[0], g[1], g[2], g[3]
        if k == 'circle':
            return g[0] - g[2], g[1] - g[2], g[0] + g[2], g[1] + g[2]
        return min(g[0], g[2]) - g[4], min(g[1], g[3]) - g[4], max(g[0], g[2]) + g[4], max(g[1], g[3]) + g[4]

    def dist(self, X, Y):
        k, g = self.kind, self.g
        if k == 'rect':
            dx = np.maximum(np.maximum(g[0] - X, X - g[2]), 0)
            dy = np.maximum(np.maximum(g[1] - Y, Y - g[3]), 0)
            return np.hypot(dx, dy)
        if k == 'circle':
            return np.maximum(np.hypot(X - g[0], Y - g[1]) - g[2], 0)
        x0, y0, x1, y1, hw = g
        vx, vy = x1 - x0, y1 - y0
        L2 = vx * vx + vy * vy
        if L2 == 0:
            t = 0
        else:
            t = np.clip(((X - x0) * vx + (Y - y0) * vy) / L2, 0, 1)
        return np.maximum(np.hypot(X - (x0 + t * vx), Y - (y0 + t * vy)) - hw, 0)


class Router:
    def __init__(self, board, rules, origin, size, corner_r=2.0, holes=()):
        self.board, self.rules = board, rules
        self.ox, self.oy = origin
        self.W, self.H = size
        self.nx = int(round(self.W / G)) + 1
        self.ny = int(round(self.H / G)) + 1
        xs = np.arange(self.nx) * G
        ys = np.arange(self.ny) * G
        self.X, self.Y = np.meshgrid(xs, ys, indexing='ij')
        self.corner_r = corner_r
        self.holes = holes
        self.edge_dist = self._edge_distance()
        self.pads = {}          # net -> [(layers, Obstacle, center)]
        self.fixed = []         # obstáculos fixos (pads, furos)
        self.routed = {}        # net -> list of obstáculos de trilha/via
        self.tracks = {}        # net -> [(layer, x0,y0,x1,y1,w)]
        self.vias = {}          # net -> [(x,y,d,drill)]
        self._collect_pads()

    # ------------------------------------------------------------ geometria
    def _edge_distance(self):
        r = self.corner_r
        cx, cy = self.W / 2, self.H / 2
        qx = np.abs(self.X - cx) - (cx - r)
        qy = np.abs(self.Y - cy) - (cy - r)
        outside = np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - r
        return -outside   # distância até a borda (positiva dentro)

    def _collect_pads(self):
        for fp in self.board.GetFootprints():
            for pad in fp.Pads():
                net = pad.GetNetname() or None
                layers = set()
                if pad.IsOnLayer(pcbnew.F_Cu):
                    layers.add(0)
                if pad.IsOnLayer(pcbnew.B_Cu):
                    layers.add(1)
                c = pad.GetPosition()
                cx, cy = to_mm(c.x) - self.ox, to_mm(c.y) - self.oy
                if pad.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH:
                    r = to_mm(pad.GetDrillSize().x) / 2
                    ob = Obstacle(None, {0, 1}, 'circle', (cx, cy, r), self.rules.clearance)
                    self.fixed.append(ob)
                    continue
                if not layers:
                    continue
                bb = pad.GetBoundingBox()
                x0, y0 = to_mm(bb.GetLeft()) - self.ox, to_mm(bb.GetTop()) - self.oy
                x1, y1 = to_mm(bb.GetRight()) - self.ox, to_mm(bb.GetBottom()) - self.oy
                if pad.GetShape() == pcbnew.PAD_SHAPE_CIRCLE:
                    ob = Obstacle(net, layers, 'circle', (cx, cy, (x1 - x0) / 2), self.rules.clear(net))
                else:
                    ob = Obstacle(net, layers, 'rect', (x0, y0, x1, y1), self.rules.clear(net))
                self.fixed.append(ob)
                if net:
                    self.pads.setdefault(net, []).append((layers, ob, (cx, cy), pad.GetAttribute() == pcbnew.PAD_ATTRIB_SMD, pad))

    def _rasterize(self, net, inflate_fn, layers_needed):
        """Grade booleana por camada: True = bloqueado para a rede 'net'."""
        blocked = np.zeros((2, self.nx, self.ny), dtype=bool)
        obs = list(self.fixed)
        for n, lst in self.routed.items():
            if n != net:
                obs.extend(lst)
        for ob in obs:
            if ob.net is not None and ob.net == net:
                continue
            infl = inflate_fn(ob)
            bx0, by0, bx1, by1 = ob.bbox()
            i0 = max(int(math.floor((bx0 - infl) / G)), 0)
            i1 = min(int(math.ceil((bx1 + infl) / G)), self.nx - 1)
            j0 = max(int(math.floor((by0 - infl) / G)), 0)
            j1 = min(int(math.ceil((by1 + infl) / G)), self.ny - 1)
            if i0 > i1 or j0 > j1:
                continue
            d = ob.dist(self.X[i0:i1 + 1, j0:j1 + 1], self.Y[i0:i1 + 1, j0:j1 + 1])
            m = d < infl
            for L in ob.layers:
                if L in layers_needed:
                    blocked[L, i0:i1 + 1, j0:j1 + 1] |= m
        return blocked

    def _hole_mask(self, extra):
        m = np.zeros((self.nx, self.ny), dtype=bool)
        for (hx, hy) in self.holes:
            m |= np.hypot(self.X - hx, self.Y - hy) < HOLE_KEEPOUT + extra
        return m

    # ------------------------------------------------------------- roteamento
    def route_net(self, net, width, clear):
        pads = self.pads.get(net, [])
        if len(pads) < 2:
            return True
        hw = width / 2
        blocked = self._rasterize(net, lambda ob: hw + max(clear, ob.clear) + MARGIN, {0, 1})
        edge_bad = self.edge_dist < EDGE_CLEAR + hw + MARGIN
        holes = self._hole_mask(hw)
        blocked[0] |= edge_bad | holes
        blocked[1] |= edge_bad | holes
        via_d, via_drill = self.rules.via(net)
        vr = via_d / 2
        vblk = self._rasterize(net, lambda ob: vr + max(clear, ob.clear) + MARGIN, {0, 1})
        via_ok = ~(vblk[0] | vblk[1] | (self.edge_dist < EDGE_CLEAR + vr + MARGIN) | self._hole_mask(vr))
        # sem via dentro de pads SMD da própria rede
        for layers, ob, c, smd, _ in pads:
            if smd:
                bx0, by0, bx1, by1 = ob.bbox()
                via_ok &= ~((self.X > bx0 - vr - 0.15) & (self.X < bx1 + vr + 0.15) &
                            (self.Y > by0 - vr - 0.15) & (self.Y < by1 + vr + 0.15))

        pad_center = {}   # célula -> centro do pad (para o trecho final até o centro)

        def pad_cells(entry):
            layers, ob, c, smd, pad = entry
            bx0, by0, bx1, by1 = ob.bbox()
            i0, i1 = max(int(math.ceil(bx0 / G)), 0), min(int(math.floor(bx1 / G)), self.nx - 1)
            j0, j1 = max(int(math.ceil(by0 / G)), 0), min(int(math.floor(by1 / G)), self.ny - 1)
            cells = []
            for i in range(i0, i1 + 1):
                for j in range(j0, j1 + 1):
                    # dentro do cobre real do pad, com folga de meia trilha
                    pt = pcbnew.VECTOR2I(mm(i * G + self.ox), mm(j * G + self.oy))
                    if pad.HitTest(pt, 0) and math.hypot(i * G - c[0], j * G - c[1]) <= max(0.0, min(bx1 - bx0, by1 - by0) / 2 - 0.05) + 1e-6 \
                            or (pad.HitTest(pt, 0) and ob.kind == 'rect' and pad.GetShape() != pcbnew.PAD_SHAPE_OVAL):
                        for L in layers:
                            cells.append((L, i, j))
            if not cells:   # pad menor que a grade: célula mais próxima do centro
                i, j = int(round(c[0] / G)), int(round(c[1] / G))
                cells = [(L, i, j) for L in layers]
            for cell in cells:
                pad_center[cell] = c
            return cells

        # ordem: começa pelo pad mais "isolado" e cresce a árvore pelo mais próximo
        remaining = list(range(len(pads)))
        tree = set(pad_cells(pads[remaining.pop(0)]))
        new_tracks, new_vias = [], []
        while remaining:
            tc = [(c[1] * G, c[2] * G) for c in tree]
            tx = np.array([p[0] for p in tc]); ty = np.array([p[1] for p in tc])
            remaining.sort(key=lambda k: np.min(np.hypot(tx - pads[k][2][0], ty - pads[k][2][1])))
            k = remaining.pop(0)
            target = set(pad_cells(pads[k]))
            path = self._astar(tree, target, blocked, via_ok, pads[k][2])
            if path is None:
                return False
            tr, vi = self._path_to_geometry(path, width, via_d, via_drill, blocked)
            # trechos até o centro dos pads nas pontas (garante a conexão)
            for end in (path[0], path[-1]):
                c = pad_center.get(end)
                if c and (abs(end[1] * G - c[0]) > 1e-6 or abs(end[2] * G - c[1]) > 1e-6):
                    tr.append((end[0], end[1] * G, end[2] * G, c[0], c[1], width))
            new_tracks += tr
            new_vias += vi
            # a árvore passa a ser o cobre real (trilhas suavizadas), não as células da busca
            tree |= self._cells_on_tracks(tr) | {(L, int(round(v[0] / G)), int(round(v[1] / G))) for v in vi for L in (0, 1)} | target
        self.tracks[net] = new_tracks
        self.vias[net] = new_vias
        obs = [Obstacle(net, {t[0]}, 'seg', (t[1], t[2], t[3], t[4], t[5] / 2), clear) for t in new_tracks]
        obs += [Obstacle(net, {0, 1}, 'circle', (v[0], v[1], v[2] / 2), clear) for v in new_vias]
        self.routed[net] = obs
        return True

    def _astar(self, sources, targets, blocked, via_ok, goal):
        nx, ny = self.nx, self.ny
        gx, gy = goal[0] / G, goal[1] / G
        tset = targets
        best = {}
        came = {}
        heap = []
        for s in sources:
            L, i, j = s
            if 0 <= i < nx and 0 <= j < ny:
                best[s] = 0.0
                heapq.heappush(heap, (math.hypot(i - gx, j - gy), 0.0, s))
        moves = [(1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0),
                 (1, 1, 1.4142), (1, -1, 1.4142), (-1, 1, 1.4142), (-1, -1, 1.4142)]
        expanded = 0
        while heap:
            f, gcost, cur = heapq.heappop(heap)
            if gcost > best.get(cur, 1e18) + 1e-9:
                continue
            if cur in tset:
                path = [cur]
                while cur in came:
                    cur = came[cur]
                    path.append(cur)
                return path[::-1]
            expanded += 1
            if expanded > 400000:
                return None
            L, i, j = cur
            for di, dj, c in moves:
                ni, nj = i + di, j + dj
                if not (0 <= ni < nx and 0 <= nj < ny):
                    continue
                nxt = (L, ni, nj)
                if blocked[L, ni, nj] and nxt not in tset:
                    continue
                if di and dj:   # diagonal: não corta quina de obstáculo
                    if blocked[L, i + di, j] or blocked[L, i, j + dj]:
                        continue
                horiz = dj == 0
                pen = 0.0
                if (L == 0 and not horiz and di == 0) or (L == 1 and horiz):
                    pen = WRONG_DIR_COST
                ng = gcost + c + pen
                if ng < best.get(nxt, 1e18):
                    best[nxt] = ng
                    came[nxt] = cur
                    heapq.heappush(heap, (ng + math.hypot(ni - gx, nj - gy), ng, nxt))
            if via_ok[i, j]:
                nxt = (1 - L, i, j)
                if not blocked[1 - L, i, j] or nxt in tset:
                    ng = gcost + VIA_COST
                    if ng < best.get(nxt, 1e18):
                        best[nxt] = ng
                        came[nxt] = cur
                        heapq.heappush(heap, (ng + math.hypot(i - gx, j - gy), ng, nxt))
        return None

    def _clear_line(self, L, a, b, blocked, ends_ok):
        """True se a reta a->b (em células) não encosta em célula bloqueada."""
        (i0, j0), (i1, j1) = a, b
        n = max(2, int(math.hypot(i1 - i0, j1 - j0) * 4))
        for k in range(n + 1):
            t = k / n
            x, y = i0 + (i1 - i0) * t, j0 + (j1 - j0) * t
            for ci in {math.floor(x), math.ceil(x)}:
                for cj in {math.floor(y), math.ceil(y)}:
                    if blocked[L, ci, cj] and (L, ci, cj) not in ends_ok:
                        return False
        return True

    def _smooth(self, L, pts, blocked, ends_ok):
        """Puxa a "corda": liga cada ponto ao mais distante visível em linha reta."""
        out = [pts[0]]
        i = 0
        while i < len(pts) - 1:
            j = len(pts) - 1
            while j > i + 1 and not self._clear_line(L, pts[i], pts[j], blocked, ends_ok):
                j -= 1
            out.append(pts[j])
            i = j
        return out

    def _path_to_geometry(self, path, width, via_d, via_drill, blocked):
        tracks, vias = [], []
        if len(path) < 2:
            return tracks, vias
        ends_ok = {path[0], path[-1]}
        runs = [[path[0]]]
        for cur in path[1:]:
            if cur[0] != runs[-1][-1][0]:
                vias.append((runs[-1][-1][1] * G, runs[-1][-1][2] * G, via_d, via_drill))
                runs.append([cur])
            else:
                runs[-1].append(cur)
        for run in runs:
            if len(run) < 2:
                continue
            L = run[0][0]
            pts = self._smooth(L, [(c[1], c[2]) for c in run], blocked, ends_ok)
            for a, b in zip(pts, pts[1:]):
                tracks.append((L, a[0] * G, a[1] * G, b[0] * G, b[1] * G, width))
        return tracks, vias

    def _cells_on_tracks(self, tracks):
        cells = set()
        for (L, x0, y0, x1, y1, w) in tracks:
            n = max(2, int(math.hypot(x1 - x0, y1 - y0) / 0.05))
            for k in range(n + 1):
                x, y = x0 + (x1 - x0) * k / n, y0 + (y1 - y0) * k / n
                i, j = int(round(x / G)), int(round(y / G))
                if math.hypot(i * G - x, j * G - y) <= 0.1:
                    cells.add((L, i, j))
        return cells

    # --------------------------------------------------------------- escrita
    def commit(self):
        for t in list(self.board.GetTracks()):
            self.board.Remove(t)
        for net, tracks in self.tracks.items():
            ni = self.board.FindNet(net)
            for (L, x0, y0, x1, y1, w) in tracks:
                t = pcbnew.PCB_TRACK(self.board)
                t.SetStart(pcbnew.VECTOR2I(mm(x0 + self.ox), mm(y0 + self.oy)))
                t.SetEnd(pcbnew.VECTOR2I(mm(x1 + self.ox), mm(y1 + self.oy)))
                t.SetWidth(mm(w))
                t.SetLayer(pcbnew.F_Cu if L == 0 else pcbnew.B_Cu)
                t.SetNet(ni)
                self.board.Add(t)
            for (x, y, d, drill) in self.vias.get(net, []):
                v = pcbnew.PCB_VIA(self.board)
                v.SetPosition(pcbnew.VECTOR2I(mm(x + self.ox), mm(y + self.oy)))
                v.SetWidth(mm(d))
                v.SetDrill(mm(drill))
                v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
                v.SetNet(ni)
                self.board.Add(v)


def route_all(board, rules, origin, size, order, holes=(), attempts=8, log=print):
    order = list(order)
    for attempt in range(1, attempts + 1):
        r = Router(board, rules, origin, size, holes=holes)
        failed = []
        t0 = time.time()
        for net in order:
            w, c = rules.width(net), rules.clear(net)
            ok = r.route_net(net, w, c)
            if not ok and net in rules.power_nets:
                # potência: tenta trilha mais fina antes de desistir
                for w2 in (0.4, 0.3):
                    ok = r.route_net(net, w2, c)
                    if ok:
                        log(f'  {net}: roteada com {w2} mm')
                        break
            if not ok:
                failed.append(net)
        log(f'tentativa {attempt}: {len(order) - len(failed)}/{len(order)} redes em {time.time() - t0:.1f}s'
            + (f' (falharam: {", ".join(failed)})' if failed else ''))
        if not failed:
            return r
        order = failed + [n for n in order if n not in failed]
    return None
