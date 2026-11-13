"""scripts/ 里几个脚本共用的小函数：路径、子进程环境(招式表/选家族)、训练、权重平均、评测。"""
import os
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)                                              # Train
TRAIN_TABLE = os.path.join(ROOT, '..', 'Cpp', 'act', 'complete.txt')             # 训练用的招式表：用其中无属性的单效果家族当模板，家族属性随机抽(见 train/gen_pool.py)
STAGE_TABLE = os.path.join(ROOT, '..', 'Cpp', 'act', 'complete.txt')             # 各阶位标准三维表的来源(开头的注释表)
EVAL_TABLE = os.path.join(ROOT, '..', 'Cpp', 'act', 'complete.txt')               # 评测用的招式表：默认 complete.txt，不指定 --select 时混合抽样25个家族(每种效果至少1个、每种属性至少3个+余下随机)
BASE_MODEL = os.path.join(ROOT, 'models', 'base', 'base_avg.pt')          # 当前的底座模型
RESULTS = os.path.join(ROOT, 'results')                                   # 评测结果(每次覆盖)
RUNS = os.path.join(ROOT, 'runs')                                         # 训练输出(每个 --name 一个目录)


def default_model():
    """eval.py 不带模型参数时评测哪个：底座模型存在就用底座，否则用 runs/ 下最近训练出 final.pt 的那个。"""
    if os.path.exists(BASE_MODEL):
        return BASE_MODEL
    cands = [os.path.join(RUNS, d, 'final.pt') for d in os.listdir(RUNS)] if os.path.isdir(RUNS) else []
    cands = [p for p in cands if os.path.exists(p)]
    return max(cands, key=os.path.getmtime) if cands else BASE_MODEL

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8')                              # Windows下重定向时中文不乱码


_STAGE_ROW = re.compile(r'^#\s+\d-\d\s+\d+(?:-\d+)?\s+\d+\s+\d+\s+\d+\s*$')      # "# 4-6   18-20   25800   38900   24800"


def _stage_rows(text):
    return [l.rstrip('\r') for l in text.split('\n') if _STAGE_ROW.match(l.rstrip('\r'))]


def child_env(table, select=None, freeze_to=None):
    """子进程环境：WXDJ_ACT = 招式表(freeze_to 给了就先拷贝一份，之后你再改原表也不影响)，WXDJ_SELECT = 评测的家族子集(逗号分隔的名字片段)。"""
    table = os.path.abspath(table)
    if not os.path.exists(table):
        sys.exit('找不到招式表 ' + table)
    if freeze_to:
        os.makedirs(os.path.dirname(freeze_to), exist_ok=True)
        text = open(table, encoding='utf-8', newline='').read()
        if not _stage_rows(text):       # 这张表没有"各阶位标准三维"表(如 xiaohua.txt)：从 complete.txt 补上，三维始终以 complete.txt 里的那张为准
            rows = _stage_rows(open(os.path.abspath(STAGE_TABLE), encoding='utf-8', newline='').read())
            if rows:
                text = '# 各阶位标准三维(这张表没有，评测时从 complete.txt 补上的)\r\n' + '\r\n'.join(rows) + '\r\n' + text
        open(freeze_to, 'w', encoding='utf-8', newline='').write(text)
        table = freeze_to
    env = dict(os.environ, PYTHONIOENCODING='utf-8', WXDJ_ACT=table)
    env.pop('WXDJ_SELECT', None)
    if select and select != 'all':
        env['WXDJ_SELECT'] = select
    return env


def run_py(script, args, env, **kw):
    """跑 Train 下的一个python脚本(输出直接显示在终端)，返回子进程结果。"""
    return subprocess.run([sys.executable, '-u', '-W', 'ignore', os.path.join(ROOT, script)] + [str(a) for a in args], env=env, **kw)


def fresh_dir(path):
    """清空并重建一个目录；只允许动 Train 里的 runs/ 和 results/，防止误删别处。"""
    path = os.path.abspath(path)
    if os.path.commonpath([path, ROOT]) != ROOT or (os.path.basename(os.path.dirname(path)) != 'runs' and os.path.basename(path) != 'results'):
        sys.exit('拒绝清空 ' + path)
    shutil.rmtree(path, ignore_errors=True)
    os.makedirs(path)


def keep_awake():
    """训练期间不让Windows自动睡眠(只对本进程有效，退出即恢复)。"""
    try:
        import ctypes
        ctypes.windll.kernel32.SetThreadExecutionState(0x80000001)
    except Exception:
        pass


