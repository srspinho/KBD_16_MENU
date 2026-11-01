"""UUIDs estáveis compartilhados entre o esquemático e a PCB."""
import uuid

NS = uuid.UUID('5e3a7c1e-1d2b-4b7a-8f00-6b6264313600')


def sid(*parts):
    return str(uuid.uuid5(NS, '/'.join(str(p) for p in parts)))
