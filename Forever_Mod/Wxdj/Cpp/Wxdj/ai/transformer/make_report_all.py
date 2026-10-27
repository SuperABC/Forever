"""把各式的评测报告(report_<式>.txt)汇总成 report_all.md：各式概览 + 每个家族的单招/双招使用率和去掉后胜率(一格里依次是各式的值)。
用法：python make_report_all.py [report目录，默认当前目录]    评测脚本(eval_only/train_pvp)结束时会自动调用
"""
import glob
import os
import re
import sys


def parse(path):
    """返回 (概览dict, {家族: (单招%, 双招%, 去掉后胜率)}, 家族顺序)"""
    overview = None
    fams = {}
    order = []
    for line in open(path, encoding='utf-8-sig'):
        line = line.strip()
        m = re.match(r'^自由对局 胜/负/平 ([\d.]+)/([\d.]+)/([\d.]+)\s+招类使用率\(.*?\)\s*\[(.*?)\]', line)
        if m:
            rates = [float(x) for x in m.group(4).split()]
            overview = dict(win=m.group(1), lose=m.group(2), draw=m.group(3), rates=rates)
            continue
        m = re.match(r'^(\S+)\s*\|\s*([\d.]+)%\s+([\d.]+)%\s+([\d.]+)$', line)
        if m:
            fams[m.group(1)] = (m.group(2), m.group(3), m.group(4))
            order.append(m.group(1))
    return overview, fams, order


def bold(text, on):
    return '**%s**' % text if on else text


def num(s):
    return ('%.1f' % float(s)).rstrip('0').rstrip('.')


def main():
    d = sys.argv[1] if len(sys.argv) > 1 else '.'
    files = {}
    for p in glob.glob(os.path.join(d, 'report_*.txt')):
        m = re.match(r'report_(\d+)\.txt$', os.path.basename(p))
        if m:
            files[int(m.group(1))] = p
    forms = sorted(files)
    if not forms:
        sys.exit('没有找到 report_<式>.txt')
    parsed = {f: parse(files[f]) for f in forms}
    label = '/'.join(str(f) for f in forms)

    out = ['# 招式强度评测汇总', '',
           '数据来自各式的 report_<式>.txt(自由对局，镜像对局)。每个式每个家族的样本有限，去掉后胜率的误差约±10%；'
           '15式以后平局(同归于尽)多，胜率整体偏低，不要和低式直接比。', '',
           '加粗：单招/双招使用率 <2% 或 >20%，去掉后胜率 <0.30。', '',
           '## 各式概览(自由对局)', '',
           '| 式 | 胜/负/平 | 跳过 | 积攒 | 炸弹 | 单招 | 双招 |', '|---|---|---|---|---|---|---|']
    for f in forms:
        ov = parsed[f][0]
        if not ov:
            continue
        r = ov['rates'] + [0] * (5 - len(ov['rates']))
        out.append('| %d | %s/%s/%s | %.1f%% | %.1f%% | %.1f%% | %.1f%% | %.1f%% |' % (
            f, ov['win'], ov['lose'], ov['draw'], *(100 * x for x in r[:5])))

    # 家族顺序：以第一个式的报告顺序为准，其余式里多出来的追加
    order = []
    for f in forms:
        for n in parsed[f][2]:
            if n not in order:
                order.append(n)
    out += ['', '## 各家族(一格里依次是第 %s 式的值)' % label, '',
            '| 家族 | 单招使用率% | 双招成员使用率% | 去掉后胜率 |', '|---|---|---|---|']
    for n in order:
        cells = [[], [], []]
        for f in forms:
            v = parsed[f][1].get(n)
            for k in range(3):
                if v is None:
                    cells[k].append('-')
                elif k < 2:
                    x = float(v[k])
                    cells[k].append(bold(num(v[k]), x < 2 or x > 20))   # 使用率<2%或>20%加粗
                else:
                    cells[k].append(bold(v[k], float(v[k]) < 0.30))     # 去掉后胜率<30%加粗
        out.append('| %s | %s |' % (n, ' | '.join('/'.join(c) for c in cells)))
    out.append('')
    path = os.path.join(d, 'report_all.md')
    open(path, 'w', encoding='utf-8', newline='\n').write('\n'.join(out))
    print('wrote', os.path.abspath(path))


if __name__ == '__main__':
    main()
