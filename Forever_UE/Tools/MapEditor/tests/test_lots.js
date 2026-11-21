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

console.log('test_lots OK');
