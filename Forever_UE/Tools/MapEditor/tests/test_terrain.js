// node tests/test_terrain.js —— 地形笔刷/约束/读写往返。
const assert = require('assert');
require('../js/terrain.js');
require('../js/io.js');
const ME = globalThis.ME;

const t = new ME.Terrain(64, 64);

// 平原格上抬高无效。
t.beginStroke();
assert.strictEqual(t.brush('raise', 10, 10, 5, { strength: 1 }), 0);
assert.strictEqual(t.endStroke(), null);

// 刷山后抬高有效，平原仍是0。
t.beginStroke();
assert.ok(t.brush('paint', 20, 20, 6, { terrain: 'terrain_mountain' }) > 0);
const paintStroke = t.endStroke();
assert.ok(paintStroke.indices.length > 0);
t.beginStroke();
assert.ok(t.brush('raise', 20, 20, 6, { strength: 2 }) > 0);
const raiseStroke = t.endStroke();
assert.ok(t.height[20 * 64 + 20] > 1);
assert.strictEqual(t.height[10 * 64 + 10], 0);
assert.ok(t.sampleHeight(20.5, 20.5) > 0.5);

// 抹平到目标。
t.beginStroke();
t.brush('flatten', 20, 20, 6, { target: 3, strength: 1 });
t.endStroke();
assert.ok(Math.abs(t.height[20 * 64 + 20] - 3) < 0.7);

// 水可以放在任意格子(含平原：岸边平原格也要有水面)，erase 清除。
t.beginStroke();
assert.ok(t.brush('water', 10, 10, 3, { level: -0.1 }) > 0);
assert.ok(!Number.isNaN(t.water[10 * 64 + 10]) && t.height[10 * 64 + 10] === 0);
t.brush('water', 10, 10, 3, { erase: true });
assert.ok(t.brush('water', 20, 20, 3, { level: -0.1 }) > 0);
t.endStroke();
assert.strictEqual(t.water[20 * 64 + 20], Math.fround(-0.1));
t.beginStroke();
t.brush('water', 20, 20, 3, { erase: true });
t.endStroke();
assert.ok(Number.isNaN(t.water[20 * 64 + 20]));

// 刷回plain清高度。
t.beginStroke();
t.brush('paint', 20, 20, 6, { terrain: 'plain' });
t.endStroke();
assert.strictEqual(t.height[20 * 64 + 20], 0);

// 撤销/重做。
const u = new ME.Terrain(16, 16);
u.beginStroke();
u.brush('paint', 8, 8, 3, { terrain: 'lake' });
const stroke = u.endStroke();
const painted = u.idx.slice();
u.applyCells(stroke.indices, stroke.before);
assert.ok(u.idx.every((v) => v === 0));
u.applyCells(stroke.indices, stroke.after);
assert.deepStrictEqual(Array.from(u.idx), Array.from(painted));

// 读写往返(含中文/长字符串调色板)。
const r = new ME.Terrain(32, 16);
r.beginStroke(); r.brush('paint', 8, 8, 4, { terrain: 'terrain_ocean' }); r.brush('paint', 20, 8, 3, { terrain: '沙漠_desert' }); r.endStroke();
r.beginStroke(); r.brush('raise', 20, 8, 3, { strength: 1.5 }); r.brush('water', 8, 8, 4, { level: -0.1 }); r.endStroke();
const buffer = ME.writeTerrain(r);
const back = ME.readTerrain(buffer);
assert.strictEqual(back.w, 32); assert.strictEqual(back.h, 16);
assert.deepStrictEqual(back.palette, r.palette);
assert.deepStrictEqual(Array.from(back.idx), Array.from(r.idx));
assert.deepStrictEqual(Array.from(back.height), Array.from(r.height));
for (let i = 0; i < r.water.length; i++) assert.ok(Object.is(back.water[i], r.water[i]) || back.water[i] === r.water[i]);

