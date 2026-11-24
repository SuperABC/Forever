// node tests/test_greens.js —— 绿化区：闭环/自交检查、凹多边形、连路解析(路被切开后仍能找回)、导出逆时针与边号重映射、旧模型迁移。
const assert = require('assert');
require('../js/terrain.js');
require('../js/io.js');
require('../js/roadnet.js');
require('../js/lots.js');
require('../js/greens.js');
const ME = globalThis.ME;
const free = (x, y) => ({ x, y });
const t = new ME.Terrain(256, 256);

// ---- 几何检查 ----
const square = [[10, 10], [30, 10], [30, 30], [10, 30]];
assert.ok(ME.greenSignedArea(square) > 0);
assert.ok(ME.canCloseGreen(square));
assert.ok(!ME.ringSelfIntersects(square));
// 领结形自交
const bowtie = [[10, 10], [30, 30], [30, 10], [10, 30]];
assert.ok(ME.ringSelfIntersects(bowtie));
assert.ok(!ME.canCloseGreen(bowtie));
// 凹多边形(L形)可以
const ell = [[0, 0], [20, 0], [20, 10], [10, 10], [10, 30], [0, 30]];
assert.ok(ME.canCloseGreen(ell));
assert.ok(ME.pointInRing(5, 25, ell));
assert.ok(!ME.pointInRing(15, 25, ell)); // 凹口里面
// 画的过程中：新边不能穿过已画的边
assert.ok(ME.canAppendVertex([[10, 10], [30, 10], [30, 30]], [10, 30]));
assert.ok(!ME.canAppendVertex([[10, 10], [30, 10], [30, 30], [10, 30]], [20, 0])); // (10,30)->(20,0)穿过(10,10)-(30,10)? 与首边相交
// 不能折回去压住上一条边
assert.ok(!ME.canAppendVertex([[0, 0], [10, 0]], [5, 0]));
// 同一点重复
assert.ok(!ME.canAppendVertex([[0, 0], [10, 0]], [10, 0]));

// ---- 导出：顺时针输入 -> 逆时针输出，边号映射 ----
{
	const m = ME.newModel(256, 256);
	// 一条横向路 y=50, x 0..100
	ME.addRoad(m, { a: free(0, 50), b: free(100, 50) });
	const road = m.roads[0];
	// 绿化在路下方(+y)：顶点顺序按"屏幕上顺时针"(面积为负)：(20,52)->(20,70)->(60,70)->(60,52)
	// 边0=(20,52)->(20,70) 竖边；边3=(60,52)->(20,52) 贴着路的上边。
	const g = ME.addGreen(m, [[20, 52], [20, 70], [60, 70], [60, 52]]);
	assert.ok(ME.greenSignedArea(g.pts) < 0);
	const hit = ME.pickRoad(m, 40, 50, 0.2);
	ME.setGreenLink(g, 3, hit.road, hit.t, hit.x, hit.y);
	const { output, warnings } = ME.exportRoadnet(m, t);
	assert.strictEqual(output.greens.length, 1);
	const og = output.greens[0];
	assert.strictEqual(og.points.length, 4);
	assert.ok(ME.greenSignedArea(og.points) > 0, '输出必须是有符号面积>0');
	assert.strictEqual(og.links.length, 1);
	assert.strictEqual(og.links[0].road, 0);
	// 反转后边3(60,52)->(20,52)变成 (20,52)->(60,52)，仍然是顶点连续两点 y=52 的那条
	const e = og.links[0].edge, a = og.points[e], b = og.points[(e + 1) % 4];
	assert.ok(a[1] === 52 && b[1] === 52, '连路的边必须还是贴着路的那条：' + JSON.stringify([a, b]));
	assert.ok(!warnings.some((w) => w.includes('绿化')), JSON.stringify(warnings));
	assert.strictEqual(road.id, m.roads[0].id);
}

// ---- 路被双击切开后：连接仍能找回，并落到靠近落点的那一半 ----
{
	const m = ME.newModel(256, 256);
	ME.addRoad(m, { a: free(0, 50), b: free(100, 50) });
	const g = ME.addGreen(m, [[70, 52], [90, 52], [90, 70], [70, 70]]);
	const hit = ME.pickRoad(m, 80, 50, 0.2);
	ME.setGreenLink(g, 0, hit.road, hit.t, hit.x, hit.y);
	// 在x=50处切开：落点x=80在右半。
	const road = m.roads[0];
	const n = ME.splitRoadAt(m, road.id, 0.5, undefined, { height: () => 0 });
	assert.ok(n);
	assert.strictEqual(m.roads.length, 2);
	const out = ME.exportRoadnet(m, t);
	assert.strictEqual(out.output.greens[0].links.length, 1);
	const linked = out.output.roads[out.output.greens[0].links[0].road];
	const right = m.roads.find((r) => ME.nodeOf(m, r.a).x === 50);
	assert.strictEqual(linked.from, 'n' + right.a, '应连到右半段');
	// 路删光：连接被忽略并警告
	ME.deleteRoad(m, m.roads[0].id); ME.deleteRoad(m, m.roads[0].id);
	const out2 = ME.exportRoadnet(m, t);
	assert.strictEqual(out2.output.greens[0].links.length, 0);
	assert.ok(out2.warnings.some((w) => w.includes('已不存在')));
}

