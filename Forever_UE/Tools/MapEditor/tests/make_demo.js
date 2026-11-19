// node tests/make_demo.js [输出目录] —— 用编辑器的纯逻辑生成一份演示地图(256x256)：山、湖、平地路网+地块、隧道、桥、爬升、城外连接点。
// 用来验证游戏侧 terrain_file / roadnet_file 能读(RoadnetViz/CoreTest)。输出 demo.terrain 和 demo.roadnet。
const fs = require('fs');
const path = require('path');
require('../js/terrain.js');
require('../js/io.js');
require('../js/roadnet.js');
require('../js/lots.js');
const ME = globalThis.ME;

const outDir = process.argv[2] || path.join(__dirname, '..', 'samples');
fs.mkdirSync(outDir, { recursive: true });

const W = 256, H = 256;
const t = new ME.Terrain(W, H);
// 山：中心(110,120)，半径28；高度抬到约6。
t.beginStroke(); t.brush('paint', 110, 120, 28, { terrain: 'terrain_mountain' }); t.endStroke();
for (let i = 0; i < 12; i++) { t.beginStroke(); t.brush('raise', 110, 120, 26, { strength: 1 }); t.endStroke(); }
// 湖：中心(200,120)，半径14，压低到-2，水位-0.1。
t.beginStroke(); t.brush('paint', 200, 120, 14, { terrain: 'terrain_ocean' }); t.endStroke();
t.beginStroke(); t.brush('flatten', 200, 120, 14, { target: -2, strength: 1 }); t.endStroke();
t.beginStroke(); t.brush('water', 200, 120, 14, { level: ME.SEA_LEVEL }); t.endStroke();
// 自定义地形字符串：沙漠，平原性质之外的一块。
t.beginStroke(); t.brush('paint', 40, 200, 10, { terrain: 'desert' }); t.endStroke();

const m = ME.newModel(W, H);
const ctx = { height: (x, y) => t.sampleHeight(x, y) };
const at = (x, y) => {
	const n = ME.pickNode(m, x, y, 0.2);
	if (n) return { node: n.id };
	const hit = ME.pickRoad(m, x, y, 0.2);
	if (hit && ME.canSplit(hit.road)) return { road: hit.road.id, t: hit.t };
	return { x, y };
};
const road = (a, b, extra) => {
	const r = ME.addRoad(m, Object.assign({ a, b }, extra || {}), ctx);
	if (r.error) throw new Error(r.error);
	return r.road;
};

// 平地路网：外围一圈 + 中间十字，西边连到城外。
road({ x: 0, y: 60, extern: true }, { x: 40, y: 60 });
road(at(40, 60), at(70, 60));
road({ x: 40, y: 30 }, { x: 40, y: 90 });
road({ x: 70, y: 30 }, at(70, 60));        // T口接到 H1 末端节点
road(at(70, 60), at(70, 90));
road(at(40, 90), at(70, 90));
road(at(40, 30), at(70, 30));
// 4车道的大路(曲线)：从(70,60)往东北弯出去。
road(at(70, 60), { x: 110, y: 40 }, { ctrl: [90, 60], lanes: [2, 0, 1, 2, 0, 1] });
// 隧道穿山：y=120。(60,120)z0 -隧道口(下坡)-> (72,120)z-1 -隧道-> (148,120)z-1 -隧道口(上坡)-> (160,120)z0；两端有平地路连着。
road({ x: 20, y: 120 }, { x: 60, y: 120 });
road(at(60, 120), { x: 72, y: 120, z: -1 }, { type: 'mouth', lanes: [1, 0, 1, 1, 0, 1] });
road(at(72, 120), { x: 148, y: 120, z: -1 }, { type: 'tunnel', lanes: [1, 0, 1, 1, 0, 1] });
road(at(148, 120), { x: 160, y: 120, z: 0 }, { type: 'mouth', lanes: [1, 0, 1, 1, 0, 1] });
road(at(160, 120), { x: 182, y: 120 });
// 桥：跨湖，三段(上坡、桥面、下坡)组成一条桥墩链，桥面高1.2。
road(at(182, 120), { x: 194, y: 120, z: 1.2 }, { type: 'bridge' });
road(at(194, 120), { x: 206, y: 120, z: 1.2 }, { type: 'bridge' });
road({ x: 218, y: 120, z: 0 }, at(206, 120), { type: 'bridge' }); // 故意反着画：验证桥墩链的reverse
road(at(218, 120), { x: 256, y: 120, extern: true });
// 贴合路：山的南坡，y=150，两端节点高度取地形高度，中间跟着地形翻过去。
road({ x: 70, y: 150, z: ctx.height(70, 150) }, { x: 150, y: 150, z: ctx.height(150, 150) }, { type: 'hug' });
road({ x: 30, y: 150 }, at(70, 150));
road(at(150, 150), { x: 175, y: 150 });
// 地块：中间小方框的南面，深度30。
const nodeAt = (x, y) => ME.pickNode(m, x, y, 0.2);
const a = nodeAt(40, 60), b = nodeAt(70, 60);
ME.addLot(m, a.id, b.id, 30, 'AREA_RESIDENCE_HIGH');
ME.addLot(m, nodeAt(40, 30).id, nodeAt(70, 30).id, 30, 'AREA_COMMERCE_HIGH');

const { output, warnings } = ME.exportRoadnet(m, t);
fs.writeFileSync(path.join(outDir, 'demo.terrain'), Buffer.from(ME.writeTerrain(t)));
fs.writeFileSync(path.join(outDir, 'demo.roadnet'), ME.writeRoadnet(m, output));
console.log(`roads(user)=${m.roads.length} roads(out)=${output.roads.length} nodes=${output.nodes.length} lots=${output.lots.length}`);
console.log('warnings:', warnings);
