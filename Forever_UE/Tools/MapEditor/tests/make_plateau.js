// node tests/make_plateau.js [输出目录] —— 生成"非0高度地块"测试用的地图：256x256，(120,120)附近抹平出一块高度H=3的平台，
// 平台上一个40x40方框路(节点z=H) + 一块居住地块(corners带z=H)，再从平台西南角接一条"贴合"路下到城外连接点(z=0)。
// 输出 plateau.terrain / plateau.roadnet，供 CoreTest 的 plateau 套件(检查建筑/园区/导航节点的地坪高度)。
const fs = require('fs');
const path = require('path');
require('../js/terrain.js');
require('../js/io.js');
require('../js/roadnet.js');
require('../js/lots.js');
const ME = globalThis.ME;
const outDir = process.argv[2] || path.join(__dirname, '..', 'samples');
fs.mkdirSync(outDir, { recursive: true });

const H = 3;
const t = new ME.Terrain(256, 256);
// 抹平平台：半径60足够把方框路和地块(100..140)连同3x3邻域都抹成同一高度；平台外是渐变的缓坡(抹平笔刷的衰减)，再外面是z=0的平地。
t.beginStroke(); t.brush('flatten', 120, 120, 60, { strength: 1, target: H, allowFlat: true }); t.endStroke();

const m = ME.newModel(256, 256);
const C = [[100, 100], [140, 100], [140, 140], [100, 140]];
const ids = C.map((p) => ME.addNode(m, p[0], p[1], false, H).id);
for (let i = 0; i < 4; i++) {
	const r = ME.addRoad(m, { a: { node: ids[i] }, b: { node: ids[(i + 1) % 4] } });
	if (r.error) throw new Error(r.error);
}
// 下山的路：地形贴合路面(fit，路面取节点高度，游戏里把地形削平到路面)，两端高度分别是平台高度和z=0(城外连接点)。
const down = ME.addRoad(m, { a: { x: 0, y: 140, z: 0, extern: true }, b: { node: ids[3] }, type: 'fit' });
if (down.error) throw new Error(down.error);
ME.addLot(m, ids[0], ids[1], 40, 'AREA_RESIDENCE_HIGH');

const { output, warnings } = ME.exportRoadnet(m, t);
if (warnings.length) console.log('warnings:', warnings);
fs.writeFileSync(path.join(outDir, 'plateau.terrain'), Buffer.from(ME.writeTerrain(t)));
fs.writeFileSync(path.join(outDir, 'plateau.roadnet'), ME.writeRoadnet(m, output));
console.log('plateau', output.roads.length, 'roads,', output.lots.length, 'lots, corners', JSON.stringify(output.lots[0].corners));
