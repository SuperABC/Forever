// node tests/test_presets.js —— 园区/建筑预设：lot本地坐标系、free pool模拟器(对应Core的RequestPlacement)、吸附、导出。
const assert = require('assert');
require('../js/terrain.js');
require('../js/io.js');
require('../js/roadnet.js');
require('../js/lots.js');
const ME = globalThis.ME;
const free = (x, y) => ({ x, y });
const near = (a, b, eps = 1e-6) => assert.ok(Math.abs(a - b) < eps, `${a} != ${b}`);

// 40x40的方框路，一整块地(四边都有路，缩进=半路宽1)：内矩形 38x38，原点=内矩形西北角(101,101)。
function box() {
	const m = ME.newModel(256, 256);
	const corners = [[100, 100], [140, 100], [140, 140], [100, 140]];
	for (let i = 0; i < 4; i++) {
		const a = corners[i], b = corners[(i + 1) % 4];
		const prev = m.nodes.find((n) => n.x === a[0] && n.y === a[1]);
		const next = m.nodes.find((n) => n.x === b[0] && n.y === b[1]);
		ME.addRoad(m, { a: prev ? { node: prev.id } : free(a[0], a[1]), b: next ? { node: next.id } : free(b[0], b[1]) });
	}
	const nw = m.nodes.find((n) => n.x === 100 && n.y === 100), ne = m.nodes.find((n) => n.x === 140 && n.y === 100);
	const lot = ME.addLot(m, nw.id, ne.id, 40, 'AREA_COMMERCE_HIGH');
	return { m, lot };
}
const pieces = (m) => ME.exportRoadnet(m, new ME.Terrain(256, 256)).roadPieces;

// ---- 本地坐标系 ----
{
	const { m, lot } = box();
	const f = ME.lotFrame(m, lot, pieces(m));
	near(f.W, 38); near(f.H, 38);
	assert.deepStrictEqual(f.road, [true, true, true, true]);
	near(f.origin[0], 101); near(f.origin[1], 101);
	near(f.ex[0], 1); near(f.ex[1], 0); near(f.ey[0], 0); near(f.ey[1], 1);   // x向东，y向南
	const w = ME.localToWorld(f, 10, 5);
	near(w[0], 111); near(w[1], 106);
	const l = ME.worldToLocal(f, 111, 106);
	near(l[0], 10); near(l[1], 5);
}

// ---- pool模拟器：贴北边的角落预设 ----
{
	const { m, lot } = box();
	const f = ME.lotFrame(m, lot, pieces(m));
	const pool0 = [{ x0: 0, y0: 0, x1: f.W, y1: f.H, road: f.road.slice() }];
	const r = ME.placePresetRect(pool0, { x0: 0, x1: 10, y0: 0, y1: 10 }, 2);
	assert.ok(!r.error, r.error);
	// 沿路方向右侧切一刀小路(x:10..11)，深度方向背后有路→普通切分(无小路)：占位块=10x10，剩下两块
	assert.deepStrictEqual([r.placed.x0, r.placed.y0, r.placed.x1, r.placed.y1], [0, 0, 10, 10]);
	assert.strictEqual(r.pool.length, 2);
	const strip = r.pool.find((c) => c.x0 > 10), rest = r.pool.find((c) => c.x0 === 0);
	near(strip.x0, 11); near(strip.x1, 38); near(strip.y0, 0); near(strip.y1, 38);
	assert.deepStrictEqual(strip.road, [true, true, true, true]);   // 西边是新小路
	near(rest.y0, 10); near(rest.y1, 38); near(rest.x1, 10);
	assert.deepStrictEqual(rest.road, [true, true, false, true]);   // 北边是刚放的预设(没路)，东边是小路

	// 第二个预设落在剩下的块里：贴南边的角
	const r2 = ME.placePresetRect(r.pool, { x0: 0, x1: 10, y0: 28, y1: 38 }, 3);
	assert.ok(!r2.error, r2.error);

	// 跨小路的矩形：不在任何一块自由地内
	const bad = ME.placePresetRect(r.pool, { x0: 5, x1: 20, y0: 15, y1: 25 }, 0);
	assert.ok(bad.error && /同一块自由地/.test(bad.error));

	// 北边没路的块(rest)：贴北边放→没路
	const noRoad = ME.placePresetRect(r.pool, { x0: 0, x1: 10, y0: 10, y1: 20 }, 2);
	assert.ok(noRoad.error && /没有路/.test(noRoad.error));
}

