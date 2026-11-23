// node tests/test_lots.js —— 地块：方向标签、边界road、margin、导出、可建造检查。
const assert = require('assert');
require('../js/terrain.js');
require('../js/io.js');
require('../js/roadnet.js');
require('../js/lots.js');
const ME = globalThis.ME;
const free = (x, y) => ({ x, y });

const m = ME.newModel(256, 256);
// 一个20x20的方框路 + 外圈
const corners = [[100, 100], [140, 100], [140, 140], [100, 140]];
for (let i = 0; i < 4; i++) {
	const a = corners[i], b = corners[(i + 1) % 4];
	const prev = m.nodes.find((n) => n.x === a[0] && n.y === a[1]);
	const next = m.nodes.find((n) => n.x === b[0] && n.y === b[1]);
	ME.addRoad(m, { a: prev ? { node: prev.id } : free(a[0], a[1]), b: next ? { node: next.id } : free(b[0], b[1]) });
}
assert.strictEqual(m.roads.length, 4);
const nw = m.nodes.find((n) => n.x === 100 && n.y === 100);
const ne = m.nodes.find((n) => n.x === 140 && n.y === 100);
// 往南(+y)深度40：A=nw,B=ne，法线 (-dy,dx)/len*depth = (0, 1)*depth → 向南为正。
const lot = ME.addLot(m, nw.id, ne.id, 30, 'AREA_COMMERCE_HIGH');

const t = new ME.Terrain(256, 256);
const { output, warnings } = ME.exportRoadnet(m, t);
assert.strictEqual(output.lots.length, 1);
const out = output.lots[0];
// 北边(2)是nw-ne的那条路；其余三边没有路。
// 轴对齐时：南边(3)=第一条(nw-ne)路在北侧还是南侧由几何决定，这里 A-B 在最北，所以是北(2)。
const north = out.boundary['2'];
assert.ok(north !== undefined, 'north boundary missing: ' + JSON.stringify(out.boundary));
assert.strictEqual(out.margin[2], 1); // 2车道+2人行道=4条*0.5=宽2，半宽1
assert.strictEqual(out.margin[0], 0);
assert.strictEqual(out.margin[1], 0);
assert.strictEqual(out.margin[3], 0);
assert.ok(!warnings.some((w) => w.includes('不是可建设区域')));

// 自定义缩进覆盖自动值。
lot.margin = [1, null, 0.25, null];
const o2 = ME.exportRoadnet(m, t).output.lots[0];
assert.strictEqual(o2.margin[0], 1);
assert.strictEqual(o2.margin[2], 0.25);

// 可建设只看高度：高度为0的山地形不报警，抬高之后的地块报警。
t.beginStroke(); t.brush('paint', 120, 115, 5, { terrain: 'terrain_mountain' }); t.endStroke();
assert.ok(!ME.exportRoadnet(m, t).warnings.some((w) => w.includes('不是可建设区域')));
t.beginStroke(); t.brush('raise', 120, 115, 5, { strength: 1 }); t.endStroke();
assert.ok(ME.exportRoadnet(m, t).warnings.some((w) => w.includes('不是可建设区域')));
// 占位分区不检查可建设性：同一块地改成AREA_PLACEHOLDER就不再报警，仍然导出。
const oldArea = lot.area; lot.area = 'AREA_PLACEHOLDER';
{ const r = ME.exportRoadnet(m, t); assert.ok(!r.warnings.some((w) => w.includes('不是可建设区域'))); assert.strictEqual(r.output.lots[0].area, 'AREA_PLACEHOLDER'); }
lot.area = oldArea;

// 旋转后的方向标签仍是四个互不相同的边。
const m2 = ME.newModel(256, 256);
const a = ME.addNode(m2, 100, 100), b = ME.addNode(m2, 120, 112);
const l2 = ME.addLot(m2, a.id, b.id, 15, 'AREA_TEST');
const c2 = ME.lotCorners(m2, l2);
const o = ME.orderCorners(c2);
assert.strictEqual(new Set(Object.values(o.edges).map((e) => JSON.stringify(e))).size, 4);


