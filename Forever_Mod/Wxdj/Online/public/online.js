"use strict";
/* 五行斗诀 联机对战(测试版)的客户端。
 * 战斗规则全部来自 engine.js(从离线版抽出)；两边浏览器各用同一个随机种子、同一串出招结算，结果一致。
 * 界面和出招流程照离线版 wxdj_simulator.html 改：去掉对手设置/文件加载/可编辑属性，改成联机的等待与重开流程。
 * 引擎里战斗按座位顺序固定为 [座位0, 座位1]；界面上"我"永远在左边，下标用 state.seat 映射。 */

const $ = id => document.getElementById(id);
const STANDARD = { hp: 25800, atk: 38900, mp: 24800, recover: 619, agi: 10, agiLoss: 1, agiRecover: 0.2, bombMultiplier: 10 };   // 4-6阶位标准属性(和离线版默认一致)
const ATTR_COLORS = ["#505060", "#b4b4be", "#3c9646", "#326ec8", "#c84628", "#aa823c", "#9646be"];

const state = {
  defs: [], families: [], formMax: 20, selectedForm: 19,
  mode: "single", firstPick: -1, firstPickFamily: -1,
  seat: 0, cults: [], controller: null,
  // connecting | waiting | select | submitted | result | rematch | oppquit | closed | local
  phase: "connecting",
  round: 1, result: 0, lastEnv: [], roundLogs: [],
  ws: null, pingTimer: 0,
};
const me = () => state.cults[state.seat];
const op = () => state.cults[1 - state.seat];

