import struct, zlib

class Node:
    def __init__(self, name, props, children):
        self.name, self.props, self.children = name, props, children
    def find(self, name):
        return [c for c in self.children if c.name == name]
    def first(self, name):
        r = self.find(name)
        return r[0] if r else None

def read_props(buf, pos, count):
    props = []
    for _ in range(count):
        t = chr(buf[pos]); pos += 1
        if t == 'Y': props.append(struct.unpack_from('<h', buf, pos)[0]); pos += 2
        elif t == 'C': props.append(bool(buf[pos])); pos += 1
        elif t == 'I': props.append(struct.unpack_from('<i', buf, pos)[0]); pos += 4
        elif t == 'F': props.append(struct.unpack_from('<f', buf, pos)[0]); pos += 4
        elif t == 'D': props.append(struct.unpack_from('<d', buf, pos)[0]); pos += 8
        elif t == 'L': props.append(struct.unpack_from('<q', buf, pos)[0]); pos += 8
        elif t in 'fdlib':
            n, enc, clen = struct.unpack_from('<III', buf, pos); pos += 12
            raw = buf[pos:pos + clen]; pos += clen
            if enc == 1: raw = zlib.decompress(raw)
            code = {'f': 'f', 'd': 'd', 'l': 'q', 'i': 'i', 'b': 'B'}[t]
            props.append(list(struct.unpack('<%d%s' % (n, code), raw)))
        elif t == 'S':
            n = struct.unpack_from('<I', buf, pos)[0]; pos += 4
            props.append(buf[pos:pos + n].decode('utf-8', 'replace')); pos += n
        elif t == 'R':
            n = struct.unpack_from('<I', buf, pos)[0]; pos += 4
            props.append(bytes(buf[pos:pos + n])); pos += n
        else:
            raise ValueError('prop type ' + t)
    return props, pos

def read_node(buf, pos, version):
    if version >= 7500:
        end, nprops, plen, nlen = struct.unpack_from('<QQQB', buf, pos); pos += 25
    else:
        end, nprops, plen, nlen = struct.unpack_from('<IIIB', buf, pos); pos += 13
    if end == 0:
        return None, pos
    name = buf[pos:pos + nlen].decode('ascii'); pos += nlen
    props, pos = read_props(buf, pos, nprops)
    children = []
    while pos < end:
        child, pos = read_node(buf, pos, version)
        if child is None:
            break
        children.append(child)
    return Node(name, props, children), end

def parse(path):
    buf = open(path, 'rb').read()
    version = struct.unpack_from('<I', buf, 23)[0]
    pos = 27
    root = Node('', [], [])
    while pos < len(buf) - 1:
        node, pos = read_node(buf, pos, version)
        if node is None:
            break
        root.children.append(node)
    return root, version