// ---- 拉伸：目标离边不到minGap(=3)时贴到边 ----
{
	const { m, lot } = box();
	const f = ME.lotFrame(m, lot, pieces(m));
	const pool0 = [{ x0: 0, y0: 0, x1: f.W, y1: f.H, road: f.road.slice() }];
	// 北边放 x:1..37、深度10：左右空隙1<3，不切小路，整条边都占
	const r = ME.placePresetRect(pool0, { x0: 1, x1: 37, y0: 0, y1: 10 }, 2);
	assert.ok(!r.error, r.error);
	assert.deepStrictEqual([r.placed.x0, r.placed.x1], [0, 38]);
	assert.strictEqual(r.pool.length, 1);   // 只剩深度方向背后的一块
}

// ---- 居中：沿路两端都要切小路 ----
{
	const { m, lot } = box();
	const f = ME.lotFrame(m, lot, pieces(m));
	const pool0 = [{ x0: 0, y0: 0, x1: f.W, y1: f.H, road: f.road.slice() }];
	const r = ME.placePresetRect(pool0, { x0: 10, x1: 28, y0: 0, y1: 12 }, 2);
	assert.ok(!r.error, r.error);
	assert.deepStrictEqual([r.placed.x0, r.placed.x1, r.placed.y0, r.placed.y1], [10, 28, 0, 12]);
	assert.strictEqual(r.pool.length, 3);   // 左右两条+深度背后一块
}

// ---- 吸附 ----
{
	assert.strictEqual(ME.snapValue(1, 0, 38), 0);        // 离起点<3 → 吸到起点
	assert.strictEqual(ME.snapValue(36, 0, 38), 38);      // 离终点<3 → 吸到终点
	assert.strictEqual(ME.snapValue(10.3, 0, 38), 10.5);  // 其余按0.5取整
	assert.strictEqual(ME.snapValue(-5, 0, 38), 0);
	const sub = { x0: 11, y0: 0, x1: 38, y1: 38, road: [true, true, true, true] };
	assert.deepStrictEqual(ME.edgeSpan(sub, 2), { lo: 11, hi: 38, depthMax: 38 });
	assert.deepStrictEqual(ME.edgeSpan(sub, 0), { lo: 0, hi: 38, depthMax: 27 });
	assert.deepStrictEqual(ME.rectFromEdge(sub, 3, 12, 20, 6), { y0: 32, y1: 38, x0: 12, x1: 20 });
	assert.strictEqual(ME.nearestRoadEdge({ x0: 0, y0: 0, x1: 10, y1: 10, road: [false, true, false, true] }, 1, 1), 1);
	assert.strictEqual(ME.nearestRoadEdge({ x0: 0, y0: 0, x1: 10, y1: 10, road: [false, false, false, false] }, 1, 1), -1);
}

// ---- 重放 + 导出 ----
{
	const { m, lot } = box();
	const p1 = ME.addPreset(m, lot, 2, { x0: 0, x1: 10, y0: 0, y1: 10 });
	p1.modId = 'zone_xx';
	const p2 = ME.addPreset(m, lot, 3, { x0: 0, x1: 10, y0: 28, y1: 38 });
	const t = new ME.Terrain(256, 256);
	let res = ME.exportRoadnet(m, t);
	assert.strictEqual(res.output.presets.length, 2);
	assert.deepStrictEqual(res.output.presets[0], { lot: 0, id: 'zone_xx', direction: 2, rect: [0, 10, 0, 10] });
	assert.ok(res.warnings.some((w) => w.includes('还没有填id')));   // p2没填id

	// 无效预设(矩形跨小路)：警告并不导出
	ME.addPreset(m, lot, 2, { x0: 5, x1: 20, y0: 15, y1: 25 });
	res = ME.exportRoadnet(m, t);
	assert.strictEqual(res.output.presets.length, 2);
	assert.ok(res.warnings.some((w) => w.includes('无效')));

	// lot在output.lots里的下标要按实际导出顺序：先插一个无效lot(节点缺失)，预设的lot下标仍指向它自己的位置
	const bad = ME.addLot(m, 99999, 99998, 10, 'AREA_TEST');
	m.lots.unshift(m.lots.pop());   // 坏lot排第一
	res = ME.exportRoadnet(m, t);
	assert.strictEqual(res.output.lots.length, 1);
	assert.strictEqual(res.output.presets[0].lot, 0);
	assert.ok(bad);

	// migrate：旧模型没有presets字段
	const old = JSON.parse(JSON.stringify(m));
	for (const l of old.lots) delete l.presets;
	ME.migrateModel(old);
	assert.ok(old.lots.every((l) => Array.isArray(l.presets)));
}

console.log('test_presets OK');
