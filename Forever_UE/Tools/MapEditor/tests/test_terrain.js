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
b.beginStroke(); b.brush('paint', 7, 7, 1, { terrain: 'rock' }); b.endStroke();
assert.ok(b.countNonBuildable(5, 5, 9, 9) > 0);

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

// 限定单一地形(only)：抬高/降低/抹平/平滑只改这个地形的格子，平滑也只看同地形邻格。
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
	// 平滑：A格子的邻格里有B时不被B拉动(B整体抬高100也不影响A的平均)
	const p = new ME.Terrain(16, 16);
	p.beginStroke(); p.brush('paint', 4, 8, 2.5, { terrain: 'A' }); p.brush('paint', 10, 8, 3.5, { terrain: 'B' }); p.endStroke();
	for (let i = 0; i < p.idx.length; i++) p.height[i] = p.nameAt(i) === 'B' ? 100 : 0;
	p.beginStroke(); p.brush('smooth', 4, 8, 6, { strength: 1, only: p.paletteIndex('A') }); p.endStroke();
	for (let i = 0; i < p.idx.length; i++) if (p.nameAt(i) === 'A') assert.ok(Math.abs(p.height[i]) < 1e-6, 'A cell ' + i + ' h=' + p.height[i]);
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
console.log('test_terrain OK');
