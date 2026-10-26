"""从人机对战页 ../public/ai.html 抽出战斗引擎，生成 public/engine.js。

引擎 = ai.html 第2个<script>里 "界面与对局流程" 标记之前的全部(内置招式表、常量、Action、Cultivator、Controller、parseActions ...)。
只读 ai.html，不改它。ai.html 里的规则/招式表改了之后，重新运行一次本脚本，联机版就跟着同步：
    python tools/build_engine.py
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, '..', 'public', 'ai.html')
OUT = os.path.join(HERE, '..', 'public', 'engine.js')
MARK = '/* ============ 界面与对局流程'


def main():
    text = open(SRC, encoding='utf-8').read()
    # 第2个<script>块：以 "use strict" 开头(第1个块是AI推理)
    m = re.search(r'<script>\s*\n("use strict";\n/\* ============ 内置数据.*?)' + re.escape(MARK), text, re.S)
    if not m:
        sys.exit('没有找到离线版的引擎脚本块(标记变了？)')
    body = m.group(1).rstrip() + '\n'
    head = ('/* 自动生成，请勿手改：python tools/build_engine.py 从 public/ai.html 抽取。\n'
            '   离线版的战斗规则 + 默认招式表(xiaohua.txt)。联机版两边浏览器各用同一份引擎、同一个随机种子结算。 */\n')
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    open(OUT, 'w', encoding='utf-8', newline='\n').write(head + body)
    print('written', OUT, len(body), 'bytes')


if __name__ == '__main__':
    main()
