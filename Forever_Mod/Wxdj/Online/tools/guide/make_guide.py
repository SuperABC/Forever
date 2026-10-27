"""生成说明书 public/guide.html：python tools/guide/make_guide.py
招式数值表、家族卡片、规则文案都在这个脚本里(HTML 模板)；末尾的动画示范在同目录 demo_section.html。
改完直接重跑即可，生成结果就是联机站点和本地打开用的那份说明书。"""
import math

import os
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', '..', 'public', 'guide.html')   # 说明书(联机站点和本地打开用同一个文件)

ATTR = {
    'none': ('无属性', '#9a9aaa'), 'metal': ('金', '#c9c9d6'), 'wood': ('木', '#3fa653'), 'water': ('水', '#3a7be0'),
    'fire': ('火', '#e0562e'), 'earth': ('土', '#b88a3e'), 'all': ('五行', '#a455d6'),
}

FAMILIES = [
    # 名字, 属性, 一句话定位, 效果图标, 效果说明, 玩法提示
    ('无招', 'none', '最朴素的一拳', '—', '没有任何效果', '数值的基准，纯拼威力。被五个元素克制，单打吃亏。'),
    ('千腿', 'none', '连环快腿', '穿', '穿透：对撞时额外打出较小威力的一部分（31.5%→60%）', '双方威力越大，额外伤害越高；对手出的招很小时，额外伤害也很小。'),
    ('开天辟地爪', 'none', '重爪破体', '物', '物理：一部分伤害直接扣健康值（26%→50%）', '专打"武力很高但健康见底"的对手。'),
    ('逆天神功', 'none', '封人经脉', '锁', '禁锢：概率让对手下回合无法出招（42%→80%）', '低式也有不小的禁锢概率，高阶非常可怕。'),
    ('铁手功', 'metal', '实打实的硬功', '—', '没有效果', '纯粹的输出招式，和同属性对手互不克制。'),
    ('伏羲九针', 'metal', '针灸续命', '疗', '回复武力 + 回复健康（立即一份，之后每隔几回合一份）', '拖得越久越赚，尤其适合当双招的搭档。'),
    ('金钟罩', 'metal', '以守为攻', '弹', '反弹：把受到的伤害按比例还给攻击者，自己只承受剩下的（21%→40%）', '反弹比例越高越不怕硬拼，是减伤的升级版。'),
    ('砍柴功', 'wood', '平平无奇的樵夫', '—', '没有效果', '数值朴素，主要用来凑双招。'),
    ('木森林变换', 'wood', '万木齐发', '—', '没有效果，但威力极高、消耗也极高', '单发最猛的招式之一，注意透支。'),
    ('海龙金身', 'water', '龙鳞护体', '盾', '减伤：受到的伤害减少一定比例（42%→80%）', '防御招式本身输出不高，用来扛住对手的大招。'),
    ('天降春雨', 'water', '春雨润心', '雨', '回复真气：立即一份，之后每隔几回合一份', '让你出得起更多的大招，像一个"真气银行"。'),
    ('狂火拳', 'fire', '烈焰灼身', '焰', '持续伤害：命中后立即一份，之后每隔几回合再烧一次', '持续伤害不受对撞胜负影响，越早命中越赚。'),
    ('火之高兴', 'fire', '星火成燎原', '连', '连招增伤：连着上一回合同家族的前一式出招，每连续一步威力多一截（21%→40%）', '消耗和威力与同式的无招相同，全靠连招把威力越叠越高：从第 1 式一路连到第 20 式，最后一招的威力约是原来的 8.6 倍。中间只要积攒、跳过、被禁锢就断了，要从头再来。'),
    ('地狱阴魔拳', 'earth', '阴魔索命', '锁', '禁锢：概率让对手下回合无法出招（31.5%→60%）', '和逆天神功类似，但属性是土。'),
    ('安如磐石', 'earth', '厚土筑壁', '护', '护盾：出招时给自己加一块盾，盾量是自己武力上限的一定比例（10.5%→20%），持续 2~3 回合（3-1~3-6 阶位的第 1~6 式是 2 回合，其余 3 回合）', '消耗高、自身威力低，是专门用来开盾的招式。盾同时挡武力和健康的伤害，伤害先打在盾上，盾被打光之后才扣武力/健康。'),
    ('天动万象', 'earth', '以静制动', '静', '静止增伤：对手这回合积攒炸弹或释放空炸弹（没攒过就放）时，这一招的伤害多一截（22%→40%，算在增伤乘区里；跳过不算静止）', '消耗是同式无招的 1.1 倍、威力是 1.05 倍，平时和普通招式差不多，专门克制“第一回合先攒炸弹”：看穿对手要攒炸弹就出它。但如果对手出的是别的招，它就只是一个平庸的招式。'),
    ('吸蜂神功', 'all', '蜂群吸元', '吸', '吸收：把造成伤害的一部分转化成自己的真气（105%→200%）', '真气不够用时，它能边打边补；吸收比例很高时，当心真气超过上限的 2 倍会爆体。'),
    ('五行八卦掌', 'all', '五行合一', '—', '没有效果，威力很高', '五行属性克制全部五个元素，不被任何属性克制。'),
]


def attr_chip(a):
    n, c = ATTR[a]
    return f'<span class="chip" style="--c:{c}">{n}</span>'


def family_cards():
    out = []
    for name, a, tag, icon, eff, tip in FAMILIES:
        n, c = ATTR[a]
        out.append(f'''<article class="fam" style="--c:{c}">
  <div class="fam-top"><span class="fam-icon">{icon}</span><div><h4>{name}</h4><div class="fam-tag">{tag}</div></div>{attr_chip(a)}</div>
  <p class="fam-eff"><b>效果</b>{eff}</p>
  <p class="fam-tip"><b>玩法</b>{tip}</p>
</article>''')
    return '\n'.join(out)


def wheel_svg():
    cx, cy, r = 190, 165, 115
    names = [('金', 'metal'), ('木', 'wood'), ('水', 'water'), ('火', 'fire'), ('土', 'earth')]
    pos = {}
    for i, (n, key) in enumerate(names):
        ang = -math.pi / 2 + i * 2 * math.pi / 5
        pos[n] = (cx + r * math.cos(ang), cy + r * math.sin(ang))
    edges = [('水', '火'), ('火', '木'), ('木', '水'), ('土', '金'), ('金', '木'), ('木', '土')]
    lines = []
    for a, b in edges:
        x1, y1 = pos[a]; x2, y2 = pos[b]
        dx, dy = x2 - x1, y2 - y1
        d = math.hypot(dx, dy)
        sx, sy = x1 + dx / d * 30, y1 + dy / d * 30
        ex, ey = x2 - dx / d * 34, y2 - dy / d * 34
        lines.append(f'<line x1="{sx:.1f}" y1="{sy:.1f}" x2="{ex:.1f}" y2="{ey:.1f}" stroke="var(--ink-soft)" stroke-width="2.4" marker-end="url(#arr)"/>')
    nodes = []
    for (n, key) in names:
        x, y = pos[n]
        col = ATTR[key][1]
        nodes.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="27" fill="{col}" stroke="var(--panel)" stroke-width="3"/>'
                     f'<text x="{x:.1f}" y="{y + 7:.1f}" text-anchor="middle" font-size="22" font-weight="700" fill="#101018">{n}</text>')
    return f'''<svg viewBox="0 0 380 336" class="svg-block" role="img" aria-label="五行相克图">
<defs><marker id="arr" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M0 0 L10 5 L0 10 z" fill="var(--ink-soft)"/></marker></defs>
{''.join(lines)}
{''.join(nodes)}
<text x="190" y="322" text-anchor="middle" font-size="13" fill="var(--ink-soft)">箭头 = "克制"，箭头指向被克的一方</text>
</svg>'''


def bar(label, color, pct, val):
    return f'''<div class="rbar"><span class="rl">{label}</span><div class="rtrack"><div class="rfill" style="width:{pct}%;background:{color}"></div></div><span class="rv">{val}</span></div>'''


PVE = open(os.path.join(HERE, 'pve_section.html'), encoding='utf-8').read()   # PvE 页：比 PvP 多出来的机制
DEMO = open(os.path.join(HERE, 'demo_section.html'), encoding='utf-8').read()   # 末尾的动画示范(样式+HTML+脚本)

