// node tests/make_presets.js [输出目录] —— 生成预设一致性测试用的地图：平地上一个40x40方框路 + 一块地 + 4个预设(含子地块里的、一个无效id)。
// 输出 presets.terrain / presets.roadnet / presets_expect.json(编辑器模拟器算出的期望：每个预设放置后的世界坐标矩形)，供 CoreTest 的 presets 套件对照。
const fs = require('fs');
const path = require('path');
require('../js/terrain.js');
require('../js/io.js');
require('../js/roadnet.js');
require('../js/lots.js');
const ME = globalThis.ME;
const outDir = process.argv[2] || path.join(__dirname, '..', 'samples');
fs.mkdirSync(outDir, { recursive: true });

const t = new ME.Terrain(256, 256);
const m = ME.newModel(256, 256);
const road = (a, b) => { const r = ME.addRoad(m, { a, b }); if (r.error) throw new Error(r.error); return r.road; };
const at = (x, y) => { const n = ME.pickNode(m, x, y, 0.2); return n ? { node: n.id } : { x, y }; };
const C = [[100, 100], [140, 100], [140, 140], [100, 140]];
for (let i = 0; i < 4; i++) road(at(...C[i]), at(...C[(i + 1) % 4]));
road({ x: 0, y: 140, extern: true }, at(100, 140));   // 城外连接
const nw = ME.pickNode(m, 100, 100, 0.2), ne = ME.pickNode(m, 140, 100, 0.2);
const lot = ME.addLot(m, nw.id, ne.id, 40, 'AREA_RESIDENCE_HIGH');
const add = (dir, rect, id) => { const p = ME.addPreset(m, lot, dir, rect); p.modId = id; };
add(2, { x0: 0, x1: 10, y0: 0, y1: 10 }, 'zone_residence_high');
add(3, { x0: 0, x1: 10, y0: 28, y1: 38 }, 'zone_residence_high');
add(1, { x0: 28, x1: 38, y0: 0, y1: 10 }, 'zone_residence_high');
add(3, { x0: 11, x1: 21, y0: 28, y1: 38 }, 'zone_not_exist');

const { output, warnings } = ME.exportRoadnet(m, t);
if (warnings.length) console.log('warnings:', warnings);
const pieces = ME.exportRoadnet(m, t).roadPieces;
const rep = ME.replayPresets(m, lot, pieces);
const expect = rep.steps.map((s, i) => {
	if (!s.placed) throw new Error('preset ' + i + ' invalid: ' + s.error);
	const [x, y] = ME.localToWorld(rep.frame, s.placed.x0, s.placed.y0);
	const [x2, y2] = ME.localToWorld(rep.frame, s.placed.x1, s.placed.y1);
	return { id: lot.presets[i].modId, x: Math.min(x, x2), y: Math.min(y, y2), w: Math.abs(x2 - x), h: Math.abs(y2 - y) };
});
fs.writeFileSync(path.join(outDir, 'presets.terrain'), Buffer.from(ME.writeTerrain(t)));
fs.writeFileSync(path.join(outDir, 'presets.roadnet'), ME.writeRoadnet(m, output));
fs.writeFileSync(path.join(outDir, 'presets_expect.json'), JSON.stringify(expect, null, 1));
console.log('presets', output.presets.length, JSON.stringify(expect));
