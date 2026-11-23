// node tests/make_transit.js [输出目录] —— 生成线路(transit)测试用的地图：平地上一个40x40方框路 + 一块地 + 2个火车站预设 + 一条铁轨(route_train) + 一个线路环。
// 输出 transit.terrain / transit.roadnet，供 CoreTest 的 transit 套件(见 Source/Test/README.md)读取；同时校验导出里的下标/停站。
const fs = require('fs');
const path = require('path');
require('../js/terrain.js');
require('../js/io.js');
require('../js/roadnet.js');
require('../js/lots.js');
require('../js/greens.js');
require('../js/transit.js');
const ME = globalThis.ME;
const outDir = process.argv[2] || path.join(__dirname, '..', 'samples');
fs.mkdirSync(outDir, { recursive: true });

// 公共底图：平地、方框路、一块地、2个火车站预设(s1 -> Station0，s2 -> Station1)。
function baseModel() {
	const t = new ME.Terrain(256, 256);
	const m = ME.newModel(256, 256);
	const road = (a, b) => { const r = ME.addRoad(m, { a, b }); if (r.error) throw new Error(r.error); return r.road; };
	const at = (x, y) => { const n = ME.pickNode(m, x, y, 0.2); return n ? { node: n.id } : { x, y }; };
	const C = [[100, 100], [140, 100], [140, 140], [100, 140]];
	for (let i = 0; i < 4; i++) road(at(...C[i]), at(...C[(i + 1) % 4]));
	road({ x: 0, y: 140, extern: true }, at(100, 140));   // 城外连接
	const nw = ME.pickNode(m, 100, 100, 0.2), ne = ME.pickNode(m, 140, 100, 0.2);
	const lot = ME.addLot(m, nw.id, ne.id, 40, 'AREA_RESIDENCE_HIGH');
	const add = (dir, rect, id) => { const p = ME.addPreset(m, lot, dir, rect); p.modId = id; return p; };
	const s1 = add(2, { x0: 0, x1: 10, y0: 0, y1: 10 }, 'building_train_station');
	const s2 = add(3, { x0: 0, x1: 10, y0: 28, y1: 38 }, 'building_train_station');

	return { t, m, s1, s2 };
}
const { t, m, s1, s2 } = baseModel();

// 铁轨：两个hub节点之间一段平地铁轨 + 一段曲线桥，route id=route_train
const rail = (spec) => { const r = ME.addTConn(m, spec); if (r.error) throw new Error(r.error); return r.conn; };
const c1 = rail({ a: { x: 40, y: 120, z: 0 }, b: { x: 80, y: 120, z: 0 }, routeId: 'route_train', type: 'ground' });
const c2 = rail({ a: { node: c1.b }, b: { x: 80, y: 180, z: 2 }, ctrl: [80, 150], routeId: 'route_train', type: 'bridge' });
const loop = ME.addTLoop(m, [{ kind: 'preset', ref: s1.id }, { kind: 'node', ref: c1.a }, { kind: 'node', ref: c2.b }, { kind: 'preset', ref: s2.id }], '');
if (loop.error) throw new Error(loop.error);
ME.setLoopStop(m, loop.loop.id, 0, true);
ME.setLoopStop(m, loop.loop.id, 3, true);

const { output, warnings } = ME.exportRoadnet(m, t);
if (warnings.length) console.log('warnings:', warnings);
const tr = output.transit;
if (!tr || tr.conns.length !== 2 || tr.loops.length !== 1 || tr.loops[0].items.length !== 4) throw new Error('transit导出不对: ' + JSON.stringify(tr));
fs.writeFileSync(path.join(outDir, 'transit.terrain'), Buffer.from(ME.writeTerrain(t)));
fs.writeFileSync(path.join(outDir, 'transit.roadnet'), ME.writeRoadnet(m, output));
console.log('transit', JSON.stringify(tr));