// ---- 贴合(hug)路拆成多段：连接选落点所在那一段 ----
{
	const m = ME.newModel(256, 256);
	const hill = new ME.Terrain(256, 256);
	hill.beginStroke(); hill.brush('paint', 35, 100, 12, { terrain: 'terrain_mountain' }); hill.endStroke();
	for (let i = 0; i < 8; i++) { hill.beginStroke(); hill.brush('raise', 35, 100, 12, { strength: 1 }); hill.endStroke(); }
	ME.addRoad(m, { a: free(10, 100), b: free(60, 100), type: 'hug' }, { height: (x, y) => hill.sampleHeight(x, y) });
	const g = ME.addGreen(m, [[40, 102], [50, 102], [50, 110], [40, 110]]);
	const hit = ME.pickRoad(m, 45.5, 100, 0.2);
	ME.setGreenLink(g, 0, hit.road, hit.t, hit.x, hit.y);
	const out = ME.exportRoadnet(m, hill);
	assert.ok(out.output.roads.length > 10, 'hug road should split to many pieces');
	const idx = out.output.greens[0].links[0].road;
	const piece = out.output.roads[idx];
	const nodeX = (id) => out.output.nodes.find((q) => q.id === id).x;
	const x0 = nodeX(piece.from), x1 = nodeX(piece.to);
	assert.ok(Math.min(x0, x1) - 0.01 <= 45.5 && 45.5 <= Math.max(x0, x1) + 0.01, `piece x range ${x0}..${x1} must contain 45.5`);
}

// ---- 自交/退化多边形导出时被跳过 ----
{
	const m = ME.newModel(256, 256);
	ME.addGreen(m, bowtie);
	ME.addGreen(m, [[1, 1], [2, 2]]);
	const out = ME.exportRoadnet(m, t);
	assert.strictEqual(out.output.greens.length, 0);
	assert.strictEqual(out.warnings.filter((w) => w.includes('绿化')).length, 2);
}

// ---- 旧模型(没有greens字段)迁移后补上空数组；cloneModel保留greens ----
{
	const old = { version: 2, mapW: 64, mapH: 64, nodes: [], roads: [], lots: [], nextId: 1 };
	const m = ME.migrateModel(old);
	assert.deepStrictEqual(m.greens, []);
	ME.addGreen(m, square);
	const c = ME.cloneModel(m);
	assert.strictEqual(c.greens.length, 1);
	assert.deepStrictEqual(ME.exportRoadnet(c, t).output.greens[0].points.length, 4);
	// 保存读回
	const text = ME.writeRoadnet(m, ME.exportRoadnet(m, t).output);
	const back = ME.readRoadnet(text);
	assert.strictEqual(back.source.greens.length, 1);
	assert.strictEqual(back.output.greens.length, 1);
}

// ---- 选边 / 选区域 ----
{
	const m = ME.newModel(256, 256);
	const g = ME.addGreen(m, square);
	const e = ME.pickGreenEdge(m, 20, 10.1, 0.5);
	assert.ok(e && e.green.id === g.id && e.edge === 0);
	assert.ok(!ME.pickGreenEdge(m, 20, 20, 0.5));
	assert.strictEqual(ME.pickGreen(m, 20, 20).id, g.id);
	assert.strictEqual(ME.pickGreen(m, 50, 50), null);
	ME.deleteGreen(m, g.id);
	assert.strictEqual(m.greens.length, 0);
}

// green id(modId)：默认''，export成output.greens[].id；旧模型(没有modId字段)导出也是''；克隆后保留。
{
	const m = ME.newModel(256, 256);
	const a = ME.addGreen(m, [[10, 10], [30, 10], [30, 30], [10, 30]]);
	const b = ME.addGreen(m, [[50, 10], [70, 10], [70, 30], [50, 30]], 'green_park');
	assert.strictEqual(a.modId, '');
	assert.strictEqual(b.modId, 'green_park');
	delete a.modId;   // 旧文件
	const out = ME.exportRoadnet(ME.cloneModel(m), t).output.greens;
	assert.strictEqual(out.length, 2);
	assert.strictEqual(out[0].id, '');
	assert.strictEqual(out[1].id, 'green_park');
}
console.log('test_greens OK');
