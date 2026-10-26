"""生成 public/index.html：部署后站点根目录要有 index.html，而入口页的源文件放在 Online/index.html(本地双击就从它进)，
它的链接带 public/ 前缀(本地打开用)；站点里 public/ 就是根，所以这里把 "public/" 前缀去掉再写出来。
    python tools/build_index.py      (npm run build 会自动跑)
"""
import os

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, '..', 'index.html')
OUT = os.path.join(HERE, '..', 'public', 'index.html')


def main():
    s = open(SRC, encoding='utf-8', newline='').read()
    n = s.count('href="public/')
    if not n:
        raise SystemExit('Online/index.html 里没有 href="public/..." 链接？')
    s = s.replace('href="public/', 'href="')
    open(OUT, 'w', encoding='utf-8', newline='').write(s)
    print('written', os.path.normpath(OUT), n, 'links')


if __name__ == '__main__':
    main()