// 第二份：同一个车站在环里出现两次，往返各停一次(transit_twice)。车站的轨道沿东西向(Station0: t0向东、t1向西)，
// 东西两侧各一条铁轨，环 = [s1停, 东1, 东2, 东1, s1停, 西1, 西2, 西1]：东2/西2处铁轨原路掉头，东1/西1在往返里各出现两次。
{
	const b = baseModel();
	const rail2 = (spec) => { const r = ME.addTConn(b.m, spec); if (r.error) throw new Error(r.error); return r.conn; };
	const ce = rail2({ a: { x: 150, y: 106, z: 0 }, b: { x: 170, y: 106, z: 0 }, routeId: 'route_train', type: 'ground' });
	const cw = rail2({ a: { x: 80, y: 106, z: 0 }, b: { x: 60, y: 106, z: 0 }, routeId: 'route_train', type: 'ground' });
	const items = [{ kind: 'preset', ref: b.s1.id }, { kind: 'node', ref: ce.a }, { kind: 'node', ref: ce.b }, { kind: 'node', ref: ce.a },
		{ kind: 'preset', ref: b.s1.id }, { kind: 'node', ref: cw.a }, { kind: 'node', ref: cw.b }, { kind: 'node', ref: cw.a }];
	const lp = ME.addTLoop(b.m, items, '');
	if (lp.error) throw new Error(lp.error);
	ME.setLoopStop(b.m, lp.loop.id, 0, true);
	ME.setLoopStop(b.m, lp.loop.id, 4, true);
	const exported = ME.exportRoadnet(b.m, b.t);
	if (exported.warnings.length) console.log('warnings(twice):', exported.warnings);
	const l = exported.output.transit.loops[0];
	if (l.items.length !== 8 || l.items[0].preset !== l.items[4].preset || !l.items[0].stop || !l.items[4].stop) throw new Error('twice导出不对: ' + JSON.stringify(l));
	fs.writeFileSync(path.join(outDir, 'transit_twice.terrain'), Buffer.from(ME.writeTerrain(b.t)));
	fs.writeFileSync(path.join(outDir, 'transit_twice.roadnet'), ME.writeRoadnet(b.m, exported.output));
	console.log('transit_twice', JSON.stringify(exported.output.transit.loops));
}

// 第三份：没有铁轨，只有两个孤立节点夹在车站两侧：环 = [s1停, 东点, s1停, 西点]。
// 东点/西点前后都是缺口、前后项在节点同一侧(都是车站)，游戏里要在节点处掉头(到达端/离开端分开+半圆)，往返各停车站的一条轨道。(transit_hub)
{
	const b = baseModel();
	const east = ME.addTNode(b.m, 160, 106, 0), west = ME.addTNode(b.m, 60, 106, 0);
	// 孤立节点没有route id：环的id从面板给
	const lp = ME.addTLoop(b.m, [{ kind: 'preset', ref: b.s1.id }, { kind: 'node', ref: east.id }, { kind: 'preset', ref: b.s1.id }, { kind: 'node', ref: west.id }], 'route_train');
	if (lp.error) throw new Error(lp.error);
	ME.setLoopStop(b.m, lp.loop.id, 0, true);
	ME.setLoopStop(b.m, lp.loop.id, 2, true);
	const exported = ME.exportRoadnet(b.m, b.t);
	if (exported.warnings.length) console.log('warnings(hub):', exported.warnings);
	fs.writeFileSync(path.join(outDir, 'transit_hub.terrain'), Buffer.from(ME.writeTerrain(b.t)));
	fs.writeFileSync(path.join(outDir, 'transit_hub.roadnet'), ME.writeRoadnet(b.m, exported.output));
	console.log('transit_hub', JSON.stringify(exported.output.transit));
}

// 第四份：公交(路网路线)：方框路上顺时针4个站(每条边一个，都在行车方向右手边、边中点)，一条线首尾成环(transit_bus)。
// 游戏里station_bus按这些站摆放(避开路面开口)，route_bus --skip连成线。
{
	const b = baseModel();
	require('../js/busline.js');
	const roads = b.m.roads.slice(0, 4);
	const r = ME.addBusLine(b.m, roads.map((road) => ({ road: road.id, t: 0.5, side: 0 })));
	if (r.error) throw new Error(r.error);
	const exported = ME.exportRoadnet(b.m, b.t);
	if (exported.warnings.length) console.log('warnings(bus):', exported.warnings);
	const rl = exported.output.transit.roadLines;
	if (!rl || rl.length !== 1 || rl[0].stops.length !== 4) throw new Error('bus导出不对: ' + JSON.stringify(rl));
	fs.writeFileSync(path.join(outDir, 'transit_bus.terrain'), Buffer.from(ME.writeTerrain(b.t)));
	fs.writeFileSync(path.join(outDir, 'transit_bus.roadnet'), ME.writeRoadnet(b.m, exported.output));
	console.log('transit_bus', JSON.stringify(rl));
}
