"""Parser/serializador mínimo de S-expressions do KiCad."""
import re

_TOKEN = re.compile(r'\s*(?:(\()|(\))|("(?:[^"\\]|\\.)*")|([^\s()"]+))')


class Sym(str):
    """Átomo sem aspas (palavra-chave ou número)."""


def parse(text):
    stack, cur = [], []
    pos = 0
    while pos < len(text):
        m = _TOKEN.match(text, pos)
        if not m:
            break
        pos = m.end()
        if m.group(1):
            stack.append(cur)
            cur = []
        elif m.group(2):
            done = cur
            cur = stack.pop()
            cur.append(done)
        elif m.group(3) is not None:
            cur.append(m.group(3)[1:-1].replace('\\"', '"').replace('\\\\', '\\'))
        elif m.group(4):
            cur.append(Sym(m.group(4)))
    return cur[0] if len(cur) == 1 else cur


def q(s):
    return '"' + str(s).replace('\\', '\\\\').replace('"', '\\"') + '"'


def dump(node, indent=0):
    if not isinstance(node, list):
        if isinstance(node, Sym):
            return str(node)
        if isinstance(node, (int, float)):
            return fmt_num(node)
        return q(node)
    simple = all(not isinstance(x, list) for x in node)
    if simple:
        return '(' + ' '.join(dump(x) for x in node) + ')'
    pad = '  ' * (indent + 1)
    parts = ['(' + dump(node[0])]
    for x in node[1:]:
        if isinstance(x, list):
            parts.append('\n' + pad + dump(x, indent + 1))
        else:
            parts.append(' ' + dump(x))
    return ''.join(parts) + ')'


def fmt_num(v):
    if isinstance(v, int):
        return str(v)
    s = ('%.4f' % v).rstrip('0').rstrip('.')
    return '0' if s in ('-0', '') else s


def find(node, key):
    return [x for x in node if isinstance(x, list) and x and x[0] == key]


def find1(node, key):
    r = find(node, key)
    return r[0] if r else None