// 3x3邻域可建造检查。
const b = new ME.Terrain(32, 32);
assert.strictEqual(b.countNonBuildable(5, 5, 9, 9), 0);
// 可建设只看高度、不看地形类型：高度为0的rock格仍可建，抬高后才不可建。
b.beginStroke(); b.brush('paint', 7, 7, 1, { terrain: 'rock' }); b.endStroke();
assert.strictEqual(b.countNonBuildable(5, 5, 9, 9), 0);
b.beginStroke(); b.brush('raise', 7, 7, 1, { strength: 1 }); b.endStroke();
assert.ok(b.height[7 * 32 + 7] > 0);
assert.ok(b.countNonBuildable(5, 5, 9, 9) > 0);
// isBuildable：抬高的格子及其3x3邻域都不可建；离得够远的仍可建；地图边缘一圈永远不可建。
assert.strictEqual(b.isBuildable(7, 7), false);
assert.strictEqual(b.isBuildable(8, 8), false);
assert.strictEqual(b.isBuildable(10, 7), true);
assert.strictEqual(b.isBuildable(0, 5), false);
assert.strictEqual(b.isBuildable(-1, 5), false);
assert.strictEqual(b.isBuildable(16, 16), true);

// 抹平到0必须能把格子精确清成0：抬高再压低留下的浮点残留、1e-7级的小高度、指数逼近的尾巴都要吸附成0(游戏的可建设判断要求高度精确为0)。
{
	const f = new ME.Terrain(32, 32);
	f.beginStroke(); f.brush('raise', 16.5, 16.5, 6, { strength: 0.37, allowFlat: true }); f.endStroke();
	f.beginStroke(); f.brush('lower', 16.5, 16.5, 6, { strength: 0.37, allowFlat: true }); f.endStroke();
	assert.ok(f.height.every((v) => v === 0), '抬高再压低同样的量不应留下残留');
	f.height[16 * 32 + 16] = 3e-7; f.height[17 * 32 + 16] = 0.0004;
	f.beginStroke(); f.brush('flatten', 16.5, 16.5, 8, { strength: 1, target: 0, allowFlat: true }); f.endStroke();
	assert.strictEqual(f.height[16 * 32 + 16], 0);
	assert.strictEqual(f.height[17 * 32 + 16], 0);
	assert.ok(f.isBuildable(16, 16));
	// 平缓抬高后用中等强度反复抹平，笔刷范围内最终全部精确为0
	for (let y = 10; y < 22; y++) for (let x = 10; x < 22; x++) f.height[y * 32 + x] = 0.8;
	for (let k = 0; k < 30; k++) { f.beginStroke(); f.brush('flatten', 16, 16, 10, { strength: 0.5, target: 0, allowFlat: true }); f.endStroke(); }
	assert.ok(f.isBuildable(16, 16));
	assert.strictEqual(f.height[16 * 32 + 16], 0);
}

// 平原格上有水：刷成plain不清水，只清高度；enforceFlat不动水；地块检查不看水。
{
	const w = new ME.Terrain(32, 32);
	w.beginStroke(); w.brush('water', 16, 16, 2, { level: -0.1 }); w.endStroke();
	w.beginStroke(); w.brush('paint', 16, 16, 2, { terrain: 'plain' }); w.endStroke();
	assert.ok(!Number.isNaN(w.water[16 * 32 + 16]));
	w.enforceFlat();
	assert.ok(!Number.isNaN(w.water[16 * 32 + 16]));
	assert.strictEqual(w.countNonBuildable(12, 12, 20, 20), 0);
	assert.strictEqual(w.countNonBuildable(2, 2, 6, 6), 0);
}

