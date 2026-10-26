"""把 Resource/act/xiaohua.txt 同步进 wxdj_simulator.html 内置的 BUILTIN_ACTIONS(网页自带的招式表是拷贝，
改了xiaohua.txt之后要跑一次，否则网页里还是旧数值)。用法：python sync_actions.py"""
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
ACT = os.path.join(HERE, '..', 'Resource', 'act', 'xiaohua.txt')
HTML = os.path.join(HERE, 'wxdj_simulator.html')

text = open(ACT, encoding='utf-8').read().replace('\r\n', '\n')
assert '`' not in text and '${' not in text
h = open(HTML, encoding='utf-8', newline='').read()
crlf = '\r\n' in h
h = h.replace('\r\n', '\n')
pat = re.compile(r'(const BUILTIN_ACTIONS = String\.raw`)(.*?)(`;)', re.S)
assert pat.search(h)
h = pat.sub(lambda m: m.group(1) + text + m.group(3), h, count=1)
open(HTML, 'w', encoding='utf-8', newline='').write(h.replace('\n', '\r\n') if crlf else h)
print('synced', len(text.split('\n')), 'lines')
