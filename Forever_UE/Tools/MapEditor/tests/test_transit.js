// node tests/test_transit.js —— 独立线路：route id连线规则、切分继承、最短路、线路环(默认不停站/停站只对预设)、导出下标与警告、存读往返、旧模型迁移。
const assert = require('assert');
require('../js/terrain.js');
require('../js/io.js');
require('../js/roadnet.js');
require('../js/lots.js');
require('../js/greens.js');
require('../js/transit.js');
const ME = globalThis.ME;
const T = new ME.Terrain(256, 256);
const free = (x, y, z) => ({ x, y, z });

// ---- 颜色：稳定、不同id不同色、空id灰 ----
assert.strictEqual(ME.routeColor('route_train'), ME.routeColor('route_train'));
assert.notStrictEqual(ME.routeColor('route_train'), ME.routeColor('route_air'));
assert.strictEqual(ME.routeColor(''), '#9aa0a6');
assert.ok(/^#[0-9a-f]{6}$/.test(ME.routeColor('route_train')));

// ---- 连线规则 ----
{
	const m = ME.newModel(256, 256);
	// 面板未填id：空地起点 → 新线id为''
	const c0 = ME.addTConn(m, { a: free(10, 10, 0), b: free(50, 10, 0), type: 'ground', routeId: '' });
	assert.ok(c0.conn && c0.conn.routeId === '');
	// 给它填id(UI右栏做的事)
	c0.conn.routeId = 'route_train';
	const a = m.transit.nodes[0], b = m.transit.nodes[1];
	// 面板未填id：从已有id的端点出发 → 继承
	const c1 = ME.addTConn(m, { a: { node: b.id }, b: free(90, 10, 0), routeId: '' });
	assert.strictEqual(c1.conn.routeId, 'route_train');
	// 面板填了相同id：从线中间出发(切分)
	const c2 = ME.addTConn(m, { a: { conn: c0.conn.id, t: 0.5 }, b: free(30, 40, 0), routeId: 'route_train' });
	assert.ok(c2.conn, c2.error);
	assert.strictEqual(m.transit.conns.length, 4);   // c0被切成2段 + c1 + c2
	assert.ok(m.transit.conns.filter((c) => c.routeId === 'route_train').length === 4);
	// 面板填了别的id：从train线上出发被拒绝，且无副作用
	const before = JSON.stringify(m.transit);
	const bad = ME.addTConn(m, { a: { node: b.id }, b: free(120, 10, 0), routeId: 'route_air' });
	assert.ok(bad.error && /route_train/.test(bad.error));
	assert.strictEqual(JSON.stringify(m.transit), before);
	// 面板填别的id：空地起点可以，且不能终止在train线上
	const air = ME.addTConn(m, { a: free(10, 100, 5), b: free(60, 100, 5), routeId: 'route_air' });
	assert.strictEqual(air.conn.routeId, 'route_air');
	const mix = ME.addTConn(m, { a: { node: m.transit.nodes[m.transit.nodes.length - 1].id }, b: { node: a.id }, routeId: '' });
	assert.ok(mix.error, '两个不同id的端点不能连在一起');
	// 面板未填：起点空地，终点吸到已有id的线 → 新线采用终点的id
	const adopt = ME.addTConn(m, { a: free(150, 150, 0), b: { node: b.id }, routeId: '' });
	assert.strictEqual(adopt.conn.routeId, 'route_train');
	// 起点=终点
	assert.ok(ME.addTConn(m, { a: { node: b.id }, b: { node: b.id } }).error);
}

// ---- 改route id：同连通块里原id相同的连线一起改；会让节点混id时拒绝 ----
{
	const m = ME.newModel(256, 256);
	const c1 = ME.addTConn(m, { a: free(0, 0, 0), b: free(10, 0, 0), routeId: '' }).conn;
	const c2 = ME.addTConn(m, { a: { node: c1.b }, b: free(20, 0, 0), routeId: '' }).conn;
	const far = ME.addTConn(m, { a: free(0, 50, 0), b: free(10, 50, 0), routeId: '' }).conn;
	assert.strictEqual(ME.setTRouteId(m, c1.id, 'route_train').count, 2);
	assert.strictEqual(c1.routeId, 'route_train'); assert.strictEqual(c2.routeId, 'route_train'); assert.strictEqual(far.routeId, '');
	const other = ME.addTConn(m, { a: free(0, 100, 0), b: free(10, 100, 0), routeId: 'route_air' }).conn;
	// 把train那串线改成route_air：不相连所以允许；改成已连着别id的则拒绝
	const joined = ME.addTConn(m, { a: { node: c2.b }, b: free(30, 0, 0), routeId: 'route_train' }).conn;
	assert.strictEqual(ME.setTRouteId(m, joined.id, 'route_bus').count, 3);
	assert.ok(c1.routeId === 'route_bus' && joined.routeId === 'route_bus');
	// 环的routeId跟着改
	const loop = ME.addTLoop(m, [{ kind: 'node', ref: c1.a }, { kind: 'node', ref: joined.b }], '').loop;
	assert.strictEqual(loop.routeId, 'route_bus');
	ME.setTRouteId(m, c1.id, 'route_rail');
	assert.strictEqual(loop.routeId, 'route_rail');
	void other;
}

// ---- 切分：新节点高度取该处剖面，ctrl拆成两段，端点附近不切 ----
{
	const m = ME.newModel(256, 256);
	const r = ME.addTConn(m, { a: free(0, 0, 0), b: free(100, 0, 10), ctrl: [50, 30], routeId: 'route_air' });
	const c = r.conn;
	const n = ME.splitTConnAt(m, c.id, 0.5, { height: () => 0 });
	assert.ok(Math.abs(n.z - 5) < 1e-9, '扁S中点高度是头尾平均');
	assert.strictEqual(m.transit.conns.length, 2);
	const [c1, c2] = m.transit.conns;
	assert.ok(c1.ctrl && c2.ctrl);
	assert.strictEqual(c1.routeId, 'route_air'); assert.strictEqual(c2.routeId, 'route_air');
	assert.strictEqual(c1.b, n.id); assert.strictEqual(c2.a, n.id);
	// 总长不变(两段拼回原曲线)
	const L = ME.tConnLength(m, c1) + ME.tConnLength(m, c2);
	const orig = ME.newModel(256, 256);
	const o = ME.addTConn(orig, { a: free(0, 0, 0), b: free(100, 0, 10), ctrl: [50, 30], routeId: 'x' }).conn;
	assert.ok(Math.abs(L - ME.tConnLength(orig, o)) < 0.05);
	// 在端点附近切分返回端点
	assert.strictEqual(ME.splitTConnAt(m, c1.id, 0.0001, null).id, c1.a);
}

// ---- 拾取 ----
{
	const m = ME.newModel(256, 256);
	const c = ME.addTConn(m, { a: free(10, 10, 0), b: free(50, 10, 0), routeId: 'r' }).conn;
	assert.strictEqual(ME.pickTNode(m, 10.1, 10.1, 0.5).id, c.a);
	assert.strictEqual(ME.pickTNode(m, 30, 30, 0.5), null);
	const hit = ME.pickTConn(m, 30, 10.2, 0.5);
	assert.ok(hit && hit.conn.id === c.id && Math.abs(hit.t - 0.5) < 0.02);
}

// ---- 地图：一个带预设的地块 + 一条线路网 ----
function buildMap() {
	const m = ME.newModel(256, 256);
	const road = (a, b) => { const r = ME.addRoad(m, { a, b }); if (r.error) throw new Error(r.error); return r.road; };
	const at = (x, y) => { const n = ME.pickNode(m, x, y, 0.2); return n ? { node: n.id } : { x, y }; };
	const C = [[100, 100], [140, 100], [140, 140], [100, 140]];
	for (let i = 0; i < 4; i++) road(at(...C[i]), at(...C[(i + 1) % 4]));
	const nw = ME.pickNode(m, 100, 100, 0.2), ne = ME.pickNode(m, 140, 100, 0.2);
	const lot = ME.addLot(m, nw.id, ne.id, 40, 'AREA_RESIDENCE_HIGH');
	const p1 = ME.addPreset(m, lot, 2, { x0: 0, x1: 10, y0: 0, y1: 10 }); p1.modId = 'building_train_station';
	const p2 = ME.addPreset(m, lot, 3, { x0: 0, x1: 10, y0: 28, y1: 38 }); p2.modId = 'building_train_station';
	const p3 = ME.addPreset(m, lot, 3, { x0: 0, x1: 10, y0: 28, y1: 38 }); p3.modId = 'x';   // 和p2重叠 → 放置失败
	return { m, lot, p1, p2, p3 };
}

// ---- 最短路 + 环显示分段 ----
{
	const { m, p1, p2 } = buildMap();
	// 铁轨： A(20,20)-B(60,20)-C(60,60)，另一条 A-C 直连更短；D孤立
	const A = ME.addTConn(m, { a: free(20, 20, 0), b: free(60, 20, 0), routeId: 'route_train' }).conn;
	const nA = m.transit.nodes[0], nB = m.transit.nodes[1];
	const BC = ME.addTConn(m, { a: { node: nB.id }, b: free(60, 60, 0), routeId: 'route_train' }).conn;
	const nC = m.transit.nodes[2];
	const AC = ME.addTConn(m, { a: { node: nA.id }, b: { node: nC.id }, routeId: 'route_train' }).conn;
	const D = ME.addTConn(m, { a: free(200, 200, 0), b: free(220, 200, 0), routeId: 'route_air' }).conn;
	const nD = m.transit.nodes[3];
	const path = ME.tShortestPath(m, 'route_train', nA.id, nC.id);
	assert.strictEqual(path.length, 1); assert.strictEqual(path[0].conn.id, AC.id); assert.strictEqual(path[0].forward, true);
	const path2 = ME.tShortestPath(m, 'route_train', nC.id, nB.id);
	assert.strictEqual(path2.length, 1); assert.strictEqual(path2[0].conn.id, BC.id); assert.strictEqual(path2[0].forward, false);
	assert.strictEqual(ME.tShortestPath(m, 'route_train', nA.id, nD.id), null);
	assert.strictEqual(ME.tShortestPath(m, 'route_air', nA.id, nC.id), null, '不同id的线不能当路径');
	assert.deepStrictEqual(ME.tShortestPath(m, 'route_train', nA.id, nA.id), []);

	// 环：预设p1 → A → C → 预设p2。默认全不停站
	const lp = ME.addTLoop(m, [{ kind: 'preset', ref: p1.id }, { kind: 'node', ref: nA.id }, { kind: 'node', ref: nC.id }, { kind: 'preset', ref: p2.id }], '');
	assert.ok(lp.loop, lp.error);
	assert.strictEqual(lp.loop.routeId, 'route_train', 'id取自环里node所在的线路');
	assert.ok(lp.loop.items.every((it) => it.kind !== 'preset' || it.stop === false));
	const pres = ME.presetPlacements(m, ME.exportRoadnet(m, T).roadPieces);
	assert.ok(pres.has(p1.id) && pres.has(p2.id));
	assert.ok(!pres.has(m.lots[0].presets[2].id), '重叠的预设p3放置失败，不在列表里');
	const segs = ME.loopSegments(m, lp.loop, pres);
	assert.strictEqual(segs.length, 4);
	assert.deepStrictEqual(segs.map((s) => s.kind), ['dashed', 'path', 'dashed', 'dashed'], '预设↔node虚线；A→C沿线路；p2→p1两个预设虚线');
	assert.ok(segs[1].pts.length >= 2 && Math.hypot(segs[1].pts[0][0] - 20, segs[1].pts[0][1] - 20) < 1e-6);
	// 停站只对预设有效
	assert.ok(ME.setLoopStop(m, lp.loop.id, 0, true));
	assert.ok(!ME.setLoopStop(m, lp.loop.id, 1, true), 'node项不能设停站');
	assert.strictEqual(lp.loop.items[0].stop, true);
	assert.strictEqual(lp.loop.items[1].stop, undefined);
	// 环校验
	assert.ok(ME.addTLoop(m, [{ kind: 'node', ref: nA.id }], '').error, '少于2项');
	assert.ok(ME.addTLoop(m, [{ kind: 'node', ref: nA.id }, { kind: 'node', ref: nA.id }], '').error, '相邻重复项');
	assert.ok(ME.addTLoop(m, [{ kind: 'node', ref: nA.id }, { kind: 'node', ref: nC.id }, { kind: 'node', ref: nC.id }, { kind: 'node', ref: nB.id }], '').error, '中间相邻重复项');
	// 同一个点可以在环里出现多次(车站往返各停一次)；点回起点的末项自动去掉
	const twice = ME.addTLoop(m, [{ kind: 'preset', ref: p1.id }, { kind: 'node', ref: nA.id }, { kind: 'node', ref: nC.id }, { kind: 'node', ref: nA.id }, { kind: 'preset', ref: p1.id }], '');
	assert.ok(twice.loop, twice.error);
	assert.strictEqual(twice.loop.items.length, 4, '末项和首项相同=点回起点，去掉末项');
	const again = ME.addTLoop(m, [{ kind: 'preset', ref: p1.id }, { kind: 'node', ref: nA.id }, { kind: 'node', ref: nC.id }, { kind: 'node', ref: nA.id }, { kind: 'preset', ref: p2.id }, { kind: 'node', ref: nA.id }], '');
	assert.ok(again.loop, again.error);
	assert.strictEqual(again.loop.items.length, 6);
	assert.strictEqual(ME.addTLoop(m, [{ kind: 'preset', ref: p1.id }, { kind: 'preset', ref: p1.id }, { kind: 'preset', ref: p1.id }], 'route_train').error !== undefined, true, '全是同一个点');
	// 重复的预设各自独立设停站，导出成两项
	const pre = ME.presetPlacements(m, ME.exportRoadnet(m, T).roadPieces);
	const stationLoop = ME.addTLoop(m, [{ kind: 'preset', ref: p1.id }, { kind: 'node', ref: nA.id }, { kind: 'node', ref: nC.id }, { kind: 'node', ref: nA.id }, { kind: 'preset', ref: p1.id }, { kind: 'node', ref: nB.id }], '').loop;
	ME.setLoopStop(m, stationLoop.id, 0, true); ME.setLoopStop(m, stationLoop.id, 4, true);
	assert.deepStrictEqual(stationLoop.items.map((it) => it.stop), [true, undefined, undefined, undefined, true, undefined]);
	const exp = ME.exportRoadnet(m, T).output.transit.loops.find((l) => l.items.length === 6 && l.items[0].stop && l.items[4].stop);
	assert.ok(exp && exp.items[0].preset === exp.items[4].preset, '同一个预设导出成两项，下标相同');
	void pre;
	assert.ok(ME.addTLoop(m, [{ kind: 'node', ref: nA.id }, { kind: 'node', ref: nD.id }], '').error, '混id');
	assert.ok(ME.addTLoop(m, [{ kind: 'preset', ref: p1.id }, { kind: 'preset', ref: p2.id }], '').error, '纯预设环必须填id');
	assert.ok(ME.addTLoop(m, [{ kind: 'preset', ref: p1.id }, { kind: 'preset', ref: p2.id }], 'route_train').loop);

	// 删除：删node后环里引用它的项被移除
	const before = lp.loop.items.length;
	ME.deleteTNode(m, nC.id);
	assert.strictEqual(ME.tLoopOf(m, lp.loop.id).items.length, before - 1);
	// 再删一个node，环剩下<2项 → 环被删掉
	ME.deleteTNode(m, nA.id);
	assert.ok(!ME.tLoopOf(m, lp.loop.id) || ME.tLoopOf(m, lp.loop.id).items.length >= 2);
	assert.ok(!ME.tNodeOf(m, nA.id));
}

// ---- 导出：预设下标、跳过无效项、无停站警告 ----
{
	const { m, p1, p2, p3 } = buildMap();
	ME.addTConn(m, { a: free(20, 20, 0), b: free(60, 20, 2), ctrl: [40, 30], type: 'bridge', routeId: 'route_train' });
	const nA = m.transit.nodes[0], nB = m.transit.nodes[1];
	const l1 = ME.addTLoop(m, [{ kind: 'preset', ref: p2.id }, { kind: 'node', ref: nA.id }, { kind: 'node', ref: nB.id }, { kind: 'preset', ref: p1.id }], '').loop;
	ME.setLoopStop(m, l1.id, 0, true);
	ME.setLoopStop(m, l1.id, 3, true);
	// 一个含无效预设p3的环：该项被丢掉，其余保留
	const l2 = ME.addTLoop(m, [{ kind: 'preset', ref: p3.id }, { kind: 'node', ref: nA.id }, { kind: 'preset', ref: p1.id }], '').loop;
	ME.setLoopStop(m, l2.id, 2, true);
	// 没id的conn：导出跳过并警告
	ME.addTConn(m, { a: free(150, 150, 0), b: free(160, 150, 0), routeId: '' });
	const { output, warnings } = ME.exportRoadnet(m, T);
	const tr = output.transit;
	assert.ok(tr);
	assert.strictEqual(tr.conns.length, 1);
	assert.deepStrictEqual(tr.conns[0], { from: 0, to: 1, ctrl: [40, 30], kind: 'bridge', route: 'route_train' });
	assert.ok(warnings.some((w) => w.includes('没有填route id')));
	assert.ok(warnings.some((w) => w.includes('预设') && w.includes('无效')));
	assert.strictEqual(output.presets.length, 2, 'p3放置失败不在output.presets里');
	// 环1：预设下标=output.presets下标(p2→1? 按lot里预设顺序p1=0,p2=1)
	const idx = (p) => m.lots[0].presets.filter((q) => q.id <= p.id && q !== p3).length - 1;
	assert.deepStrictEqual(tr.loops[0], { route: 'route_train', items: [{ preset: idx(p2), stop: true }, { node: 0 }, { node: 1 }, { preset: idx(p1), stop: true }] });
	assert.strictEqual(tr.loops[1].items.length, 2);
	assert.ok(!('presetIndexById' in JSON.parse(JSON.stringify(output))), '辅助下标不写进文件');
	// 无停站警告
	ME.setLoopStop(m, l1.id, 0, false); ME.setLoopStop(m, l1.id, 3, false);
	const w2 = ME.exportRoadnet(m, T).warnings;
	assert.ok(w2.some((w) => w.includes('没有设置停站')));
}

// ---- 没有transit数据时不输出output.transit；旧模型迁移补默认值 ----
{
	const m = ME.newModel(256, 256);
	assert.ok(!('transit' in ME.exportRoadnet(m, T).output));
	const old = { version: 2, mapW: 256, mapH: 256, nodes: [], roads: [], lots: [], nextId: 1 };
	ME.migrateModel(old);
	assert.deepStrictEqual(old.transit, { nodes: [], conns: [], loops: [], roadLines: [] });
	assert.ok(ME.cloneModel(m).transit);
}

// ---- 存读往返 ----
{
	const { m, p1, p2 } = buildMap();
	ME.addTConn(m, { a: free(20, 20, 3), b: free(60, 20, 3), routeId: 'route_air', type: 'tunnel' });
	const l = ME.addTLoop(m, [{ kind: 'preset', ref: p1.id }, { kind: 'node', ref: m.transit.nodes[0].id }, { kind: 'preset', ref: p2.id }], '').loop;
	ME.setLoopStop(m, l.id, 0, true);
	const { output } = ME.exportRoadnet(m, T);
	const back = ME.readRoadnet(ME.writeRoadnet(m, output));
	assert.deepStrictEqual(back.source.transit, JSON.parse(JSON.stringify(m.transit)));
	assert.deepStrictEqual(back.output.transit, JSON.parse(JSON.stringify(output.transit)));
	ME.migrateModel(back.source);
	const again = ME.exportRoadnet(back.source, T).output.transit;
	assert.deepStrictEqual(again, JSON.parse(JSON.stringify(output.transit)));
}

// ---- 城外连接点(extern)：边界上建，导出带extern标记，拖动贴边界，不在边界上警告 ----
{
	const m = ME.newModel(256, 256);
	const c = ME.addTConn(m, { a: { x: 0, y: 100, z: 0, extern: true }, b: free(60, 100, 0), type: 'ground', routeId: 'route_train' });
	assert.ok(c.conn, c.error);
	const ext = ME.tNodeOf(m, c.conn.a);
	assert.strictEqual(ext.extern, true);
	assert.ok(!ME.tNodeOf(m, c.conn.b).extern);
	// 拖动：贴到最近的边界
	ME.moveTNode(m, ext.id, 5, 120);
	assert.deepStrictEqual([ext.x, ext.y], [0, 120]);
	ME.moveTNode(m, ext.id, 100, 250);
	assert.deepStrictEqual([ext.x, ext.y], [100, 256]);
	// 非extern节点不受影响
	const plain = ME.tNodeOf(m, c.conn.b);
	ME.moveTNode(m, plain.id, 70, 50);
	assert.deepStrictEqual([plain.x, plain.y], [70, 50]);
	const { output, warnings } = ME.exportRoadnet(m, T);
	assert.strictEqual(output.transit.nodes.filter((n) => n.extern).length, 1);
	assert.ok(!warnings.some((w) => /边界/.test(w)));
	ext.x = 50; ext.y = 50;
	assert.ok(ME.exportRoadnet(m, T).warnings.some((w) => /不在地图边界/.test(w)));
}

console.log('test_transit OK');
