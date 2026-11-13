"""把 Cpp/act/complete.txt 开头注释里的"各阶位标准三维"(健康/武力/真气)同步进 public/ai.html(人机对战页，也就是模拟器)：
  1. /*STAGE_STATS_BEGIN*/ ... /*STAGE_STATS_END*/ 之间的 STAGE_STATS 表(没有这对标记就插到"标准属性"那段注释前面)；
  2. const STANDARD 里的 hp / atk / mp(取最后一行，也就是 4-6 阶位)。
网页里的标准属性 standardForForm(form) 以这张表为准；每回合回复真气、敏捷、炸弹倍率仍按原来的公式。
用法：python tools/sync_stages.py [招式表]     默认 Cpp/act/complete.txt；改了表里的三维之后跑一次(再 python tools/build_engine.py 更新联机引擎)。
表的格式：每行以"# "开头，依次是 阶位 包含的式 健康 武力 真气，如  "# 4-6   18-20   25800   38900   24800"。
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ACT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', '..', 'Cpp', 'act', 'complete.txt')
HTML = os.path.join(HERE, '..', 'public', 'ai.html')
BEGIN, END = '/*STAGE_STATS_BEGIN*/', '/*STAGE_STATS_END*/'

rows = []
for line in open(ACT, encoding='utf-8').read().replace('\r\n', '\n').split('\n'):
    m = re.match(r'^#\s+(\d-\d)\s+(\d+)(?:-(\d+))?\s+(\d+)\s+(\d+)\s+(\d+)\s*$', line)
    if m:
        lo = int(m.group(2))
        rows.append((m.group(1), lo, int(m.group(3) or lo), int(m.group(4)), int(m.group(5)), int(m.group(6))))
covered = sorted(f for _, lo, hi, *_ in rows for f in range(lo, hi + 1))
if covered != list(range(1, 21)):
    sys.exit('阶位三维表没有恰好覆盖第1~20式(每式一行且不重叠)：%s' % rows)

raw = open(HTML, encoding='utf-8', newline='').read()
crlf = '\r\n' in raw
h = raw.replace('\r\n', '\n')

block = '\n'.join([
    BEGIN,
    '// 各阶位的标准三维(健康/武力/真气)：来自 Cpp/act/complete.txt 开头的注释表，由 tools/sync_stages.py 同步，别手改',
    'const STAGE_STATS = [',
] + ['  { lo: %d, hi: %d, hp: %d, atk: %d, mp: %d },   // %s' % (lo, hi, hp, atk, mp, realm) for realm, lo, hi, hp, atk, mp in rows] + [
    '];',
    END,
])
pat = re.compile(re.escape(BEGIN) + r'.*?' + re.escape(END), re.S)
if pat.search(h):
    h = pat.sub(lambda m: block, h, count=1)
else:
    anchor = '/* ---- 标准属性：按选定的式所在阶位算'
    assert h.count(anchor) == 1, '找不到插入位置'
    h = h.replace(anchor, block + '\n\n' + anchor, 1)

last = rows[-1]
std = re.compile(r'(const STANDARD = \{ hp: )\d+(, atk: )\d+(, mp: )\d+(,)')
assert std.search(h), '找不到 const STANDARD'
h = std.sub(lambda m: '%s%d%s%d%s%d%s' % (m.group(1), last[3], m.group(2), last[4], m.group(3), last[5], m.group(4)), h, count=1)

open(HTML, 'w', encoding='utf-8', newline='').write(h.replace('\n', '\r\n') if crlf else h)
print('已同步 %d 个阶位到 ai.html；STANDARD = 健康%d 武力%d 真气%d(%s)' % (len(rows), last[3], last[4], last[5], last[0]))
for realm, lo, hi, hp, atk, mp in rows:
    print('  %s 式%d-%d 健康%d 武力%d 真气%d' % (realm, lo, hi, hp, atk, mp))
