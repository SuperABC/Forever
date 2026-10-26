// 联机对战的匹配与转发逻辑(纯逻辑，不依赖 Cloudflare)：Durable Object(src/index.js)和本地测试页(tools/test_harness.html)共用这一份。
// 服务端不含任何战斗规则：只配对、攒齐双方出招后同时下发、处理重开/退出/断线，并核对双方每回合的状态摘要。
//
// 连接对象 conn 对服务端是不透明的，由适配器提供：
//   env.all()            -> 当前所有仍然连着的 conn
//   env.getAtt(conn)     -> 读这个连接的附加数据(对象，可为空)
//   env.setAtt(conn, o)  -> 写附加数据(Durable Object 休眠后附加数据仍保留，所以对局状态都放在这里)
//   env.send(conn, obj)  -> 发 JSON 消息
//   env.close(conn)      -> 关闭连接
//   env.rand()           -> [0,1) 随机数
//
// 连接状态 att = { state: 'waiting' | 'playing' | 'done', matchId, seat, act, again, hashes }
//
// 协议(JSON)：
//   客户端 -> 服务端  act{a} / again / quit / sync{r,h} / ping
//   服务端 -> 客户端  waiting / matched{seat,seed} / round{acts:[座位0的招,座位1的招]} / restart{seed} / opp_quit / desync{r}

const ACT_TYPES = new Set(['skip', 'acc', 'bomb', 'single', 'dual']);

function newSeed(env) { return Math.floor(env.rand() * 4294967296) >>> 0; }
function newId(env) { return Math.floor(env.rand() * 4294967296).toString(16) + Math.floor(env.rand() * 4294967296).toString(16); }

function att(env, conn) { return env.getAtt(conn) || {}; }
function patch(env, conn, p) { env.setAtt(conn, Object.assign({}, att(env, conn), p)); }

function findOpponent(env, conn) {
  const a = att(env, conn);
  if (!a.matchId) return null;
  for (const c of env.all()) {
    if (c === conn) continue;
    const b = att(env, c);
    if (b.matchId === a.matchId) return c;
  }
  return null;
}

// 出招描述只收固定格式，防止对端收到乱七八糟的东西：{t:'skip'|'acc'|'bomb'} / {t:'single',i} / {t:'dual',i,j}
function cleanAct(a) {
  if (!a || typeof a !== 'object' || !ACT_TYPES.has(a.t)) return null;
  const idx = v => Number.isInteger(v) && v >= 0 && v < 100000;
  if (a.t === 'single') return idx(a.i) ? { t: 'single', i: a.i } : null;
  if (a.t === 'dual') return idx(a.i) && idx(a.j) ? { t: 'dual', i: a.i, j: a.j } : null;
  return { t: a.t };
}

export function onOpen(env, conn) {
  let waiter = null;
  for (const c of env.all()) {
    if (c !== conn && att(env, c).state === 'waiting') { waiter = c; break; }
  }
  if (!waiter) {
    env.setAtt(conn, { state: 'waiting' });
    env.send(conn, { t: 'waiting' });
    return;
  }
  const matchId = newId(env), seed = newSeed(env);
  env.setAtt(waiter, { state: 'playing', matchId, seat: 0, act: null, again: false, hashes: {} });
  env.setAtt(conn, { state: 'playing', matchId, seat: 1, act: null, again: false, hashes: {} });
  env.send(waiter, { t: 'matched', seat: 0, seed });
  env.send(conn, { t: 'matched', seat: 1, seed });
}

export function onMessage(env, conn, msg) {
  if (!msg || typeof msg !== 'object') return;
  const a = att(env, conn);
  if (msg.t === 'ping') { env.send(conn, { t: 'pong' }); return; }
  if (a.state !== 'playing') return;

  if (msg.t === 'act') {
    const act = cleanAct(msg.a);
    if (!act || a.act) return;                    // 非法出招/这一回合已经出过了：忽略
    patch(env, conn, { act });
    const opp = findOpponent(env, conn);
    if (!opp) return;
    const b = att(env, opp);
    if (!b.act) return;                           // 对手还没出：先扣着，不给对手看
    const acts = [null, null];
    acts[a.seat] = act; acts[b.seat] = b.act;
    patch(env, conn, { act: null });
    patch(env, opp, { act: null });
    env.send(conn, { t: 'round', acts });
    env.send(opp, { t: 'round', acts });
    return;
  }

  if (msg.t === 'again') {
    patch(env, conn, { again: true, act: null });
    const opp = findOpponent(env, conn);
    if (!opp || !att(env, opp).again) return;     // 等对方也选重新开始
    const seed = newSeed(env);
    patch(env, conn, { again: false, act: null, hashes: {} });
    patch(env, opp, { again: false, act: null, hashes: {} });
    env.send(conn, { t: 'restart', seed });
    env.send(opp, { t: 'restart', seed });
    return;
  }

  if (msg.t === 'sync') {                          // 双方各自结算后的状态摘要，不一致说明两边算得不同(只做诊断)
    if (typeof msg.h !== 'string' || msg.h.length > 200 || !Number.isInteger(msg.r)) return;
    const hashes = Object.assign({}, a.hashes || {});
    hashes[msg.r] = msg.h;
    patch(env, conn, { hashes });
    const opp = findOpponent(env, conn);
    if (!opp) return;
    const other = (att(env, opp).hashes || {})[msg.r];
    if (other !== undefined && other !== msg.h) {
      env.send(conn, { t: 'desync', r: msg.r });
      env.send(opp, { t: 'desync', r: msg.r });
    }
    return;
  }

  if (msg.t === 'quit') {
    leave(env, conn);
    env.close(conn);
  }
}

export function onClose(env, conn) {
  leave(env, conn);
}

// 离开(主动退出或断线)：通知对手"对手已退出"，并让对手的连接从这局里脱离
function leave(env, conn) {
  const a = att(env, conn);
  if (a.state === 'playing') {
    const opp = findOpponent(env, conn);
    env.setAtt(conn, { state: 'done' });
    if (opp) {
      env.setAtt(opp, { state: 'done' });
      env.send(opp, { t: 'opp_quit' });
    }
  } else {
    env.setAtt(conn, { state: 'done' });
  }
}