/* ---- 伪随机：mulberry32，整数运算，各浏览器结果完全一致 ---- */
function mulberry32(a) {
  return function () {
    a |= 0; a = (a + 0x6D2B79F5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

/* ---- 招式表 ---- */
function loadActions(text) {
  const defs = parseActions(text);
  if (!defs.length) return false;
  state.defs = defs;
  state.families = [];
  defs.forEach((d, i) => {
    const pos = d.name.indexOf("第");
    const name = pos < 0 ? d.name : d.name.slice(0, pos);
    const last = state.families[state.families.length - 1];
    if (last && last.name === name) last.formCount++;
    else state.families.push({ name, firstAction: i, formCount: 1 });
  });
  state.formMax = Math.min(...state.families.map(f => f.formCount));
  state.selectedForm = state.formMax - 1;
  return true;
}
function actionIndexOf(family) {
  const f = state.families[family];
  return f.firstAction + Math.min(state.selectedForm, f.formCount - 1);
}

/* ---- 开局 ---- */
function newBattle(seed) {
  const nameOf = s => (s === state.seat ? "你" : "对手");
  state.cults = [0, 1].map(s => new Cultivator(nameOf(s), state.defs, STANDARD.hp, STANDARD.atk, STANDARD.mp, STANDARD.agi,
    STANDARD.recover, STANDARD.agiLoss, STANDARD.agiRecover, STANDARD.bombMultiplier));
  state.controller = new Controller(state.cults);
  state.controller.rand = mulberry32(seed);
  state.round = 1; state.result = 0; state.lastEnv = []; state.roundLogs = [];
  state.firstPick = -1; state.firstPickFamily = -1; state.mode = "single";
  $("log").innerHTML = "";
  appendHeader();
  state.controller.startTurn();
  appendNews();
  state.phase = "select";
  checkEnd(false);
  refreshAll();
}

/* ---- 场地效果 ---- */
const ENV_INFO = {
  [E_BURN]: { name: "灼烧", desc: "伤害x1.5", color: "#c85028" },
  [E_RUST]: { name: "锈蚀", desc: "必中，炸弹系数0.5", color: "#82728c" },
  [E_DUST]: { name: "扬尘", desc: "命中率50%", color: "#aa8c46" },
  [E_MUD]: { name: "泥沼", desc: "消耗x1.5", color: "#6e5a37" },
};
const envRemaining = e => e.total - e.currentRound;
function refreshEnvironment() {
  const bar = $("envBar");
  bar.innerHTML = '<span class="lab">场地：</span>';
  const list = state.controller ? state.controller.environment : [];
  if (!list.length) { const n = document.createElement("span"); n.className = "none"; n.textContent = "无"; bar.appendChild(n); return; }
  for (const e of list) {
    const info = ENV_INFO[e.type], chip = document.createElement("span");
    chip.className = "chip"; chip.style.background = info.color;
    chip.textContent = info.name + " " + info.desc + " 剩" + envRemaining(e) + "回合";
    bar.appendChild(chip);
  }
}
function logEnvironmentChanges() {
  const now = state.controller.environment.map(e => [e.type, envRemaining(e)]);
  const before = state.lastEnv || [];
  for (const [type, left] of now) {
    const info = ENV_INFO[type], old = before.find(b => b[0] === type);
    if (!old) pushLogLine("场地出现：" + info.name + "(" + info.desc + "，还剩" + left + "回合)");
    else if (left > old[1]) pushLogLine("场地刷新：" + info.name + "(还剩" + left + "回合)");
  }
  for (const [type] of before) if (!now.some(n => n[0] === type)) pushLogLine("场地消失：" + ENV_INFO[type].name);
  state.lastEnv = now;
}

/* ---- 战报 ---- */
function formatLog(log) {
  const who = i => (i === 0 || i === 1 ? state.cults[i].name : "");
  const cult = i => state.cults[i];
  const damageText = kind => {
    let s = "";
    if (log.pointAtk > 0) s += ATTR_TEXT[log.attribute] + log.pointAtk + "点" + kind + "武力伤害";
    if (log.pointAtk > 0 && log.pointHp > 0) s += "和";
    if (log.pointHp > 0 || log.pointAtk <= 0) s += log.pointHp + "点" + kind + "物理伤害";
    return s;
  };
  switch (log.type) {
    case L_MP: return who(log.subject) + "真气量回复至" + cult(log.subject).currentMP;
    case L_ATK: return who(log.subject) + "武力值回复至" + cult(log.subject).currentATK;
    case L_HP: return who(log.subject) + "健康值回复至" + cult(log.subject).currentHP;
    case L_CONSUME: return who(log.subject) + "消耗" + log.amount + "点武力值";
    case L_SKIP: return who(log.subject) + "跳过回合";
    case L_MOVE: return who(log.subject) + "消耗" + log.amount + "点真气对" + who(log.object) + "出招：" + log.name + "，点数" + log.pointAtk;
    case L_DAMAGE: return who(log.subject) + "对" + who(log.object) + "造成" + damageText("招式");
    case L_DODGE: return who(log.object) + "闪避成功";
    case L_FAIL: return who(log.subject) + "出招失败";
    case L_EFFECT:
      switch (log.effect) {
        case E_PENETRATE: return who(log.subject) + "对" + who(log.object) + "造成" + damageText("穿透");
        case E_REBOUND: return who(log.subject) + "对" + who(log.object) + "造成" + damageText("反弹");
        case E_ABSORB: return who(log.subject) + "吸收" + log.amount + "点真气";
        case E_LOCK: return who(log.object) + "被" + who(log.subject) + "禁锢";
        case E_DOT: return who(log.object) + "受到" + log.amount + "点持续武力伤害";
        case E_REBATE: return who(log.subject) + "恢复" + log.amount + "点真气";
        case E_RECOVER: return who(log.subject) + "恢复" + log.amount + "点武力值";
        case E_CURE: return who(log.subject) + "恢复" + log.amount + "点健康值";
      }
  }
  return "";
}
function pushLogLine(text, header = false) {
  if (!text) return;
  const box = $("log");
  const div = document.createElement("div");
  div.textContent = text;
  if (header) div.className = "hdr";
  box.appendChild(div);
  while (box.childElementCount > 2000) box.removeChild(box.firstChild);
  box.scrollTop = box.scrollHeight;
}
function appendHeader() { pushLogLine("== 第" + state.round + "回合 ==", true); }
function appendNews() { const news = state.controller.getNews(); for (const log of news) pushLogLine(formatLog(log)); }

/* ---- 结算：服务端把双方出招同时发来，按座位顺序喂给引擎 ---- */
function buildAction(cult, d) {
  const n = state.defs.length;
  const ok = v => Number.isInteger(v) && v >= 0 && v < n;
  if (!d) return skipAction();
  switch (d.t) {
    case "acc": return accumulateAction();
    case "bomb": return bombAction();
    case "single": return ok(d.i) ? cult.getAction(d.i) : skipAction();
    case "dual": return ok(d.i) && ok(d.j) ? cult.getAction(d.i, d.j) : skipAction();
    default: return skipAction();
  }
}
function digest() {
  const cs = state.cults.map(c => [c.currentHP, c.currentATK, c.currentMP, +c.agi.toFixed(4), c.bomb].join(","));
  const env = state.controller.environment.map(e => e.type + ":" + envRemaining(e)).join(",");
  return cs.join("|") + "|" + env;
}
function resolveRound(acts) {
  if (state.phase !== "submitted" && state.phase !== "select") return;
  const roundNo = state.round;
  const actions = [0, 1].map(s => buildAction(state.cults[s], acts[s]));
  const pairs = [[actions[0], 1], [actions[1], 0]];     // 座位0打座位1，座位1打座位0
  const c = state.controller;
  state.roundLogs = [];
  c.actionTurn(pairs); appendNews();
  c.checkTurn(pairs); appendNews();
  c.endTurn(); appendNews();
  logEnvironmentChanges();
  state.firstPick = -1; state.firstPickFamily = -1;
  state.phase = "select";
  checkEnd(true);
  send({ t: "sync", r: roundNo, h: digest() });
  refreshAll();
}
function checkEnd(advance) {
  const meDead = !me().isAlive(), opDead = !op().isAlive();
  if (meDead || opDead) {
    state.result = meDead && opDead ? 0 : meDead ? -1 : 1;
    state.phase = "result";
  } else if (advance) {
    state.round++;
    appendHeader();
    state.controller.startTurn();
    appendNews();
    if (!me().isAlive() || !op().isAlive()) checkEnd(false);   // 开局恢复真气也可能超过上限2倍而致死
  }
}

/* ---- 联机 ---- */
function wsUrl() {
  const q = new URLSearchParams(location.search).get("ws");
  if (q) return q;
  return (location.protocol === "https:" ? "wss://" : "ws://") + location.host + "/ws";
}
function connect() {
  if (location.protocol !== "http:" && location.protocol !== "https:") { state.phase = "local"; refreshAll(); return; }   // 本地双击打开(file:)：没有服务器可连
  // 本地测试页(tools/test_harness.html)会提供一个假的 WebSocket，不连真实服务器
  const hook = window.parent !== window && window.parent.WXDJ_CONNECT;
  const ws = hook ? window.parent.WXDJ_CONNECT() : new WebSocket(wsUrl());
  state.ws = ws;
  ws.onmessage = ev => { let m = null; try { m = JSON.parse(ev.data); } catch (e) { return; } onMessage(m); };
  ws.onclose = () => {
    clearInterval(state.pingTimer);
    if (!state.leaving && state.phase !== "oppquit") { state.phase = "closed"; refreshAll(); }
  };
  ws.onerror = () => { /* onclose 会处理 */ };
  state.pingTimer = setInterval(() => send({ t: "ping" }), 25000);
  refreshAll();
}
// 离开当前对局/匹配并回到主界面：先告诉服务端(对手会收到"对手已退出")，再关连接
function goHome() {
  state.leaving = true;
  send({ t: "quit" });
  try { state.ws.close(); } catch (e) { /* 已关闭 */ }
  location.href = "../index.html";
}
function send(obj) {
  const ws = state.ws;
  if (ws && ws.readyState === 1) ws.send(JSON.stringify(obj));
}
function onMessage(m) {
  switch (m.t) {
    case "waiting": state.phase = "waiting"; refreshAll(); break;
    case "matched": state.seat = m.seat; newBattle(m.seed); break;
    case "restart": newBattle(m.seed); break;
    case "round": resolveRound(m.acts); break;
    case "opp_quit":
      state.phase = "oppquit"; refreshAll();
      break;
    case "desync":
      pushLogLine("⚠ 两边的计算结果不一致(第" + m.r + "回合)，这局可能已经不同步，请退出后重新匹配");
      console.error("desync at round", m.r);
      break;
  }
}

/* ---- 出招 ---- */
function playerLocked() {
  return state.phase === "select" && state.controller && (state.controller.status[state.seat] & 1) !== 0;
}
function submit(desc) {
  if (state.phase !== "select") return;
  if (playerLocked() && desc.t !== "skip" && desc.t !== "acc") return;
  state.firstPick = -1; state.firstPickFamily = -1;
  state.phase = "submitted";                // 已出招：全屏锁住，等对手
  send({ t: "act", a: desc });
  refreshAll();
}
function onFamilyClick(family) {
  if (state.phase !== "select" || playerLocked()) return;
  const idx = actionIndexOf(family);
  if (state.mode === "single") submit({ t: "single", i: idx });
  else if (state.firstPick < 0) { state.firstPick = idx; state.firstPickFamily = family; refreshAll(); }
  else submit({ t: "dual", i: state.firstPick, j: idx });
}

/* ---- 界面 ---- */
function buildStatus(idx) {
  const root = $("status" + idx);
  root.innerHTML = `
    <div class="nameRow"><span class="name"></span>
      <span class="meta" data-f="agi"></span><span class="meta" data-f="mult"></span><span class="meta" data-f="recover"></span><span class="meta" data-f="bomb"></span></div>
    ${[["mp", "MP"], ["atk", "ATK"], ["hp", "HP"]].map(([k, label]) => `
    <div class="barRow"><div class="bar"><div class="fill ${k}" data-fill="${k}"></div></div>
      <div class="val"><b>${label}</b><span data-f="cur-${k}"></span> / <span data-f="max-${k}"></span></div></div>`).join("")}`;
}
function refreshStatus() {
  if (!state.cults.length) return;
  [0, 1].forEach(pos => {                            // pos 0 = 左边(我)，pos 1 = 右边(对手)
    const c = pos === 0 ? me() : op(), root = $("status" + pos);
    const set = (f, v) => { const el = root.querySelector(`[data-f="${f}"]`); if (el) el.textContent = v; };
    root.querySelector(".name").textContent = c.name;
    set("agi", "AGI " + +c.agi.toFixed(1) + "/" + c.maxAgi);
    set("mult", "炸弹倍率 " + c.bombMultiplier);
    set("recover", "回复 " + c.recover);
    set("bomb", "炸弹 " + c.bomb);
    for (const [k, cur, max] of [["mp", c.currentMP, c.maxMP], ["atk", c.currentATK, c.maxATK], ["hp", c.currentHP, c.maxHP]]) {
      set("cur-" + k, cur); set("max-" + k, max);
      const pct = max > 0 && cur > 0 ? Math.min(100, cur * 100 / max) : 0;
      root.querySelector(`[data-fill="${k}"]`).style.width = pct + "%";
    }
  });
}
function hintText() {
  if (playerLocked()) return "你被禁锢了：只能积攒真气炸弹或跳过回合";
  if (state.mode === "single") return "单招：点击按钮，按滑块选的式出招";
  if (state.firstPick < 0) return "双招：请选择第一招";
  return "双招：第一招「" + state.defs[state.firstPick].name + "」，请选择第二招(同式，滑块已锁定)";
}
function refreshActions() {
  $("modeSingle").classList.toggle("sel", state.mode === "single");
  $("modeDual").classList.toggle("sel", state.mode === "dual");
  const live = state.phase === "select";
  $("accBtn").disabled = !live;
  $("skipBtn").disabled = !live;
  const lk = playerLocked();
  if (lk) { state.firstPick = -1; state.firstPickFamily = -1; }
  $("bombBtn").disabled = !live || lk;   // 没攒过炸弹也能点：空炸弹是不扣敏捷的纯跳过
  $("lockBanner").style.display = lk ? "block" : "none";
  $("rightCol").classList.toggle("locked", lk);
  $("bombBtn").textContent = "释放真气炸弹(" + (state.cults.length ? me().bomb : 0) + ")";
  const locked = state.firstPick >= 0;
  $("form").disabled = locked;
  $("form").max = state.formMax;
  $("form").value = state.selectedForm + 1;
  $("sliderRow").classList.toggle("locked", locked);
  $("formLabel").textContent = "第" + (state.selectedForm + 1) + "式";
  $("hint").textContent = hintText();

  const list = $("list");
  state.families.forEach((f, i) => {
    const row = list.children[i];
    if (!row) return;
    const a = state.defs[actionIndexOf(i)];
    row.querySelector(".cost").textContent = a.point + "/" + a.power;
    row.querySelector(".stripe").style.background = ATTR_COLORS[a.attr];
    row.classList.toggle("picked", state.mode === "dual" && state.firstPickFamily === i);
    row.classList.toggle("disabled", lk);
  });
}
function buildList() {
  const list = $("list");
  list.innerHTML = "";
  state.families.forEach((f, i) => {
    const row = document.createElement("div");
    row.className = "fam";
    row.innerHTML = `<div class="stripe"></div><div class="nm"></div><div class="cost"></div>`;
    row.querySelector(".nm").textContent = f.name;
    row.addEventListener("click", () => onFamilyClick(i));
    row.addEventListener("mousemove", ev => showTip(i, ev));
    row.addEventListener("mouseleave", hideTip);
    list.appendChild(row);
  });
}

/* ---- 招式效果文字(悬停提示) ---- */
const pct = v => parseFloat((v * 100).toPrecision(3)) + "%";
function ratioText(effect) {
  const v = effect.ratios.slice(0, 7);
  const same = (from, to) => { for (let i = from + 1; i <= to; i++) if (Math.abs(v[i] - v[from]) > 1e-6) return false; return true; };
  if (same(0, 6)) return pct(v[0]);
  if (same(0, 5)) return pct(v[0]) + "，对方为五行属性时" + pct(v[6]);
  const labels = ["无", "金", "木", "水", "火", "土", "五行"];
  return "按对方招式属性" + v.map((x, i) => " " + labels[i] + pct(x)).join("");
}
function timedText(label, e) {
  let s = label + "：";
  if (e.needHit) s += "命中后";
  if (e.instant > 0) s += "立即" + e.instant + "点";
  if (e.continuous > 0 && e.rounds > 0) {
    if (e.instant > 0) s += "；";
    s += "每隔" + e.interval + "回合" + e.continuous + "点，共" + e.rounds + "次";
  }
  return s;
}
function effectLines(def) {
  const lines = [];
  for (const e of def.effects) {
    switch (e.type) {
      case E_PHYSICAL: lines.push("物理：" + ratioText(e) + "的伤害转为物理伤害(扣健康值)"); break;
      case E_PENETRATE: lines.push("穿透：对撞时，按较小威力的" + ratioText(e) + "造成额外伤害"); break;
      case E_REDUCE: lines.push("减伤：受到的伤害减少" + ratioText(e)); break;
      case E_REBOUND: lines.push("反弹：将受到伤害的" + ratioText(e) + "反弹给攻击者，自己只承受剩下的(减伤的升级版)"); break;
      case E_ABSORB: lines.push("吸收：将造成伤害的" + ratioText(e) + "转化为真气"); break;
      case E_LOCK: lines.push("禁锢：" + pct(e.prob) + "概率使对手下一回合无法行动"); break;
      case E_DOT: lines.push(timedText("持续伤害", e)); break;
      case E_REBATE: lines.push(timedText("回复真气", e)); break;
      case E_RECOVER: lines.push(timedText("回复武力", e)); break;
      case E_CURE: lines.push(timedText("回复健康", e)); break;
    }
  }
  return lines;
}
function showTip(family, ev) {
  const def = state.defs[actionIndexOf(family)];
  const lines = effectLines(def);
  const tip = $("tip");
  if (!lines.length) { tip.style.display = "none"; return; }
  tip.innerHTML = "";
  const t = document.createElement("div"); t.className = "t"; t.textContent = def.name; tip.appendChild(t);
  for (const l of lines) { const d = document.createElement("div"); d.textContent = l; tip.appendChild(d); }
  tip.style.display = "block";
  const w = tip.offsetWidth, h = tip.offsetHeight;
  let x = ev.clientX + 16, y = ev.clientY + 20;
  if (x + w > innerWidth) x = ev.clientX - 16 - w;
  if (y + h > innerHeight) y = ev.clientY - 12 - h;
  tip.style.left = Math.max(0, x) + "px"; tip.style.top = Math.max(0, y) + "px";
}
function hideTip() { $("tip").style.display = "none"; }

/* ---- 遮罩：匹配中/等待对手/对手已退出/断线 + 对局结束弹窗 ---- */
// 每种状态：[标题, 说明, 按钮1, 按钮2]；按钮 = [文字, 动作]，动作 home=回主界面(退出当前对局/匹配)，again=重新匹配(刷新页面)
const NET_TEXT = {
  connecting: ["正在连接服务器", "", ["返回主界面", "home"], null],
  waiting: ["正在匹配对手", "有人在等待时会立即配对，请稍候", ["取消匹配", "home"], null],
  submitted: ["等待对手行动", "你已出招，等对手出招后双方同时结算", null, null],
  oppquit: ["对手已退出", "", ["返回主界面", "home"], ["重新匹配", "again"]],
  closed: ["与服务器断开连接", "", ["返回主界面", "home"], ["重新连接", "again"]],
  local: ["联机对战需要在线访问", "现在是在本地直接打开的页面，联机只能用部署后的网址。人机对战和查看说明可以离线使用。", ["返回主界面", "home"], null],
};
function refreshOverlays() {
  const net = NET_TEXT[state.phase], netEl = $("netOverlay");
  netEl.classList.toggle("show", !!net);
  if (net) {
    const dots = state.phase === "connecting" || state.phase === "waiting" || state.phase === "submitted";
    $("netText").innerHTML = "";
    $("netText").appendChild(document.createTextNode(net[0]));
    if (dots) { const s = document.createElement("span"); s.className = "dots"; $("netText").appendChild(s); }
    $("netSub").textContent = net[1];
    for (const [id, b] of [["netBtn", net[2]], ["netBtn2", net[3]]]) {
      const el = $(id);
      el.style.display = b ? "" : "none";
      el.textContent = b ? b[0] : "";
      el.dataset.act = b ? b[1] : "";
    }
    hideTip();
  }
  const over = $("overlay");
  const showResult = state.phase === "result" || state.phase === "rematch";
  over.classList.toggle("show", showResult);
  over.classList.toggle("win", showResult && state.result > 0);
  over.classList.toggle("lose", showResult && state.result < 0);
  over.classList.toggle("draw", showResult && state.result === 0);
  if (showResult) {
    $("overlayText").textContent = state.phase === "rematch" ? "等待对方选择…" :
      state.result > 0 ? "挑战成功" : state.result < 0 ? "挑战失败" : "平局";
    $("againBtn").style.display = state.phase === "rematch" ? "none" : "";
  }
}
function refreshAll() {
  refreshStatus();
  refreshEnvironment();
  refreshActions();
  refreshOverlays();
}

/* ---- 事件 ---- */
function init() {
  loadActions(BUILTIN_ACTIONS);
  buildStatus(0); buildStatus(1);
  buildList();
  $("modeSingle").onclick = () => { state.mode = "single"; state.firstPick = -1; state.firstPickFamily = -1; refreshAll(); };
  $("modeDual").onclick = () => { state.mode = "dual"; state.firstPick = -1; state.firstPickFamily = -1; refreshAll(); };
  $("accBtn").onclick = () => submit({ t: "acc" });
  $("bombBtn").onclick = () => submit({ t: "bomb" });
  $("skipBtn").onclick = () => submit({ t: "skip" });
  $("form").oninput = ev => { state.selectedForm = parseInt(ev.target.value) - 1; refreshActions(); };
  $("againBtn").onclick = () => { if (state.phase !== "result") return; state.phase = "rematch"; send({ t: "again" }); refreshAll(); };
  $("quitBtn").onclick = () => goHome();   // 结束弹窗里的"退出"：离开对局，回主界面
  $("backBtn").onclick = () => {
    // 对局进行中离开会让对手"对手已退出"，先确认一下；匹配中/已结束直接走
    if ((state.phase === "select" || state.phase === "submitted") && !confirm("现在离开会结束这局，对手会看到“对手已退出”。确定返回主界面吗？")) return;
    goHome();
  };
  for (const id of ["netBtn", "netBtn2"]) $(id).onclick = ev => {
    if (ev.target.dataset.act === "home") goHome();
    else location.reload();            // 重新匹配/重新连接：刷新页面就会重新连接并匹配
  };
  connect();
}
init();

// 供本地测试页(tools/test_harness.html)读取状态、驱动出招
window.wxdjDebug = { state, submit, digest };