// 限定单一地形(only)：抬高/降低/抹平/平滑只改这个地形的格子，平滑时交界邻格也参与平均。
{
	const o = new ME.Terrain(32, 32);
	o.beginStroke(); o.brush('paint', 12, 16, 4, { terrain: 'A' }); o.brush('paint', 20, 16, 4, { terrain: 'B' }); o.endStroke();
	const ia = o.paletteIndex('A'), ib = o.paletteIndex('B');
	const cellA = 16 * 32 + 11, cellB = 16 * 32 + 21;
	o.beginStroke(); o.brush('raise', 16, 16, 8, { strength: 1, only: ia }); o.endStroke();
	assert.ok(o.height[cellA] > 0);
	assert.strictEqual(o.height[cellB], 0);
	o.beginStroke(); o.brush('raise', 16, 16, 8, { strength: 1 }); o.endStroke();
	assert.ok(o.height[cellB] > 0);
	// 抹平到0只作用于B
	const hA = o.height[cellA];
	o.beginStroke(); o.brush('flatten', 16, 16, 8, { target: 0, strength: 1, only: ib }); o.endStroke();
	assert.strictEqual(o.height[cellA], hA);
	assert.ok(o.height[cellB] < 0.5);
	// 平滑(限定地形)：邻格里的别的地形/平原也参与平均，交界处被拉向邻格高度，悬崖越抹越矮而不是越高。
	const q = new ME.Terrain(32, 32);
	q.beginStroke(); q.brush('paint', 16, 16, 8, { terrain: 'M' }); q.endStroke();
	const im = q.paletteIndex('M');
	for (let i = 0; i < q.idx.length; i++) q.height[i] = q.idx[i] === im ? 10 : 0;
	const edge = 16 * 32 + 8, mid = 16 * 32 + 12; // 山体左缘(紧挨平原)和中间
	assert.strictEqual(q.nameAt(edge), 'M');
	assert.ok(q.height[edge] === 10 && q.nameAt(edge - 1) === 'plain');
	let prev = q.height[edge];
	for (let k = 0; k < 4; k++) {
		q.beginStroke(); q.brush('smooth', 16, 16, 12, { strength: 1, only: im }); q.endStroke();
		assert.ok(q.height[edge] < prev, 'edge must drop, pass ' + k + ' h=' + q.height[edge]);
		prev = q.height[edge];
	}
	assert.ok(q.height[edge] < 8 && q.height[mid] > q.height[edge]); // 从中间到边缘单调下降成坡
	assert.ok(q.idx.every((v, i) => v === im || q.height[i] === 0)); // 平原格没被动
}

// 等高线：一座圆锥山，主等高线(每5条)标记，线段端点落在对应高度上。
{
	const c = new ME.Terrain(40, 40);
	c.beginStroke(); c.brush('paint', 20, 20, 12, { terrain: 'hill' }); c.endStroke();
	for (let y = 0; y < 40; y++) for (let x = 0; x < 40; x++) if (c.nameAt(y * 40 + x) === 'hill') c.height[y * 40 + x] = Math.max(0, 10 - Math.hypot(x - 20, y - 20));
	const res = ME.contourSegments(c, 1);
	assert.ok(res.segs.length > 0 && res.segs.length % 5 === 0);
	for (let i = 0; i < res.segs.length; i += 5) {
		const hmid = c.sampleHeight((res.segs[i] + res.segs[i + 2]) / 2, (res.segs[i + 1] + res.segs[i + 3]) / 2);
		const level = Math.round(hmid);
		assert.ok(Math.abs(hmid - level) < 0.35, 'level ' + hmid);
		assert.strictEqual(res.segs[i + 4], Math.round(hmid) % 5 === 0 ? 1 : 0);
	}
	assert.strictEqual(ME.contourSegments(new ME.Terrain(8, 8), 1).segs.length, 0);
	assert.ok(ME.contourSegments(c, 0.001).interval > 0.001); // 层数过多自动加大间距
}