// 非0高度地块：地块z=两个基准节点z的均值，corners带第3个数；路必须水平且和地块等高才算边界路；基准节点高度不一致报警告；平台上可建设性只看极差。
{
	const H = 3.7;
	const m3 = ME.newModel(256, 256);
	const pts = [[100, 100], [140, 100], [140, 140], [100, 140]];
	const ids = pts.map((p) => ME.addNode(m3, p[0], p[1], false, H).id);
	for (let i = 0; i < 4; i++) ME.addRoad(m3, { a: { node: ids[i] }, b: { node: ids[(i + 1) % 4] } });
	assert.strictEqual(m3.roads.length, 4);
	const lot3 = ME.addLot(m3, ids[0], ids[1], 40, 'AREA_RESIDENCE_HIGH');
	assert.strictEqual(ME.lotZ(m3, lot3), H);
	assert.ok(ME.lotCorners(m3, lot3).every((c) => c.length === 3 && c[2] === H));
	const t3 = new ME.Terrain(256, 256);
	t3.beginStroke(); t3.brush('flatten', 120, 120, 60, { strength: 1, target: H, allowFlat: true }); t3.endStroke();
	const r3 = ME.exportRoadnet(m3, t3);
	assert.ok(!r3.warnings.some((w) => w.includes('不是可建设区域')), '抹平到H的平台上可建: ' + r3.warnings.join('|'));
	assert.ok(!r3.warnings.some((w) => w.includes('基准节点高度不一致')));
	const o3 = r3.output.lots[0];
	assert.ok(o3.corners.every((c) => c.length === 3 && c[2] === H), JSON.stringify(o3.corners));
	assert.ok(o3.boundary['2'] !== undefined && o3.boundary['0'] !== undefined && o3.boundary['1'] !== undefined && o3.boundary['3'] !== undefined, '四边都有同高的路');
	// 没抹平的地方(z=0平地的地形)：平台高度和地形没关系，但地形不平(斜坡)就报警
	const slope = new ME.Terrain(256, 256);
	for (let y = 0; y < 256; y++) for (let x = 0; x < 256; x++) slope.height[y * 256 + x] = x * 0.01;
	assert.ok(ME.exportRoadnet(m3, slope).warnings.some((w) => w.includes('不是可建设区域')));
	// 其中一条路抬高0.5：不水平的路不是边界路；整条南边路(和地块不同高)也不是
	m3.nodes.find((n) => n.id === ids[2]).z = H + 0.5;
	const o4 = ME.exportRoadnet(m3, t3).output.lots[0];
	assert.ok(o4.boundary['1'] === undefined && o4.boundary['3'] === undefined, '连着抬高节点的路(不水平)不算边界: ' + JSON.stringify(o4.boundary));
	assert.ok(o4.boundary['2'] !== undefined && o4.boundary['0'] !== undefined, '另外两条水平且等高的路仍是边界');
	// 路水平但和地块不同高：南边两个节点都抬高0.5(南边路水平但z=H+0.5)，东西路不水平，只有基准边(北边路)同高
	m3.nodes.find((n) => n.id === ids[3]).z = H + 0.5;
	const o5 = ME.exportRoadnet(m3, t3).output.lots[0];
	assert.deepStrictEqual(Object.keys(o5.boundary), ['2'], '只有和地块同高的北边路是边界: ' + JSON.stringify(o5.boundary));
	// 基准节点高度不一致 -> 警告，z取均值
	const m4 = ME.newModel(256, 256);
	const p = ME.addNode(m4, 100, 100, false, 2), q = ME.addNode(m4, 120, 100, false, 2.5);
	const l4 = ME.addLot(m4, p.id, q.id, 10, 'AREA_TEST');
	assert.strictEqual(ME.lotZ(m4, l4), 2.25);
	assert.ok(ME.exportRoadnet(m4, new ME.Terrain(256, 256)).warnings.some((w) => w.includes('基准节点高度不一致')));
	// 容差：z差4e-6不算不一致，也不影响边界判断
	q.z = 2 + 4e-6;
	assert.ok(!ME.exportRoadnet(m4, new ME.Terrain(256, 256)).warnings.some((w) => w.includes('基准节点高度不一致')));
}
console.log('test_lots OK');