def train(run_dir, iters, table, extra, init=None, lr=None, seed=1, hours=0.0, resume=False):
    """训练到 run_dir(先清空)，结束后对最后的快照做权重平均得到 run_dir/final.pt。返回 final.pt 的路径。
    resume=True 且 run_dir 里已有 latest.pt：接着上次中断的地方训练，不清空目录，沿用目录里冻结的招式表；这时 iters 是在已训的基础上再训多少轮(自己读 latest.pt 里已训的轮数)，hours 是这一次的时间上限。"""
    keep_awake()
    frozen = os.path.join(run_dir, 'table_used.txt')
    if resume and os.path.exists(os.path.join(run_dir, 'latest.pt')) and os.path.exists(frozen):
        import torch
        done = int(torch.load(os.path.join(run_dir, 'latest.pt'), map_location='cpu', weights_only=False)['iter'])
        print('接着训练：%s，已训 %d 轮，再训 %d 轮，到第 %d 轮' % (run_dir, done, iters, done + iters))
        env = child_env(frozen)
        args = ['--ckpt_dir', run_dir, '--iters', 0, '--max_iters', done + iters, '--seed', seed, '--hours', hours, '--resume']
    else:
        fresh_dir(run_dir)
        env = child_env(table, freeze_to=frozen)
        args = ['--ckpt_dir', run_dir, '--iters', iters, '--seed', seed, '--hours', hours]
    if init:
        args += ['--init', os.path.abspath(init)]
    if lr:
        args += ['--lr', lr]
    r = run_py('train/train.py', args + list(extra), env)
    if r.returncode != 0:
        sys.exit('训练失败')
    final = os.path.join(run_dir, 'final.pt')
    r = run_py('train/avg_weights.py', [run_dir, '--out', final], env)
    if r.returncode != 0 or not os.path.exists(final):          # 快照太少没法平均：直接用最终的权重
        import torch
        ck = torch.load(os.path.join(run_dir, 'latest.pt'), map_location='cpu', weights_only=False)
        torch.save(ck['net'], final)
        print('快照不足，final.pt = 最终权重')
    print('训练完成：%s' % final)
    return final


def count_selected(env):
    """用同样的环境变量读一遍招式表，返回 (选中的家族数, 家族数上限, 各属性家族数的说明)。读失败就直接退出并显示原因。"""
    code = "import sys; sys.path.insert(0, sys.argv[1]); import gen_pool as g; print(len(g.REAL), g.MAX_FAMS); import collections; c = collections.Counter(f.forms[0]['attr'] for f in g.REAL); print('各属性家族数 ' + ' '.join('%s%d' % (n, c[a]) for n, a in zip(['无', '金', '木', '水', '火', '土', '五行'], g.ATTRS)))"
    r = subprocess.run([sys.executable, '-c', code, os.path.join(ROOT, 'train')], env=env, capture_output=True, text=True, encoding='utf-8', errors='replace')
    if r.returncode != 0:
        sys.exit('读招式表失败：\n' + r.stderr.strip()[-1500:])
    lines = r.stdout.strip().split('\n')
    n, mx = (int(x) for x in lines[0].split())
    return n, mx, (lines[1] if len(lines) > 1 else '')


def evaluate(model, table, select, games, forms):
    """评测 model 在最高式 forms 的5式窗口上的表现，结果写到 results/ (先清空，覆盖上次)，汇总成 results/report_all.md。"""
    model = os.path.abspath(model)
    if os.path.isdir(model):
        model = os.path.join(model, 'final.pt')
    if not os.path.exists(model):
        sys.exit('找不到模型 %s\n(eval.py 不带模型参数时默认评测 models/base/base_avg.pt；评测自己训的模型要带上路径，如 python scripts/eval.py runs/<名字>)' % model)
    fresh_dir(RESULTS)
    env = child_env(table, select, freeze_to=os.path.join(RESULTS, 'table_used.txt'))
    print('模型 %s\n招式表 %s\n最高式 %s，每个对局组合 %d 盘' % (model, os.path.abspath(table), forms, games), flush=True)
    # 先检查选中了几个家族：0个(名字片段写错了)或太多(超过环境容量)都直接说清楚，不要等到评测里才崩
    n, mx, names = count_selected(env)
    print('--select "%s" 选中 %d 个家族(上限 %d)，%s' % (select, n, mx, names), flush=True)
    if n == 0:
        sys.exit('--select "%s" 没有匹配到任何家族(家族名形如 火属性穿透、无属性无效果)' % select)
    if n > mx:
        sys.exit('选中的家族太多(%d > %d)，请用 --select 选个子集，如 无效果 / 火属性 / 穿透' % (n, mx))
    bad = []
    t_all = time.time()
    for f in forms:
        print('最高式%d 评测中 ...' % f, flush=True)
        t = time.time()
        r = subprocess.run([sys.executable, '-W', 'ignore', os.path.join(ROOT, 'eval', 'eval_report.py'), model, str(f), '--games', str(games)],
                           env=env, capture_output=True, text=True, encoding='utf-8', errors='replace')
        text = '\n'.join(l for l in r.stdout.split('\n') if 'Warning' not in l and 'torch.load' not in l)
        if r.returncode != 0:
            text += '\n[评测失败，返回码 %d，错误信息]\n%s\n' % (r.returncode, r.stderr.strip())    # 存进报告文件，事后还能查
        open(os.path.join(RESULTS, 'report_%d.txt' % f), 'w', encoding='utf-8').write(text)
        if r.returncode != 0:
            bad.append(f)
            print('最高式%d 失败，错误信息：\n%s' % (f, '\n'.join(r.stderr.strip().split('\n')[-12:])), flush=True)
        else:
            print('最高式%d 完成，%.1f 分钟' % (f, (time.time() - t) / 60), flush=True)
    if len(bad) == len(forms):
        sys.exit('全部失败，错误信息见上面')
    subprocess.run([sys.executable, os.path.join(ROOT, 'eval', 'make_report_all.py'), RESULTS], env=env)
    print('评测完成，用时 %.1f 分钟，汇总：%s' % ((time.time() - t_all) / 60, os.path.join(RESULTS, 'report_all.md')))