// 平滑强度>1：多遍平滑，比强度1抹得更平。
{
	const mk = () => { const t = new ME.Terrain(32, 32); t.beginStroke(); t.brush('paint', 16, 16, 12, { terrain: 'm' }); t.height[16 * 32 + 16] = 10; t.endStroke(); return t; };
	const a = mk(), b = mk();
	a.beginStroke(); a.brush('smooth', 16, 16, 6, { strength: 1 }); a.endStroke();
	b.beginStroke(); b.brush('smooth', 16, 16, 6, { strength: 3 }); b.endStroke();
	assert.ok(b.height[16 * 32 + 16] < a.height[16 * 32 + 16] && a.height[16 * 32 + 16] < 10);
}
// 允许修改平地：默认平原格高度不动，allowFlat打开后可改；被抬高的平原格地块检查不合格；导出读回不被清零。
{
	const f = new ME.Terrain(32, 32);
	f.beginStroke(); f.brush('raise', 16, 16, 6, { strength: 1 }); f.endStroke();
	assert.strictEqual(f.height[16 * 32 + 16], 0);
	assert.strictEqual(f.countNonBuildable(12, 12, 20, 20), 0);
	f.beginStroke(); f.brush('raise', 16, 16, 6, { strength: 1, allowFlat: true }); f.endStroke();
	assert.ok(f.height[16 * 32 + 16] > 0.5);
	assert.ok(f.countNonBuildable(12, 12, 20, 20) > 0);
	const back = ME.readTerrain(ME.writeTerrain(f));
	assert.ok(Math.abs(back.height[16 * 32 + 16] - f.height[16 * 32 + 16]) < 1e-6);
}
// 一键平滑高度：山体/海底朝平原缓降，平原不动，宽度以外不动。
{
	const t = new ME.Terrain(64, 64);
	t.beginStroke(); t.brush('paint', 32, 32, 20, { terrain: 'm' }); t.endStroke();
	const im = t.paletteIndex('m');
	for (let i = 0; i < t.idx.length; i++) t.height[i] = t.idx[i] === im ? 10 : 0;
	const edge = 32 * 64 + 12, mid = 32 * 64 + 32; // 山左缘(紧邻平原)和山心
	assert.strictEqual(t.nameAt(edge), 'm'); assert.strictEqual(t.nameAt(edge - 1), 'plain');
	t.beginStroke(); const n = t.relaxFlatBoundary(4); t.endStroke();
	assert.ok(n > 0);
	assert.ok(t.height[edge] < 2, 'edge near 0, got ' + t.height[edge]);
	assert.ok(t.height[edge + 1] > t.height[edge] && t.height[edge + 3] > t.height[edge + 1]); // 向内单调升
	assert.strictEqual(t.height[mid], 10); // 宽度以外不动
	assert.ok(t.idx.every((v, i) => v === im || t.height[i] === 0)); // 平原不动
	// 海底(负高度)同样朝0缓降
	const s2 = new ME.Terrain(64, 64);
	s2.beginStroke(); s2.brush('paint', 32, 32, 20, { terrain: 'sea' }); s2.endStroke();
	for (let i = 0; i < s2.idx.length; i++) s2.height[i] = s2.nameAt(i) === 'sea' ? -6 : 0;
	s2.beginStroke(); s2.relaxFlatBoundary(4); s2.endStroke();
	assert.ok(s2.height[32 * 64 + 12] > -2 && s2.height[32 * 64 + 32] === -6);
	// 没有平原：不动
	const full = new ME.Terrain(8, 8); full.palette = ['plain', 'm']; full.idx.fill(1); full.height.fill(5);
	assert.strictEqual(full.relaxFlatBoundary(4), 0);
	// 撤销
	const a = (() => { const q = new ME.Terrain(16, 16); q.beginStroke(); q.brush('paint', 8, 8, 5, { terrain: 'm' }); q.endStroke(); for (let i = 0; i < 256; i++) if (q.nameAt(i) === 'm') q.height[i] = 8; q.beginStroke(); q.relaxFlatBoundary(3); const act = q.endStroke(); q.applyCells(act.indices, act.before); return q; })();
	assert.ok(a.idx.every((v, i) => a.nameAt(i) !== 'm' || a.height[i] === 8));
}

// 一键平滑类型：毛刺/突角被去掉，大块不变，任何类型字符串都行。
{
	const t = new ME.Terrain(32, 32);
	t.beginStroke(); t.brush('paint', 16, 16, 9, { terrain: 'rock' }); t.endStroke();
	const ir = t.paletteIndex('rock');
	// 平原里的1格毛刺
	t.idx[3 * 32 + 3] = ir; t.height[3 * 32 + 3] = 4;
	// 岩石里的1格洞(别的类型)
	t.idx[16 * 32 + 16] = 0;
	const before = Array.from(t.idx).filter((v) => v === ir).length;
	t.beginStroke(); const n = t.smoothTypes(1); t.endStroke();
	assert.ok(n >= 2);
	assert.strictEqual(t.idx[3 * 32 + 3], 0); assert.strictEqual(t.height[3 * 32 + 3], 0); // 毛刺没了，变平原高度清0
	assert.strictEqual(t.idx[16 * 32 + 16], ir); // 洞补上了
	assert.ok(Array.from(t.idx).filter((v) => v === ir).length > before - 40); // 大块基本保留
	// 再跑一遍已经平滑的：改动很少
	t.beginStroke(); const n2 = t.smoothTypes(1); t.endStroke();
	assert.ok(n2 < n + 10);
	// 两种非平原类型之间的毛刺也去掉
	const u = new ME.Terrain(16, 16); u.palette = ['plain', 'a', 'b']; u.idx.fill(1);
	u.idx[8 * 16 + 8] = 2; u.height.fill(3);
	assert.ok(u.smoothTypes(1) >= 1); assert.strictEqual(u.idx[8 * 16 + 8], 1);
}
console.log('test_terrain OK');