HTML = f'''<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>五行斗诀 新手入门指南</title>
<style>
  :root {{
    --bg: #12121a; --panel: #1c1c27; --panel2: #232333; --line: #3a3a50; --ink: #ececf4; --ink-soft: #a9a9bd;
    --gold: #f0c860; --red: #e24b4b; --yellow: #e0b038; --blue: #4a8ff0; --green: #3fa653; --purple: #a455d6;
    --radius: 14px;
  }}
  @media (prefers-color-scheme: light) {{
    :root {{ --bg: #f3f1ea; --panel: #ffffff; --panel2: #f0eee6; --line: #d9d5c6; --ink: #1d1d28; --ink-soft: #5a5a6c; --gold: #b07d12; }}
  }}
  * {{ box-sizing: border-box; }}
  html {{ scroll-behavior: smooth; }}
  body {{ margin: 0; background: var(--bg); color: var(--ink); font: 16px/1.75 "Microsoft YaHei", "PingFang SC", "Noto Sans SC", sans-serif; }}
  a {{ color: var(--gold); }}
  .wrap {{ max-width: 980px; margin: 0 auto; padding: 0 20px 80px; }}
  header.hero {{ background: radial-gradient(1200px 400px at 50% -20%, #3a2f6e55, transparent), linear-gradient(180deg, #1a1a2c, var(--bg)); padding: 56px 20px 30px; text-align: center; border-bottom: 1px solid var(--line); }}
  header.hero h1 {{ font-size: clamp(34px, 6vw, 56px); margin: 0 0 6px; letter-spacing: .18em; }}
  header.hero .sub {{ color: var(--ink-soft); font-size: 18px; margin-bottom: 26px; }}
  .hero svg {{ width: min(760px, 100%); height: auto; }}
  nav.toc {{ position: sticky; top: 0; z-index: 10; background: color-mix(in srgb, var(--bg) 88%, transparent); backdrop-filter: blur(8px); border-bottom: 1px solid var(--line); }}
  nav.toc .in {{ max-width: 1040px; margin: 0 auto; padding: 8px 16px; display: flex; gap: 6px 8px; flex-wrap: wrap; font-size: 14px; }}
  nav.toc a {{ text-decoration: none; color: var(--ink-soft); padding: 2px 6px; border-radius: 6px; }}
  nav.toc a:hover {{ color: var(--ink); background: var(--panel2); }}
  section {{ margin-top: 54px; scroll-margin-top: 104px; }}
  /* PvP / PvE 切换 */
  nav.toc .tabs {{ padding-bottom: 0; gap: 8px; padding-left: 130px; }}   /* 让出左上角悬浮的"返回主界面"按钮 */
  .tabbtn {{ font: inherit; font-size: 15px; font-weight: 700; cursor: pointer; border: 1px solid var(--line); border-bottom: 0; background: var(--panel); color: var(--ink-soft); padding: 5px 18px; border-radius: 10px 10px 0 0; }}
  .tabbtn:hover {{ color: var(--ink); }}
  .tabbtn[aria-selected="true"] {{ background: var(--gold); color: #1b1608; border-color: var(--gold); }}
  .pane[hidden], nav.toc .links[hidden] {{ display: none; }}
  nav.toc .links {{ border-top: 1px solid var(--line); }}
  h2 {{ font-size: 28px; margin: 0 0 6px; display: flex; align-items: center; gap: 12px; }}
  h2 .no {{ display: inline-grid; place-items: center; width: 38px; height: 38px; border-radius: 50%; background: var(--gold); color: #1b1608; font-size: 20px; font-weight: 800; }}
  h3 {{ font-size: 20px; margin: 26px 0 8px; }}
  h4 {{ margin: 0; font-size: 18px; }}
  p {{ margin: 8px 0; }}
  .lead {{ color: var(--ink-soft); }}
  .card {{ background: var(--panel); border: 1px solid var(--line); border-radius: var(--radius); padding: 18px 20px; }}
  .grid {{ display: grid; gap: 14px; }}
  .g2 {{ grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }}
  .g3 {{ grid-template-columns: repeat(auto-fit, minmax(240px, 1fr)); }}
  .g4 {{ grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); }}
  .note {{ border-left: 4px solid var(--gold); background: var(--panel); padding: 10px 16px; border-radius: 0 10px 10px 0; margin: 14px 0; }}
  .warn {{ border-left-color: var(--red); }}
  .tip {{ border-left-color: var(--green); }}
  .svg-block {{ width: 100%; height: auto; display: block; }}
  .chip {{ display: inline-block; padding: 0 9px; border-radius: 999px; background: var(--c); color: #12121a; font-size: 13px; font-weight: 700; line-height: 22px; white-space: nowrap; }}
  table {{ width: 100%; border-collapse: collapse; margin: 10px 0; font-size: 15px; }}
  th, td {{ border: 1px solid var(--line); padding: 7px 10px; text-align: left; vertical-align: top; }}
  th {{ background: var(--panel2); }}
  td.c, th.c {{ text-align: center; }}
  code, .kbd {{ background: var(--panel2); border: 1px solid var(--line); border-radius: 6px; padding: 1px 7px; font-size: 14px; }}
  .big {{ font-size: 40px; line-height: 1; }}
  /* 一分钟看懂 */
  .quick {{ counter-reset: q; display: grid; gap: 12px; }}
  .quick li {{ list-style: none; display: grid; grid-template-columns: 44px 1fr; gap: 12px; align-items: start; background: var(--panel); border: 1px solid var(--line); border-radius: var(--radius); padding: 14px 16px; }}
  .quick li::before {{ counter-increment: q; content: counter(q); display: grid; place-items: center; width: 40px; height: 40px; border-radius: 12px; background: var(--gold); color: #1b1608; font-weight: 800; font-size: 20px; }}
  .quick {{ padding: 0; margin: 14px 0 0; }}
  /* 资源条 */
  .rbar {{ display: grid; grid-template-columns: 56px 1fr 70px; gap: 8px; align-items: center; margin: 6px 0; }}
  .rl {{ font-weight: 700; }}
  .rtrack {{ height: 16px; background: #00000040; border-radius: 8px; overflow: hidden; border: 1px solid var(--line); }}
  .rfill {{ height: 100%; border-radius: 8px; }}
  .rv {{ font-size: 13px; color: var(--ink-soft); text-align: right; }}
  /* 界面示意 */
  .ui {{ position: relative; background: #12121a; color: #ececf4; border: 1px solid #3a3a50; border-radius: 12px; padding: 12px; font-size: 13px; }}
  .ui .row {{ display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }}
  .ui .st {{ padding: 6px; }}
  .ui .body {{ display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-top: 10px; }}
  .ui .log {{ position: relative; overflow: visible; background: #1c1c24; border: 1px solid #464655; border-radius: 6px; padding: 8px; height: 190px; font-size: 12px; line-height: 1.5;  }}
  .ui .log .h {{ color: #e6be5a; margin: 4px 0; }}
  .ui .btns {{ display: grid; gap: 6px; }}
  .ui .btns .r2 {{ display: grid; grid-template-columns: 1fr 1fr; gap: 6px; }}
  .ui .btns .r3 {{ display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 6px; }}
  .ui .b {{ background: #465a82; color: #fff; padding: 6px; text-align: center; border-radius: 3px; border: 1px solid #0f0f14; }}
  .ui .b.sel {{ outline: 2px solid #ffe678; outline-offset: -2px; }}
  .ui .b.acc {{ background: #8c6428; }} .ui .b.bomb {{ background: #aa3c32; }} .ui .b.skip {{ background: #5a5a64; }}
  .ui .slider {{ background: #2a2a36; padding: 4px 8px; display: flex; gap: 10px; align-items: center; }}
  .ui .slider .t {{ flex: 1; height: 4px; background: #5a5a6e; position: relative; border-radius: 2px; }}
  .ui .slider .t i {{ position: absolute; right: 0; top: -6px; width: 8px; height: 16px; background: #ffe68c; border: 1px solid #0f0f14; }}
  .ui .mfam {{ display: flex; background: #3c3c48; border: 1px solid #0f0f14; margin-top: 2px; padding: 3px 8px 3px 0; align-items: center; }}
  .ui .mfam .s {{ width: 6px; align-self: stretch; margin-right: 8px; }}
  .ui .mfam .n {{ flex: 1; }}
  .ui .hint {{ color: #e6be5a; }}
  .ui .env {{ margin-top: 8px; }}
  .ui .env span.k {{ padding: 2px 10px; background: #c85028; color: #fff; }}
  .ui .rel {{ position: relative; }}
  .callout {{ position: absolute; left: -10px; top: -10px; z-index: 2; background: var(--gold); color: #1b1608; width: 26px; height: 26px; border-radius: 50%; display: grid; place-items: center; font-weight: 800; font-size: 14px; box-shadow: 0 0 0 3px #12121a; }}
  ol.legend {{ columns: 2; column-gap: 28px; padding-left: 20px; }}
  ol.legend li {{ margin: 4px 0; break-inside: avoid; }}
  /* 家族 */
  .fam {{ background: var(--panel); border: 1px solid var(--line); border-top: 5px solid var(--c); border-radius: var(--radius); padding: 14px 16px; }}
  .fam-top {{ display: grid; grid-template-columns: 44px 1fr auto; gap: 10px; align-items: center; }}
  .fam-icon {{ display: grid; place-items: center; width: 44px; height: 44px; border-radius: 12px; background: var(--c); color: #12121a; font-weight: 800; font-size: 22px; }}
  .fam-tag {{ color: var(--ink-soft); font-size: 13px; }}
  .fam p {{ margin: 6px 0 0; font-size: 14px; line-height: 1.6; }}
  .fam b {{ display: inline-block; margin-right: 8px; padding: 0 7px; border-radius: 6px; background: var(--panel2); font-size: 12px; color: var(--ink-soft); }}
  /* 步骤 */
  .steps {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(210px, 1fr)); gap: 12px; }}
  .step {{ background: var(--panel); border: 1px solid var(--line); border-radius: var(--radius); padding: 14px 16px; position: relative; }}
  .step .n {{ position: absolute; top: -14px; left: 14px; background: var(--gold); color: #1b1608; border-radius: 999px; width: 28px; height: 28px; display: grid; place-items: center; font-weight: 800; }}
  .step h4 {{ margin-top: 6px; }}
  .kv {{ display: grid; grid-template-columns: auto 1fr; gap: 4px 12px; font-size: 15px; }}
  .kv dt {{ color: var(--ink-soft); }}
  .kv dd {{ margin: 0; }}
  footer {{ margin-top: 70px; padding-top: 20px; border-top: 1px solid var(--line); color: var(--ink-soft); font-size: 14px; }}
  @media (max-width: 640px) {{ section {{ scroll-margin-top: 150px; }} }}
  @media (max-width: 640px) {{ .ui .body {{ grid-template-columns: 1fr; }} ol.legend {{ columns: 1; }} }}

  /* ---- 联机站点：返回主界面 ---- */
  .wxdj-back {{ display: inline-block; text-decoration: none; cursor: pointer; border: 1px solid #0f0f14; background: #465a82; color: #fff;
    padding: 6px 12px; border-radius: 3px; font-size: 13px; line-height: 1.4; }}
  .wxdj-back:hover {{ background: #5a6f9a; }}
  .wxdj-back.float {{ position: fixed; left: 10px; top: 10px; z-index: 50; box-shadow: 0 2px 8px #0008; }}
</style>
</head>
<body>
<a class="wxdj-back float" href="../index.html">← 返回主界面</a>

<header class="hero">
  <h1>五行斗诀</h1>
  <div class="sub">一场关于「克制、资源、读心」的回合制对决 · 新手入门指南</div>
  <svg viewBox="0 0 760 250" role="img" aria-label="两名修炼者对决">
    <defs>
      <linearGradient id="glow" x1="0" x2="1"><stop offset="0" stop-color="#4a8ff0" stop-opacity=".0"/><stop offset=".5" stop-color="#f0c860" stop-opacity=".9"/><stop offset="1" stop-color="#e24b4b" stop-opacity=".0"/></linearGradient>
    </defs>
    <rect x="0" y="198" width="760" height="3" fill="url(#glow)"/>
    <!-- 左边的人 -->
    <g transform="translate(150,0)">
      <circle cx="0" cy="92" r="22" fill="#e9c9a0" stroke="#12121a" stroke-width="3"/>
      <path d="M-34 200 L-26 118 Q0 104 26 118 L34 200 Z" fill="#3a7be0" stroke="#12121a" stroke-width="3"/>
      <path d="M26 128 L78 98" stroke="#e9c9a0" stroke-width="12" stroke-linecap="round"/>
      <circle cx="86" cy="94" r="14" fill="#3a7be0" opacity=".9"/><circle cx="86" cy="94" r="22" fill="none" stroke="#3a7be0" stroke-width="3" opacity=".6"/>
      <text x="0" y="230" text-anchor="middle" font-size="16" fill="#a9a9bd">玩家</text>
    </g>
    <!-- 右边的人 -->
    <g transform="translate(610,0)">
      <circle cx="0" cy="92" r="22" fill="#e9c9a0" stroke="#12121a" stroke-width="3"/>
      <path d="M-34 200 L-26 118 Q0 104 26 118 L34 200 Z" fill="#e0562e" stroke="#12121a" stroke-width="3"/>
      <path d="M-26 128 L-78 98" stroke="#e9c9a0" stroke-width="12" stroke-linecap="round"/>
      <circle cx="-86" cy="94" r="14" fill="#e0562e" opacity=".9"/><circle cx="-86" cy="94" r="22" fill="none" stroke="#e0562e" stroke-width="3" opacity=".6"/>
      <text x="0" y="230" text-anchor="middle" font-size="16" fill="#a9a9bd">对手</text>
    </g>
    <!-- 中间的碰撞 -->
    <g transform="translate(380,98)">
      <path d="M0 -44 L9 -12 L40 -20 L16 4 L44 26 L10 20 L0 50 L-10 20 L-44 26 L-16 4 L-40 -20 L-9 -12 Z" fill="#f0c860" stroke="#12121a" stroke-width="3"/>
      <text x="0" y="8" text-anchor="middle" font-size="20" font-weight="800" fill="#12121a">VS</text>
    </g>
    <!-- 顶部的小字 -->
    <text x="380" y="28" text-anchor="middle" font-size="16" fill="#a9a9bd">双方同时出招 · 威力对撞 · 谁先倒下谁输</text>
  </svg>
</header>

<nav class="toc">
<div class="in tabs" role="tablist">
  <button class="tabbtn" id="tab-pvp" role="tab" data-pane="pvp" aria-selected="true">PvP 对战</button>
  <button class="tabbtn" id="tab-pve" role="tab" data-pane="pve" aria-selected="false">PvE 新增机制</button>
</div>
<div class="in links" data-for="pvp">
  <a href="#quick">一分钟看懂</a><a href="#ui">界面</a><a href="#res">资源</a><a href="#round">回合流程</a><a href="#acts">五种操作</a>
  <a href="#clash">对撞与闪避</a><a href="#wuxing">五行克制</a><a href="#field">场地</a><a href="#fams">招式图鉴</a><a href="#bomb">炸弹</a>
  <a href="#adv">进阶</a><a href="#tips">新手建议</a><a href="#stage">标准属性</a><a href="#demo">动画示范</a>
</div>
<div class="in links" data-for="pve" hidden>
  <a href="#pve-intro">总览</a><a href="#pve-elem">五行属性</a><a href="#pve-resist">抗性</a><a href="#pve-shield">护盾</a><a href="#pve-crit">暴击</a><a href="#pve-trait">特性</a><a href="#pve-order">结算顺序</a><a href="#pve-example">大数字例子</a><a href="#pve-tips">建议</a>
</div>
</nav>

<div class="wrap">
<div class="pane" id="pane-pvp">

<section id="quick">
  <h2><span class="no">1</span>一分钟看懂</h2>
  <p class="lead">《五行斗诀》是一个一对一的回合制对决小游戏：你和对手<b>每回合同时选一个招式</b>，然后一起结算。</p>
  <ol class="quick">
    <li><div><b>赢的方法：</b>把对手的<span style="color:var(--yellow)"><b>武力值</b></span>或<span style="color:var(--red)"><b>健康值</b></span>打到 0（任何一项归零都算输）。</div></li>
    <li><div><b>出招要花「真气」：</b>招式越强越贵；真气不够也能出，但缺的部分要用<b>两倍的武力值</b>来顶，很危险。</div></li>
    <li><div><b>威力对撞：</b>双方的招式碰在一起，<b>威力大的一方打出差额</b>；五行属性相克的话，被克的一方威力<b>减半</b>。</div></li>
    <li><div><b>招式分成 18 个家族，每个家族有 20 式：</b>每个家族有自己的特殊效果（穿透、禁锢、反弹、回复……），式数越大，威力越高、消耗越多、效果越强。</div></li>
    <li><div><b>除了出招，还可以「跳过」和用「真气炸弹」：</b>跳过会让你这回合的敏捷翻倍，很可能躲开对手的攻击；真气炸弹要先花几回合积攒，放出去威力惊人，但你自己也会受伤。</div></li>
  </ol>
  <div class="note"><b>这一页讲的是 PvP 对战。</b>人机/剧情里的 PvE 在这些规则之上多了五行属性、抗性、暴击、特性等机制，等你熟悉了 PvP，再点上方的 <a href="#pve">PvE 新增机制</a> 去了解。</div>
</section>

<section id="ui">
  <h2><span class="no">2</span>界面速览</h2>
  <p class="lead">先认识一下对局界面（下图是示意图，数字对应下面的说明）。</p>
  <div class="ui" style="position:relative">
    <div class="row">
      <div class="st rel"><span class="callout">1</span><b>玩家</b>　AGI 10/10
        {bar('MP', '#3c82dc', 70, '17360/24800')}{bar('ATK', '#dcaf32', 100, '38900/38900')}{bar('HP', '#d23c3c', 82, '21156/25800')}</div>
      <div class="st"><b>售货员</b>　AGI 9/10
        {bar('MP', '#3c82dc', 45, '11160/24800')}{bar('ATK', '#dcaf32', 64, '24896/38900')}{bar('HP', '#d23c3c', 100, '25800/25800')}</div>
    </div>
    <div class="env rel"><span class="callout">2</span>场地：<span class="k">灼烧 伤害x1.5 剩2回合</span></div>
    <div class="body">
      <div class="log rel"><span class="callout">3</span>
        <div class="h">== 第3回合 ==</div>
        玩家消耗3590点真气对售货员出招：铁手功第二十式<br>
        售货员消耗4040点真气对玩家出招：海龙金身第二十式<br>
        玩家对售货员造成金属性1230点招式武力伤害<br>
        场地出现：灼烧(伤害x1.5，还剩2回合)<br>
        玩家真气量回复至17360<br>
        <div class="h">== 第4回合 ==</div>
      </div>
      <div class="btns">
        <div class="r2 rel"><span class="callout">4</span><div class="b sel">单招</div><div class="b">双招</div></div>
        <div class="r3"><div class="b acc">积攒真气炸弹</div><div class="b bomb">释放真气炸弹(0)</div><div class="b skip">跳过</div></div>
        <div class="slider rel"><span class="callout">5</span><span>第20式</span><div class="t"><i></i></div></div>
        <div class="hint">单招：点击按钮，按滑块选的式出招</div>
        <div class="rel"><span class="callout">6</span>
          <div class="mfam"><div class="s" style="background:#9a9aaa"></div><div class="n">无招</div><div>3200/9440</div></div>
          <div class="mfam"><div class="s" style="background:#9a9aaa"></div><div class="n">千腿</div><div>3770/12298</div></div>
          <div class="mfam"><div class="s" style="background:#c9c9d6"></div><div class="n">铁手功</div><div>3590/9711</div></div>
          <div class="mfam"><div class="s" style="background:#3a7be0"></div><div class="n">海龙金身</div><div>4040/4825</div></div>
        </div>
      </div>
    </div>
  </div>
  <ol class="legend" style="margin-top:14px">
    <li><b>状态条</b>：蓝色=真气 MP，黄色=武力 ATK，红色=健康 HP。名字后的 AGI 是敏捷（当前/上限）。</li>
    <li><b>场地栏</b>：当前生效的场地效果和剩余回合数，没有就显示"无"。</li>
    <li><b>战报</b>：每一步都会写出来，谁出了什么招、造成了多少伤害、有什么特殊效果。</li>
    <li><b>操作区</b>：<span class="kbd">单招</span>/<span class="kbd">双招</span> 切换出招方式；下面三个按钮是积攒、放炸弹、跳过。</li>
    <li><b>式数滑块</b>：决定你出的是每个家族的第几式（越大越强，也越贵）。</li>
    <li><b>招式列表</b>：每个家族一行，右边写着"消耗/威力"。把鼠标放上去会弹出这一式的效果说明。</li>
  </ol>
</section>

<section id="res">
  <h2><span class="no">3</span>你的三项资源和敏捷</h2>
  <div class="grid g3">
    <div class="card"><div class="big" style="color:var(--blue)">◈</div><h3 style="margin-top:6px">真气 MP</h3>
      <p>出招要花的"钱"。每回合开始会<b>自动回复</b>一点，上限是你的最大真气。</p>
      <p style="color:var(--ink-soft);font-size:14px">⚠ 如果真气被撑到最大值的 <b>2 倍</b>以上（比如大量吸收），你会直接爆体而亡。</p></div>
    <div class="card"><div class="big" style="color:var(--yellow)">✦</div><h3 style="margin-top:6px">武力 ATK</h3>
      <p>你的战斗力。普通招式的伤害主要扣它。<b>归零就输。</b></p>
      <p style="color:var(--ink-soft);font-size:14px">真气不够用时，缺的部分<b>按 2 倍</b>从武力里扣（透支）。</p></div>
    <div class="card"><div class="big" style="color:var(--red)">♥</div><h3 style="margin-top:6px">健康 HP</h3>
      <p>你的血量。<b>归零也输。</b>物理伤害、炸弹和很重的一击会直接扣它。</p>
      <p style="color:var(--ink-soft);font-size:14px">另有一条：单次受到的武力伤害太大，也会连带扣健康。</p></div>
  </div>

  <h3>敏捷 AGI：决定能不能躲</h3>
  <p>敏捷越高越容易闪避。标准敏捷随阶位变化：<b>3-1 是 5.25，4-6 是 10</b>，也是你的敏捷上限。<b>跳过一次要损失 1 点敏捷，连续跳过会越扣越多</b>（第 2 次扣 1.5，第 3 次扣 2，以此类推；出招或积攒后清零），每回合会自动回复 0.2 点；敏捷不够（跳过之后会低于 1）时，这次跳过不翻倍，只能用原来的敏捷去躲。后面"对撞与闪避"一节会详细讲。</p>

  <h3>武力受伤是怎么算的</h3>
  <svg viewBox="0 0 760 170" class="svg-block" role="img" aria-label="武力伤害示意">
    <rect x="20" y="40" width="720" height="34" rx="8" fill="#dcaf32" opacity=".85"/>
    <rect x="20" y="40" width="288" height="34" rx="8" fill="#e8c860"/>
    <text x="164" y="63" text-anchor="middle" font-size="14" fill="#12121a" font-weight="700">小于 30%：只扣武力</text>
    <rect x="308" y="40" width="216" height="34" fill="#d8962c"/>
    <text x="416" y="63" text-anchor="middle" font-size="14" fill="#12121a" font-weight="700">30%~60%：另外再扣一半的健康</text>
    <rect x="524" y="40" width="216" height="34" rx="0" fill="#c8402c"/>
    <text x="632" y="63" text-anchor="middle" font-size="14" fill="#fff" font-weight="700">≥60%：另外再扣全额的健康</text>
    <text x="20" y="24" font-size="14" fill="var(--ink-soft)">你一次受到的武力伤害，占你「当前武力」的比例（武力总是会被扣掉这么多，下面是额外要不要扣健康）：</text>
    <path d="M308 80 v22 M524 80 v22" stroke="var(--ink-soft)" stroke-width="2"/>
    <text x="308" y="120" text-anchor="middle" font-size="14" fill="var(--ink-soft)">30%</text>
    <text x="524" y="120" text-anchor="middle" font-size="14" fill="var(--ink-soft)">60%</text>
    <text x="380" y="156" text-anchor="middle" font-size="15" fill="var(--ink)">所以：武力越低，同样的一击越容易把健康也打穿。</text>
  </svg>
</section>

<section id="round">
  <h2><span class="no">4</span>一个回合怎么进行</h2>
  <div class="steps" style="margin-top:20px">
    <div class="step"><span class="n">1</span><h4>回合开始</h4><p>先结算<b>持续效果</b>（持续伤害、持续回复，上回合被禁锢的人这时开始被锁住），然后双方自动<b>回复真气和敏捷</b>。</p></div>
    <div class="step"><span class="n">2</span><h4>同时选招</h4><p>你和对手<b>各自选一个行动</b>，选的时候看不到对方选了什么——所以要猜、要读心。</p></div>
    <div class="step"><span class="n">3</span><h4>出招与结算</h4><p>先扣出招的真气，再按"双方对撞"或"对手没有应对"的规则算伤害和特殊效果。</p></div>
    <div class="step"><span class="n">4</span><h4>回合结束</h4><p>场地效果的计时减一回合；有人倒下就分出胜负，否则进入下一回合。</p></div>
  </div>
  <svg viewBox="0 0 760 90" class="svg-block" style="margin-top:14px" aria-label="回合循环">
    <defs><marker id="a2" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="7" markerHeight="7" orient="auto"><path d="M0 0 L10 5 L0 10 z" fill="var(--gold)"/></marker></defs>
    <g font-size="15" text-anchor="middle" fill="var(--ink)">
      <rect x="10" y="22" width="150" height="46" rx="12" fill="var(--panel2)" stroke="var(--line)"/><text x="85" y="51">回复 · 持续效果</text>
      <rect x="205" y="22" width="150" height="46" rx="12" fill="var(--panel2)" stroke="var(--line)"/><text x="280" y="51">同时选招</text>
      <rect x="400" y="22" width="150" height="46" rx="12" fill="var(--panel2)" stroke="var(--line)"/><text x="475" y="51">结算伤害</text>
      <rect x="595" y="22" width="150" height="46" rx="12" fill="var(--panel2)" stroke="var(--line)"/><text x="670" y="51">有人倒下？</text>
    </g>
    <path d="M160 45 H200 M355 45 H395 M550 45 H590" stroke="var(--gold)" stroke-width="3" marker-end="url(#a2)"/>
    <path d="M670 70 Q670 86 380 86 Q85 86 85 72" fill="none" stroke="var(--gold)" stroke-width="2" stroke-dasharray="6 5" marker-end="url(#a2)"/>
    <text x="400" y="80" text-anchor="middle" font-size="13" fill="var(--ink-soft)">没有 → 下一回合</text>
  </svg>
</section>

<section id="acts">
  <h2><span class="no">5</span>每回合你能做的五件事</h2>
  <div class="grid g3" style="margin-top:14px">
    <div class="card"><h3 style="margin-top:0">① 单招</h3>
      <svg viewBox="0 0 220 70" class="svg-block"><rect x="10" y="14" width="200" height="42" rx="10" fill="#465a82"/><text x="110" y="42" text-anchor="middle" font-size="16" fill="#fff">铁手功 · 第20式</text></svg>
      <p>选一个家族、一个式，打出一招，是最常用的操作。真气够就正常扣；不够就先扣光真气，缺的部分用武力顶（见"透支"）。</p></div>
    <div class="card"><h3 style="margin-top:0">② 双招</h3>
      <svg viewBox="0 0 220 70" class="svg-block"><rect x="6" y="14" width="96" height="42" rx="10" fill="#465a82"/><text x="54" y="40" text-anchor="middle" font-size="14" fill="#fff">千腿</text><text x="110" y="42" text-anchor="middle" font-size="22" fill="var(--gold)">+</text><rect x="118" y="14" width="96" height="42" rx="10" fill="#465a82"/><text x="166" y="40" text-anchor="middle" font-size="14" fill="#fff">伏羲九针</text></svg>
      <p>同时打出两个招式：<b>消耗是两招之和，威力是两招之和的 0.6 倍</b>，两个招式的<b>效果都会生效</b>，属性按"双招属性合成"一节的规则合成。两招<b>必须是同一式</b>（比如都是第 20 式）。</p></div>
    <div class="card"><h3 style="margin-top:0">③ 积攒真气炸弹</h3>
      <svg viewBox="0 0 220 70" class="svg-block"><rect x="10" y="40" width="200" height="16" rx="8" fill="#00000040" stroke="var(--line)"/><rect x="10" y="40" width="120" height="16" rx="8" fill="#aa3c32"/><circle cx="110" cy="22" r="14" fill="#555"/><path d="M122 12 L132 4" stroke="#f0c860" stroke-width="3"/><circle cx="134" cy="3" r="3" fill="#ff8a30"/></svg>
      <p>把当前真气的一部分转成炸弹能量存起来。这一回合你<b>不出招</b>，对手的攻击会全额打在你身上，而且你这回合<b>受到的所有伤害再多 20%</b>（易伤，算作攒炸弹的代价），要小心。每次消耗当前真气的 20%，所以通常要攒几次。详见"真气炸弹"一节。</p></div>
    <div class="card"><h3 style="margin-top:0">④ 释放真气炸弹</h3>
      <svg viewBox="0 0 220 70" class="svg-block"><path d="M110 4 L122 26 L150 20 L132 40 L156 56 L126 54 L110 68 L94 54 L64 56 L88 40 L70 20 L98 26 Z" fill="#f0c860" stroke="#aa3c32" stroke-width="3"/></svg>
      <p>把积攒的能量一次放出：威力惊人，对手<b>躲不掉</b>，但你自己也会受到反噬。如果还没有攒过炸弹就点它，等于一次不扣敏捷的“纯跳过”：敏捷不翻倍也不损失，但这回合不回复真气。</p></div>
    <div class="card"><h3 style="margin-top:0">⑤ 跳过</h3>
      <svg viewBox="0 0 220 70" class="svg-block"><path d="M30 35 H150" stroke="#8a8a9a" stroke-width="6" stroke-linecap="round" stroke-dasharray="2 12"/><path d="M160 12 Q200 35 160 58 Q176 35 160 12 Z" fill="#4a8ff0"/></svg>
      <p>这回合什么都不出，但你的<b>敏捷翻倍</b>：双方敏捷相同时，有 <b>80%</b> 的概率躲开对手的攻击（具体见"对撞与闪避"一节的概率表）。代价是损失 1 点敏捷。</p></div>
    <div class="card"><h3 style="margin-top:0">被禁锢时</h3>
      <svg viewBox="0 0 220 70" class="svg-block"><rect x="14" y="10" width="192" height="50" rx="8" fill="#c82828"/><text x="110" y="42" text-anchor="middle" font-size="18" font-weight="700" fill="#fff">被禁锢！</text></svg>
      <p>这一回合你出不了招：<b>只有"积攒"和"跳过"能点</b>，其余按钮全部变灰（就算硬出也会被当成跳过）。</p></div>
  </div>
  <div class="note tip"><b>小提示：</b>把鼠标放在招式上，会弹出这一式的效果和数值；没有特殊效果的招式不会弹。</div>
</section>

<section id="clash">
  <h2><span class="no">6</span>对撞、命中与闪避</h2>
  <h3>双方都出招：威力对撞</h3>
  <svg viewBox="0 0 760 250" class="svg-block" role="img" aria-label="对撞示意">
    <text x="20" y="22" font-size="15" fill="var(--ink-soft)">例：你出威力 9000 的招，对手出威力 6000 的招，两招撞在一起——</text>
    <text x="20" y="62" font-size="15" fill="var(--ink)">你的招</text>
    <rect x="110" y="44" width="450" height="30" rx="6" fill="#3a7be0"/>
    <text x="118" y="65" font-size="14" font-weight="700" fill="#fff">威力 9000</text>
    <text x="20" y="112" font-size="15" fill="var(--ink)">对手的招</text>
    <rect x="110" y="94" width="300" height="30" rx="6" fill="#e0562e"/>
    <text x="118" y="115" font-size="14" font-weight="700" fill="#fff">威力 6000</text>
    <!-- 抵消部分 -->
    <rect x="110" y="44" width="300" height="80" rx="6" fill="none" stroke="var(--ink-soft)" stroke-width="2" stroke-dasharray="6 5"/>
    <text x="260" y="146" text-anchor="middle" font-size="14" fill="var(--ink-soft)">▲ 这 6000 互相抵消</text>
    <!-- 剩下的部分 -->
    <rect x="410" y="44" width="150" height="30" rx="6" fill="#f0c860"/>
    <text x="485" y="65" text-anchor="middle" font-size="14" font-weight="700" fill="#1b1608">剩下 3000</text>
    <path d="M485 78 V140 Q485 150 500 150 H640" fill="none" stroke="#f0c860" stroke-width="3" marker-end="url(#a4)"/>
    <defs><marker id="a4" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="7" markerHeight="7" orient="auto"><path d="M0 0 L10 5 L0 10 z" fill="#f0c860"/></marker></defs>
    <text x="650" y="146" font-size="15" font-weight="700" fill="#f0c860">打到对手身上</text>
    <text x="20" y="196" font-size="16" fill="var(--ink)">结果：<tspan font-weight="700" fill="#f0c860">对手受到 9000 − 6000 = 3000 点伤害</tspan>；你这一回合<tspan font-weight="700">不受这一招的伤害</tspan>。</text>
    <text x="20" y="222" font-size="14" fill="var(--ink-soft)">简单说：威力大的赢，只付出"差额"那么大的伤害；威力小的那一招被完全抵消掉。</text>
    <text x="20" y="242" font-size="14" fill="var(--ink-soft)">（招式自带的特殊效果，比如穿透、禁锢、持续伤害，双方一般都会照常生效。）</text>
  </svg>
  <div class="grid g2">
    <div class="card"><h3 style="margin-top:0">属性相克 = 威力减半</h3>
      <p>如果你的招式属性<b>克制</b>对手的，对手这一招的威力在对撞里按<b>一半</b>算。所以威力差不多的两招，被克制的一方往往直接输掉。</p></div>
    <div class="card"><h3 style="margin-top:0">平局</h3>
      <p>威力恰好相等时，双方都不受这一击的伤害，只有招式自带的特殊效果照常生效。</p></div>
  </div>

  <h3>对手没有出招应对：你的攻击全额打出，但对手有机会闪开</h3>
  <p>如果对手这回合选了<b>跳过</b>或<b>积攒</b>，或者他<b>被禁锢</b>出不了招，你的招式就没有对手的招来抵消：<b>全部威力</b>都会打到他身上——除非他<b>闪避</b>成功。闪避成功时这一击完全无效；闪避失败才会被命中（扬尘场地里命中还要再过 50% 那一关）。</p>
  <p><b>闪避概率 = 防守方敏捷² ÷（防守方敏捷² + 进攻方敏捷²）</b>。只看双方<b>当前敏捷的比值</b>：防守方敏捷越高越容易躲，进攻方敏捷越高越难躲；双方一样高就是 50%。</p>
  <div class="grid g3">
    <div class="card"><div class="big" style="font-size:30px">50%</div><b>对手选了「积攒」，双方敏捷相同</b><p style="color:var(--ink-soft);font-size:14px">没有跳过就用自己当前的敏捷去躲（被禁锢也一样），敏捷不翻倍。</p></div>
    <div class="card"><div class="big" style="font-size:30px;color:var(--green)">80%</div><b>对手选了「跳过」，双方敏捷相同</b><p style="color:var(--ink-soft);font-size:14px">跳过那一回合，防守方的敏捷<b>翻倍</b>来算；被禁锢的人选跳过也一样。</p></div>
    <div class="card"><div class="big" style="font-size:30px;color:var(--red)">0%</div><b>场上有锈蚀</b><p style="color:var(--ink-soft);font-size:14px">锈蚀场地让所有人都闪不掉；其他情况下，敏捷再低也有一点概率躲开。</p></div>
  </div>
  <table style="margin-top:14px">
    <tr><th>防守方 ÷ 进攻方的敏捷</th><th class="c">0.5</th><th class="c">0.75</th><th class="c">1</th><th class="c">1.5</th><th class="c">2</th><th class="c">3</th></tr>
    <tr><th>没跳过（用自己的敏捷）</th><td class="c">20%</td><td class="c">36%</td><td class="c">50%</td><td class="c">69%</td><td class="c">80%</td><td class="c">90%</td></tr>
    <tr><th>选了跳过（敏捷翻倍）</th><td class="c">50%</td><td class="c">69%</td><td class="c">80%</td><td class="c">90%</td><td class="c">94%</td><td class="c">97%</td></tr>
  </table>
  <p style="color:var(--ink-soft);font-size:14px">例：你的敏捷 5，对手的敏捷 10，比值是 0.5。你没跳过：躲开的概率只有 20%；你选了跳过：敏捷翻倍成 10，和对手一样高，概率 50%。</p>
  <div class="note warn"><b>注意：</b>闪避只对<b>普通招式</b>有效，<b>真气炸弹躲不掉</b>。</div>
</section>

<section id="wuxing">
  <h2><span class="no">7</span>五行克制</h2>
  <p class="lead">招式有七种属性：金、木、水、火、土、<b>五行</b>、<b>无属性</b>。克制只在"双方对撞"时起作用，效果是<b>让被克的一方威力减半</b>。</p>
  <div class="grid g2" style="align-items:center">
    <div class="card">{wheel_svg()}</div>
    <div>
      <table>
        <tr><th>克制方 →</th><th>被克的一方</th></tr>
        <tr><td>{attr_chip('water')} 水</td><td>火</td></tr>
        <tr><td>{attr_chip('fire')} 火</td><td>木</td></tr>
        <tr><td>{attr_chip('wood')} 木</td><td>水、土</td></tr>
        <tr><td>{attr_chip('metal')} 金</td><td>木</td></tr>
        <tr><td>{attr_chip('earth')} 土</td><td>金</td></tr>
        <tr><td>{attr_chip('all')} 五行</td><td>金、木、水、火、土（全部）</td></tr>
        <tr><td colspan="2">金、木、水、火、土<b>都克制</b> {attr_chip('none')}</td></tr>
      </table>
      <div class="note">五行属性没有被任何属性克制；无属性不克制任何属性，却被五个元素同时克制，所以无属性招式想靠"纯拼威力"并不轻松。</div>
    </div>
  </div>

  <h3>双招的属性怎么合成</h3>
  <p>双招里两个招式的属性会合成一个"最终属性"，用它去和对手对撞：</p>
  <table>
    <tr><th>组合</th><th class="c">最终属性</th><th>组合</th><th class="c">最终属性</th></tr>
    <tr><td>木 + 火</td><td class="c">火</td><td>金 + 水</td><td class="c">水</td></tr>
    <tr><td>火 + 土</td><td class="c">土</td><td>水 + 木</td><td class="c">木</td></tr>
    <tr><td>土 + 金</td><td class="c">土</td><td>火 + 金</td><td class="c">金</td></tr>
    <tr><td>金 + 木</td><td class="c">金</td><td>木 + 土</td><td class="c">木</td></tr>
    <tr><td>土 + 水</td><td class="c">土</td><td>水 + 火</td><td class="c">水</td></tr>
    <tr><td>无属性 + 某元素</td><td class="c">那个元素</td><td>五行 + 某元素</td><td class="c">那个元素</td></tr>
    <tr><td>同属性 + 同属性</td><td class="c">原属性</td><td>五行 + 无属性</td><td class="c">五行</td></tr>
  </table>
  <div class="note tip"><b>妙用：</b>用一个元素招式去搭配无属性的招，就能给无属性招"染色"，让它也能克制别人（反过来也有风险）。</div>
</section>

<section id="field">
  <h2><span class="no">8</span>场地效果</h2>
  <p class="lead">两种元素的招式对撞，会在场上留下持续 <b>3 回合</b>的场地效果，对<b>双方</b>都生效。</p>
  <div class="grid g4">
    <div class="card" style="border-top:5px solid #e0562e"><h3 style="margin-top:0">灼烧</h3><p><span class="chip" style="--c:#c9c9d6">金</span> + <span class="chip" style="--c:#e0562e">火</span></p><p><b>所有伤害 ×1.5</b></p></div>
    <div class="card" style="border-top:5px solid #82728c"><h3 style="margin-top:0">锈蚀</h3><p><span class="chip" style="--c:#c9c9d6">金</span> + <span class="chip" style="--c:#3a7be0">水</span></p><p><b>谁也闪避不了</b>；真气炸弹对双方的系数都变成 0.5</p></div>
    <div class="card" style="border-top:5px solid #b88a3e"><h3 style="margin-top:0">扬尘</h3><p><span class="chip" style="--c:#b88a3e">土</span> + <span class="chip" style="--c:#e0562e">火</span></p><p><b>所有攻击（包括对撞）的命中率只有 50%</b></p></div>
    <div class="card" style="border-top:5px solid #6e5a37"><h3 style="margin-top:0">泥沼</h3><p><span class="chip" style="--c:#b88a3e">土</span> + <span class="chip" style="--c:#3a7be0">水</span></p><p><b>出招消耗 ×1.5</b></p></div>
  </div>
  <h3>怎么触发、怎么扑灭</h3>
  <ul>
    <li><b>触发：</b>双方对撞的两招，最终属性刚好是上面的组合；或者一方打出的双招，<b>它自己的两个招式</b>的属性刚好组成这样的组合——但这种“双招自造”只在这个双招<b>没命中</b>（被闪避，或在扬尘里判定失败）时才会产生，命中了就不会。</li>
    <li><b>刷新：</b>同类场地再次触发会重新计时，不叠加。</li>
    <li><b>扑灭：</b>出招的属性能"压制"某些场地：
      <span class="chip" style="--c:#c9c9d6">金</span> 灭 扬尘、泥沼；
      <span class="chip" style="--c:#3a7be0">水</span> 灭 灼烧、扬尘；
      <span class="chip" style="--c:#e0562e">火</span> 灭 锈蚀、泥沼；
      <span class="chip" style="--c:#b88a3e">土</span> 灭 灼烧、锈蚀。</li>
  </ul>
  <p style="color:var(--ink-soft)">场地栏会实时显示当前效果和剩余回合，战报里也会写"场地出现/刷新/消失"。</p>
</section>

<section id="fams">
  <h2><span class="no">9</span>招式家族图鉴（共 18 个）</h2>
  <p class="lead">每个家族有 20 个式：式数越大，威力越高、消耗越贵，<b>特殊效果也越强</b>（比如禁锢概率从 4% 一路涨到 80%）。属性色条在招式列表里也是同样的颜色。</p>
  <div class="grid g3" style="margin-top:14px">
{family_cards()}
  </div>
</section>

<section id="eff">
  <h2><span class="no">10</span>特殊效果小词典</h2>
  <table>
    <tr><th style="width:110px">效果</th><th>说明</th></tr>
    <tr><td><b>穿透</b></td><td>对撞时，额外打出"双方较小那个威力"的一部分，哪怕你在对撞里输了也会打。</td></tr>
    <tr><td><b>物理</b></td><td>一部分伤害不扣武力、直接扣<b>健康</b>。</td></tr>
    <tr><td><b>减伤</b></td><td>受到的伤害减少一定比例。</td></tr>
    <tr><td><b>反弹</b></td><td>把受到伤害的一部分原样还给攻击者，<b>自己只承受剩下的</b>——它是减伤的升级版。</td></tr>
    <tr><td><b>吸收</b></td><td>把你造成的伤害的一部分转成自己的真气（注意别吸过头，真气超过上限 2 倍会爆体）。</td></tr>
    <tr><td><b>禁锢</b></td><td>命中后有概率让对手<b>下回合</b>无法出招（只能积攒或跳过）。</td></tr>
    <tr><td><b>持续伤害</b></td><td>命中后先打一次，之后每隔几回合再扣一次。不受对撞抵消。</td></tr>
    <tr><td><b>回复真气/武力/健康</b></td><td>立即回复一份，之后每隔几回合再回复一份。</td></tr>
    <tr><td><b>静止增伤</b></td><td>对手这回合<b>积攒炸弹</b>或<b>释放空炸弹</b>（没攒过就放）时，本招伤害多一截，和其他增伤加在同一个乘区里；对手<b>跳过</b>不算静止。双招里只有带这个效果的那一半按份量计入。</td></tr>
    <tr><td><b>连招增伤</b></td><td>连着上一回合<b>同家族的前一式</b>出招（比如先出第 18 式、再出第 19 式、再出第 20 式），每连续一步威力就多一截；中间只要<b>积攒、放炸弹、跳过或被禁锢</b>就断了，要从头再连。双招里包含这一招也算连着，不要求命中。</td></tr>
    <tr><td><b>护盾</b></td><td>出招时给自己加一块护盾（盾量是自己武力上限的一定比例），能同时挡<b>武力和健康</b>的伤害，只持续几回合。多块护盾可以同时存在、各自计时；受伤时每块盾都被减去同样的伤害，伤害超过最厚那块盾的部分才会真的扣武力/健康（反弹也只反弹穿过护盾的那一部分）。</td></tr>
  </table>
</section>

<section id="bomb">
  <h2><span class="no">11</span>真气炸弹：高风险高回报</h2>
  <svg viewBox="0 0 760 200" class="svg-block" role="img" aria-label="炸弹流程">
    <defs><marker id="a3" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="7" markerHeight="7" orient="auto"><path d="M0 0 L10 5 L0 10 z" fill="var(--gold)"/></marker></defs>
    <g font-size="15" text-anchor="middle" fill="var(--ink)">
      <rect x="10" y="40" width="200" height="110" rx="14" fill="var(--panel)" stroke="var(--line)"/>
      <text x="110" y="68" font-weight="700">① 积攒</text>
      <text x="110" y="94" font-size="13" fill="var(--ink-soft)">消耗当前真气的 20%</text>
      <text x="110" y="116" font-size="13" fill="var(--ink-soft)">按「阶位倍率」存成炸弹</text>
      <text x="110" y="138" font-size="13" fill="var(--gold)">（这回合你不出招）</text>
      <rect x="280" y="40" width="200" height="110" rx="14" fill="var(--panel)" stroke="var(--line)"/>
      <text x="380" y="68" font-weight="700">② 可以攒好几次</text>
      <text x="380" y="94" font-size="13" fill="var(--ink-soft)">能量会一直累加</text>
      <text x="380" y="116" font-size="13" fill="var(--ink-soft)">直到你按「释放」</text>
      <rect x="550" y="40" width="200" height="110" rx="14" fill="var(--panel)" stroke="var(--line)"/>
      <text x="650" y="68" font-weight="700">③ 释放</text>
      <text x="650" y="94" font-size="13" fill="var(--ink-soft)">对手没出招：受 炸弹×0.4</text>
      <text x="650" y="116" font-size="13" fill="var(--ink-soft)">对手出了招：受 (炸弹 − 他的威力)×0.8</text>
      <text x="650" y="138" font-size="13" fill="var(--red)">你自己受 炸弹×0.2；炸弹躲不掉</text>
    </g>
    <path d="M214 95 H274 M484 95 H544" stroke="var(--gold)" stroke-width="3" marker-end="url(#a3)"/>
  </svg>
  <h3>倍率由你的阶位决定</h3>
  <p>阶位越高，同样一次积攒存下的能量越多。倍率由你所在的阶位决定，低阶位很小，高阶位才有威力：</p>
  <table>
    <tr><th>阶位</th><th class="c">3-1</th><th class="c">3-2</th><th class="c">3-3</th><th class="c">3-4</th><th class="c">3-5</th><th class="c">3-6</th><th class="c">4-1</th><th class="c">4-2</th><th class="c">4-3</th><th class="c">4-4</th><th class="c">4-5</th><th class="c">4-6</th></tr>
    <tr><th>对应的式</th><td class="c">1</td><td class="c">2</td><td class="c">3</td><td class="c">4</td><td class="c">5</td><td class="c">6</td><td class="c">7-8</td><td class="c">9-10</td><td class="c">11-12</td><td class="c">13-14</td><td class="c">15-17</td><td class="c">18-20</td></tr>
    <tr><th>炸弹倍率</th><td class="c">1</td><td class="c">1.2</td><td class="c">1.5</td><td class="c">1.9</td><td class="c">2.4</td><td class="c">3</td><td class="c">3.7</td><td class="c">4.5</td><td class="c">5.6</td><td class="c">7</td><td class="c">8.5</td><td class="c">10</td></tr>
  </table>
  <div class="note warn"><b>危险：</b>积攒的那一回合你会多受 20% 的伤害（对手的招、对手的炸弹都算），和减伤、护盾一起结算。你的阶位越高，同样一次积攒存下的能量越多（见上表）；但释放时你自己也会吃 20% 的反噬，攒得越多越疼。<br><b>对手没出招</b>（跳过、积攒、没攒就放的“空炸弹”、被禁锢）时，炸弹只按 0.4 打过去，所以通常要攒好几次才能一击打死；<b>对手出了招</b>就按 0.7 计算差额（他的招本身要抵消一部分，还可能带减伤、反弹），所以炸弹对出招的人更狠。<br>如果<b>双方同时释放</b>，两个炸弹相加再乘 0.2，双方<b>各</b>承受这么多，攒得越多两边越疼。</div>
</section>

<section id="adv">
  <h2><span class="no">12</span>进阶规则</h2>
  <div class="grid g2">
    <div class="card"><h3 style="margin-top:0">透支：真气不够也能出招</h3>
      <svg viewBox="0 0 340 120" class="svg-block">
        <text x="10" y="22" font-size="14" fill="var(--ink-soft)">出招要 3000 真气，你只剩 1000：</text>
        <rect x="10" y="34" width="320" height="22" rx="6" fill="#00000040" stroke="var(--line)"/>
        <rect x="10" y="34" width="107" height="22" rx="6" fill="#3c82dc"/>
        <rect x="117" y="34" width="213" height="22" rx="0" fill="#c8402c" opacity=".35"/>
        <text x="63" y="50" text-anchor="middle" font-size="12" fill="#fff">真气全部用掉</text>
        <text x="223" y="50" text-anchor="middle" font-size="12" fill="var(--ink)">缺 2000</text>
        <text x="10" y="86" font-size="14" fill="var(--ink)">→ 额外扣掉 <tspan font-weight="700" fill="var(--yellow)">2 × 2000 = 4000</tspan> 点武力值</text>
        <text x="10" y="108" font-size="13" fill="var(--ink-soft)">先把真气扣光，缺的才靠武力顶</text></svg>
      <p>武力同时是你的"命"，透支太多可能把自己打倒，要权衡。</p></div>
    <div class="card"><h3 style="margin-top:0">禁锢：一回合"哑火"</h3>
      <p>被禁锢的那一回合，你选的单招、双招、炸弹都会被<b>当作跳过</b>，招式上的减伤、反弹等效果也不生效。</p>
      <p>你只能选 <b>积攒</b> 或 <b>跳过</b>：选跳过的话，敏捷翻倍，双方敏捷相同时有 <b>80%</b> 概率躲开对方的攻击；选积攒也能躲，只是敏捷不翻倍（相同时 50%）。对方要先打中你才能再次禁锢，所以想连续锁住你并不容易。</p></div>
    <div class="card"><h3 style="margin-top:0">敏捷的账</h3>
      <p>跳过 <b>−1</b> 敏捷，每回合自动 <b>+0.2</b>，最高不超过初始值。<b>连续跳过要多扣</b>：第 1 次扣 1，第 2 次扣 1.5，第 3 次扣 2……只要中间出过招、积攒或放过炸弹就重新从 1 算。真气不够时，与其一直跳过，不如改出便宜的低式招式。</p>
      <p>敏捷 &lt; 2 时，再跳过就会低于 1：这次跳过<b>不再翻倍</b>（也不扣敏捷），只能用自己原来的敏捷去躲。</p></div>
    <div class="card"><h3 style="margin-top:0">效果的叠加</h3>
      <p>双招里<b>两个招式的效果都生效</b>；但如果两个招式带的是<b>同一种效果</b>，只取<b>第一个</b>招式的（比如逆天神功+地狱阴魔拳取逆天神功的禁锢，顺序反过来就取地狱阴魔拳的）。如果其中一个带"物理"，整个双招的伤害就按物理比例结算。</p>
      <p>双招的消耗是两招之和，威力却只有两招之和的 0.6 倍（约为单招的 1.2 倍）：双招贵在<b>两个效果同时生效</b>，而不是威力。</p></div>
  </div>
</section>

<section id="tips">
  <h2><span class="no">13</span>十条新手建议</h2>
  <ol class="quick">
    <li><div><b>先盯住资源：</b>每回合看一眼双方的真气、武力、健康。别让武力掉到很低——一次重击就能把健康也带走。</div></li>
    <li><div><b>别无脑出最大的招：</b>真气一下就见底，之后每回合只能回复一点，全靠透支会很快把自己拖垮。</div></li>
    <li><div><b>记住五行克制：</b>对手上回合出了什么属性，你这回合就挑克它的；同样的威力，克制能让对手的招式缩水一半。</div></li>
    <li><div><b>别给对手"白打"的机会：</b>你积攒的回合不出招，会被全额打中，还要多受 20% 的伤害；被禁锢时选跳过还有 80% 概率闪开。出手前想想对手这回合会不会打你。</div></li>
    <li><div><b>跳过是躲大招的好办法：</b>预感对手要放大招，就跳过——敏捷相同时有 80% 概率躲开；但跳过要扣敏捷，连续跳扣得更多，别连着跳。</div></li>
    <li><div><b>善用双招组合：</b>把"回复/持续伤害"的招式和高威力招式配成一对，往往比单招更赚。</div></li>
    <li><div><b>留意场地：</b>灼烧时伤害 ×1.5——领先时尽量别触发，落后时可以故意制造；泥沼时出招变贵，先别出大招。</div></li>
    <li><div><b>炸弹是赌注：</b>攒炸弹的那一回合你不出招，被人打一下是常态；别在对手也可能放炸弹时释放，否则两个炸弹合并，很可能同归于尽。</div></li>
    <li><div><b>高阶要防禁锢：</b>式数越高禁锢概率越大。被锁的那回合优先点"跳过"，不要硬点没用的招。</div></li>
    <li><div><b>多试几次：</b>想看某个招式的具体数值，把鼠标放上去就有说明；想练手，可以先从低阶位（第 1 式）开始，规则熟了再往高阶挑战。</div></li>
  </ol>
</section>

<section id="stage">
  <h2><span class="no">14</span>标准属性（各阶位的初始数值）</h2>
  <p class="lead">双方在同一阶位对决时，初始属性是一样的（敏捷按阶位：3-1 为 5.25，之后随式数每式 +0.25，4-6 为 10；式越低，跳过的敏捷损失越吃亏）。想跨阶位对决，可以自己改属性。</p>
  <table>
    <tr><th>阶位</th><th>对应式</th><th class="c">健康</th><th class="c">武力</th><th class="c">真气</th><th class="c">每回合回复真气</th></tr>
    <tr><td>3-1</td><td>1</td><td class="c">1000</td><td class="c">1200</td><td class="c">800</td><td class="c">20</td></tr>
    <tr><td>3-2</td><td>2</td><td class="c">1230</td><td class="c">1480</td><td class="c">960</td><td class="c">24</td></tr>
    <tr><td>3-3</td><td>3</td><td class="c">1510</td><td class="c">1810</td><td class="c">1150</td><td class="c">29</td></tr>
    <tr><td>3-4</td><td>4</td><td class="c">1770</td><td class="c">2180</td><td class="c">1400</td><td class="c">35</td></tr>
    <tr><td>3-5</td><td>5</td><td class="c">2090</td><td class="c">2640</td><td class="c">1720</td><td class="c">43</td></tr>
    <tr><td>3-6</td><td>6</td><td class="c">2450</td><td class="c">3180</td><td class="c">2100</td><td class="c">52</td></tr>
    <tr><td>4-1</td><td>7–8</td><td class="c">3110</td><td class="c">4190</td><td class="c">2830</td><td class="c">71</td></tr>
    <tr><td>4-2</td><td>9–10</td><td class="c">4270</td><td class="c">6050</td><td class="c">4200</td><td class="c">105</td></tr>
    <tr><td>4-3</td><td>11–12</td><td class="c">5840</td><td class="c">8710</td><td class="c">6250</td><td class="c">156</td></tr>
    <tr><td>4-4</td><td>13–14</td><td class="c">8560</td><td class="c">12900</td><td class="c">9080</td><td class="c">227</td></tr>
    <tr><td>4-5</td><td>15–17</td><td class="c">14100</td><td class="c">21400</td><td class="c">14300</td><td class="c">358</td></tr>
    <tr><td>4-6</td><td>18–20</td><td class="c">25800</td><td class="c">38900</td><td class="c">24800</td><td class="c">619</td></tr>
  </table>
  <p style="color:var(--ink-soft);font-size:14px">在模拟器里，选了"AI 用第 N 式"，双方的标准属性会自动换成对应阶位的这一行。</p>
</section>

{DEMO}
</div>

<div class="pane" id="pane-pve" hidden>
{PVE}
</div>

<footer>
  <b>关于数值：</b>游戏里各招式的消耗、威力、效果数值仍在持续平衡调整中，本指南讲的是规则和思路，具体数字以游戏里显示的为准。<br>
  想一起体验对决？回到主界面，点「人机对战」和AI对手练手，或点「开始匹配」和真人对战。
</footer>

</div>

<script>
// PvP / PvE 两页在同一个文件里切换；#pve / #pvp 或任何一页里的锚点都能直接打开对应那一页
(function () {{
  var panes = {{ pvp: document.getElementById('pane-pvp'), pve: document.getElementById('pane-pve') }};
  var links = document.querySelectorAll('nav.toc .links');
  var tabs = document.querySelectorAll('.tabbtn');
  function show(name) {{
    for (var k in panes) panes[k].hidden = k !== name;
    links.forEach(function (l) {{ l.hidden = l.dataset['for'] !== name; }});
    tabs.forEach(function (t) {{ t.setAttribute('aria-selected', String(t.dataset.pane === name)); }});
  }}
  function fromHash() {{
    var id = decodeURIComponent(location.hash.slice(1));
    if (id === 'pvp' || id === 'pve') {{ show(id); window.scrollTo(0, 0); return true; }}
    var el = id && document.getElementById(id);
    if (!el) return false;
    for (var k in panes) if (panes[k].contains(el)) {{ show(k); el.scrollIntoView(); return true; }}
    return false;
  }}
  tabs.forEach(function (t) {{
    t.addEventListener('click', function () {{ show(t.dataset.pane); history.replaceState(null, '', '#' + t.dataset.pane); window.scrollTo(0, 0); }});
  }});
  window.addEventListener('hashchange', fromHash);
  if (!fromHash()) show('pvp');
}})();
</script>
</body>
</html>
'''

open(OUT, 'w', encoding='utf-8', newline='\n').write(HTML)
print('written', len(HTML))
