"""由 complete.txt 里"无属性"的14个家族(调数值就改这里)重新生成两张招式表：

  1. complete.txt 里其余6种属性(金/木/水/火/土/五行)的家族：消耗和效果参数照抄无属性同效果同式，威力 = 无属性同式威力 x 属性系数(单属性0.7、五行0.5)；
  2. xiaohua.txt(游戏和网页模拟器用的表)：19个有实际名字的招式家族(千腿、金钟罩 ...)，每个家族只是 complete.txt 里一个 属性+效果 家族换了个实际的招式名，
     消耗、威力、效果参数都是按 属性+效果 直接在 complete.txt 里查表填上，不做任何额外计算
     (如伏羲九针=金属性回复健康，灵兰生息=木属性回复武力，砍柴功和木森林变换都是木属性无效果，数值相同)。

用法(Train 下)：python tools/gen_tables.py       改了 complete.txt 里无属性家族的数值后跑一次；两张表的换行格式(CRLF)保持不变。
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ACT = os.path.join(HERE, '..', '..', 'Cpp', 'act')
COMPLETE = os.path.join(ACT, 'complete.txt')
XIAOHUA = os.path.join(ACT, 'xiaohua.txt')

ATTRS = [('无', 'none', 1.0), ('金', 'metal', 0.7), ('木', 'wood', 0.7), ('水', 'water', 0.7), ('火', 'fire', 0.7), ('土', 'earth', 0.7), ('五行', 'all', 0.5)]
FACTOR = {a: f for _, a, f in ATTRS}
CN = {a: n for n, a, _ in ATTRS}

# 实际招式名 -> (属性, [无属性效果家族名...])；效果家族空表示无效果
MOVES = [
    ('无招', 'none', ['无效果']),
    ('千腿', 'none', ['穿透']),
    ('开天辟地爪', 'none', ['物理']),
    ('逆天神功', 'none', ['禁锢']),
    ('铁手功', 'metal', ['无效果']),
    ('伏羲九针', 'metal', ['回复健康']),
    ('金钟罩', 'metal', ['反弹']),
    ('灵兰生息', 'wood', ['回复武力']),
    ('砍柴功', 'wood', ['无效果']),
    ('木森林变换', 'wood', ['无效果']),
    ('海龙金身', 'water', ['减伤']),
    ('天降春雨', 'water', ['回复真气']),
    ('狂火拳', 'fire', ['持续伤害']),
    ('火之高兴', 'fire', ['连招增伤']),
    ('地狱阴魔拳', 'earth', ['禁锢']),
    ('安如磐石', 'earth', ['护盾']),
    ('天动万象', 'earth', ['静止增伤']),
    ('吸蜂神功', 'all', ['吸收']),
    ('五行八卦掌', 'all', ['无效果']),
]
SUFFIX = ['一', '二', '三', '四', '五', '六', '七', '八', '九', '十', '十一', '十二', '十三', '十四', '十五', '十六', '十七', '十八', '十九', '二十']


def read_lines(p):
    return open(p, encoding='utf-8', newline='').read().split('\r\n')


def parse_none(lines):
    """complete.txt 里无属性的家族：{效果名: [(消耗, 威力, 阶位, [效果行])]*20}。"""
    fams, i = {}, 0
    while i < len(lines):
        t = lines[i].split(' ')
        if len(t) == 6 and re.match(r'^\d+-\d+$', t[4]) and t[0].startswith('无属性'):
            fam = t[0][3:].split('第')[0]
            n = int(t[5])
            fams.setdefault(fam, []).append((int(t[2]), int(t[3]), t[4], lines[i + 1:i + 1 + n]))
            i += 1 + n
        else:
            i += 1
    assert len(fams) == 14 and all(len(v) == 20 for v in fams.values()), {k: len(v) for k, v in fams.items()}
    return fams


def header_of(lines):
    """complete.txt 开头到阶位三维表结束(含那行 # ====)。"""
    marks = [i for i, l in enumerate(lines) if l.startswith('# ====')]
    return lines[:marks[2] + 1]


def build_complete(head, fams):
    out = list(head)
    for an, attr, f in ATTRS:
        for fam, forms in fams.items():
            out += ['', '# %s属性%s' % (an, fam)]
            for k, (c, p, realm, eff) in enumerate(forms):
                out.append('%s属性%s第%s式 %s %d %d %s %d' % (an, fam, SUFFIX[k], attr, c, round(p * f), realm, len(eff)))
                out += eff
    out.append('')
    return out


def parse_all(lines):
    """complete.txt 里的全部家族：{(属性, 效果名): [(消耗, 威力, 阶位, [效果行])]*20}，属性用 none/metal/... 。"""
    cn2attr = {n + '属性': a for n, a, _ in ATTRS}
    fams, i = {}, 0
    while i < len(lines):
        t = lines[i].split(' ')
        if len(t) == 6 and re.match(r'^\d+-\d+$', t[4]):
            head, rest = lines[i].split(' ')[0].split('第')[0], None
            attr_cn = next(c for c in cn2attr if head.startswith(c))
            fams.setdefault((cn2attr[attr_cn], head[len(attr_cn):]), []).append((int(t[2]), int(t[3]), t[4], lines[i + 1:i + 1 + int(t[5])]))
            i += 1 + int(t[5])
        else:
            i += 1
    return fams


def build_xiaohua(all_fams):
    out = ['# xiaohua.txt：游戏和网页模拟器用的招式表，19个有实际名字的招式家族 x 20式 = 380 个招式。',
           '# 每个家族都是 complete.txt 里对应 属性+效果 的家族换成实际招式名，消耗/威力/效果参数按 属性+效果 直接查表填入，由 tools/gen_tables.py 生成',
           '# (改数值请改 complete.txt 里的无属性家族后重新生成，别手改这张表)。威力已含属性系数(单属性0.7、五行0.5)。',
           '# 阶位标准三维见 complete.txt 开头的注释表。']
    for name, attr, effs in MOVES:
        fam = all_fams[(attr, effs[0])]
        out += ['', '# %s(%s属性，%s)' % (name, CN[attr], effs[0])]
        for k, (c, p, realm, eff) in enumerate(fam):
            out.append('%s第%s式 %s %d %d %s %d' % (name, SUFFIX[k], attr, c, p, realm, len(eff)))
            out += eff
    out.append('')
    return out


def main():
    lines = read_lines(COMPLETE)
    fams = parse_none(lines)
    head = header_of(lines)
    new_complete = build_complete(head, fams)
    new_xiaohua = build_xiaohua(parse_all(new_complete))      # 先全部算好再写文件，算错时不会把表写成空的
    open(COMPLETE, 'w', encoding='utf-8', newline='').write('\r\n'.join(new_complete))
    open(XIAOHUA, 'w', encoding='utf-8', newline='').write('\r\n'.join(new_xiaohua))
    print('complete.txt、xiaohua.txt 已重新生成')


if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8')
    main()
