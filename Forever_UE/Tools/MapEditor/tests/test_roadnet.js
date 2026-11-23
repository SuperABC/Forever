// node tests/test_roadnet.js —— 路网：切分/十字/T口/隧道桥拆段/爬升/导出。
const assert = require('assert');
require('../js/terrain.js');
require('../js/io.js');
require('../js/roadnet.js');
const ME = globalThis.ME;

const free = (x, y, extern) => ({ x, y, extern });

// 1. 十字交叉：横竖两条路相交，应得到1个路口(度4)，4条路。
{
	const m = ME.newModel(256, 256);
	assert.ok(ME.addRoad(m, { a: free(10, 50), b: free(90, 50) }).road);
	assert.ok(ME.addRoad(m, { a: free(50, 10), b: free(50, 90) }).road);
	assert.strictEqual(m.roads.length, 4);
	const center = m.nodes.find((n) => Math.abs(n.x - 50) < 1e-6 && Math.abs(n.y - 50) < 1e-6);
	assert.ok(center);
	assert.strictEqual(ME.nodeDegree(m, center.id), 4);
	const { output } = ME.exportRoadnet(m, null);
	assert.strictEqual(output.nodes.filter((n) => n.type === 'intersection').length, 5); // 中心+4个端点(度1)
	assert.strictEqual(output.roads.length, 4);
}

// 2. T口：新路终点落在已有路中间(用{road,t}引用)。
{
	const m = ME.newModel(256, 256);
	const first = ME.addRoad(m, { a: free(10, 50), b: free(90, 50) }).road;
	const hit = ME.pickRoad(m, 40, 52, 3);
	assert.ok(hit);
	const second = ME.addRoad(m, { a: free(40, 10), b: { road: hit.road.id, t: hit.t } });
	assert.ok(second.road);
	assert.strictEqual(m.roads.length, 3);
	const t = m.nodes.find((n) => ME.nodeDegree(m, n.id) === 3);
	assert.ok(t);
	assert.ok(Math.abs(t.x - 40) < 0.5 && Math.abs(t.y - 50) < 1e-6);
}

// 3. 曲线路与直线路相交也建路口；子曲线两端衔接。
{
	const m = ME.newModel(256, 256);
	ME.addRoad(m, { a: free(10, 50), b: free(90, 50) });
	const r = ME.addRoad(m, { a: free(50, 10), b: free(50, 90), ctrl: [80, 50] });
	assert.ok(r.road);
	assert.ok(m.roads.length >= 4);
	for (const road of m.roads) {
		const q = ME.quadOf(m, road);
		assert.ok(q.p0 && q.p1);
	}
}

// 4. 节点高度：头尾高度相同走直线(平)，不同走扁S(两个控制点，端点水平，1/3长度处)。
{
	const m = ME.newModel(256, 256);
	ME.addRoad(m, { a: free(10, 50), b: free(40, 50), type: 'tunnel' }); // 两端z=0：平
	let out = ME.exportRoadnet(m, null).output;
	assert.strictEqual(out.roads.length, 1);
	assert.strictEqual(out.roads[0].kind, 'tunnel');
	assert.strictEqual(out.roads[0].controls.length, 0);

	const m2 = ME.newModel(256, 256);
	const r = ME.addRoad(m2, { a: { x: 10, y: 50, z: 0 }, b: { x: 50, y: 50, z: -1 }, type: 'mouth' }).road;
	out = ME.exportRoadnet(m2, null).output;
	assert.deepStrictEqual(out.roads.map((x) => x.kind), ['mouth']);
	const ramp = out.roads[0];
	assert.strictEqual(ramp.controls.length, 2);
	assert.ok(Math.abs(ramp.controls[0][0] - (10 + 40 / 3)) < 1e-6);
	assert.strictEqual(ramp.controls[0][2], 0);
	assert.strictEqual(ramp.controls[1][2], -1);
	const z = new Map(out.nodes.map((n) => [n.id, n.z]));
	assert.strictEqual(z.get(ramp.from), 0);
	assert.strictEqual(z.get(ramp.to), -1);
	// 剖面：中点是两端平均，两端水平(端点附近几乎不变)
	const f = ME.profileOf(m2, r, { height: () => 0 });
	assert.ok(Math.abs(f(0.5) + 0.5) < 1e-9);
	assert.ok(Math.abs(f(0.02)) < 0.01);
	// 隧道口连着隧道的那一端写端墙标记(wallFrom/wallTo)，和两端高度无关(山坡上两端一样高也要)
	const m4 = ME.newModel(256, 256);
	const mouth = ME.addRoad(m4, { a: { x: 10, y: 50, z: 0 }, b: { x: 30, y: 50, z: 0 }, type: 'mouth' }).road;
	const tun = ME.addRoad(m4, { a: { node: mouth.b }, b: { x: 60, y: 50, z: 0 }, type: 'tunnel' }).road;
	ME.addRoad(m4, { a: { x: 90, y: 50, z: 0 }, b: { node: tun.b }, type: 'mouth' });
	out = ME.exportRoadnet(m4, null).output;
	const mouths = out.roads.filter((x) => x.kind === 'mouth');
	assert.strictEqual(mouths.length, 2);
	assert.ok(!mouths[0].wallFrom && mouths[0].wallTo === true);
	assert.ok(!mouths[1].wallFrom && mouths[1].wallTo === true);
	// 坡度太陡警告：10单位长要降3
	const m3 = ME.newModel(256, 256);
	ME.addRoad(m3, { a: { x: 10, y: 50, z: 0 }, b: { x: 20, y: 50, z: -3 }, type: 'mouth' });
	assert.ok(ME.validate(m3, null).some((w) => w.includes('坡度')));
}

// 5. 桥：连续的桥归一条桥墩链(含反向画的)；不能从桥中间接路；地面路从桥下穿过不建路口。
{
	const m = ME.newModel(256, 256);
	const r1 = ME.addRoad(m, { a: free(10, 50), b: { x: 22, y: 50, z: 1.5 }, type: 'bridge' }).road;
	const r2 = ME.addRoad(m, { a: { x: 60, y: 50, z: 1.5 }, b: { node: r1.b }, type: 'bridge' }).road; // 反向画：从60画回22
	const { output } = ME.exportRoadnet(m, null);
	assert.deepStrictEqual(output.roads.map((x) => x.kind), ['bridge', 'bridge']);
	assert.strictEqual(output.roads[0].chain, output.roads[1].chain);
	assert.deepStrictEqual(output.roads.map((x) => x.order), [0, 1]);
	assert.deepStrictEqual(output.roads.map((x) => x.reverse), [false, true]);
	const hit = ME.pickRoad(m, 40, 50, 2);
	const e = ME.addRoad(m, { a: free(40, 10), b: { road: hit.road.id, t: hit.t } });
	assert.ok(e.error);
	const m2 = ME.cloneModel(m);
	const under = ME.addRoad(m2, { a: free(40, 10), b: free(40, 90) });
	assert.ok(under.road);
	assert.strictEqual(m2.roads.length, 3);
	assert.ok(!ME.validate(m2, null).some((w) => w.includes('同层')));
	// 度数>2的节点把桥断成两条链
	const m3 = ME.cloneModel(m);
	ME.addRoad(m3, { a: { node: r1.b }, b: free(22, 90) });
	const chains = ME.bridgeChains(m3);
	assert.notStrictEqual(chains.get(r1.id).chain, chains.get(r2.id).chain);
}

// 5b. 从地面路中间接路：新节点的高度取路在该处的高度；同层十字取两路高度平均。
{
	const m = ME.newModel(256, 256);
	const g = ME.addRoad(m, { a: { x: 10, y: 50, z: 0 }, b: { x: 90, y: 50, z: 1 }, type: 'ground' }).road;
	const hit = ME.pickRoad(m, 50, 51, 3);
	ME.addRoad(m, { a: free(50, 10), b: { road: g.id, t: hit.t } });
	const t = m.nodes.find((n) => ME.nodeDegree(m, n.id) === 3);
	assert.ok(Math.abs(t.z - 0.5) < 0.02, 'z ' + t.z);
	const m2 = ME.newModel(256, 256);
	ME.addRoad(m2, { a: { x: 10, y: 50, z: 0 }, b: { x: 90, y: 50, z: 0 } });
	ME.addRoad(m2, { a: { x: 50, y: 10, z: 0.2 }, b: { x: 50, y: 90, z: 0.2 } });
	const c = m2.nodes.find((n) => ME.nodeDegree(m2, n.id) === 4);
	assert.ok(c && Math.abs(c.z - 0.1) < 1e-6);
}

// 6. 贴合：山上的路跟随地形，平原部分为0；头节点高度有偏移时偏移逐渐过渡到尾节点。
{
	const t = new ME.Terrain(128, 128);
	t.beginStroke(); t.brush('paint', 64, 50, 20, { terrain: 'terrain_mountain' }); t.endStroke();
	t.beginStroke(); for (let i = 0; i < 6; i++) t.brush('raise', 64, 50, 15, { strength: 1 }); t.endStroke();
	const m = ME.newModel(128, 128);
	ME.addRoad(m, { a: free(20, 50), b: free(110, 50), type: 'hug' });
	const { output } = ME.exportRoadnet(m, t);
	assert.ok(output.roads.length > 10);
	assert.ok(output.roads.every((r) => r.kind === 'hug' || r.kind === 'ground'));
	const maxZ = Math.max(...output.nodes.map((n) => n.z));
	assert.ok(maxZ > 1, 'peak z ' + maxZ);
	assert.ok(Math.abs(output.nodes.find((n) => n.id === output.roads[0].from).z) < 1e-3);
	// 头节点抬高0.5：起点z=0.5，尾点仍是地形(0)，中途单调回落到地形之上
	const m1 = ME.newModel(128, 128);
	ME.addRoad(m1, { a: { x: 20, y: 50, z: 0.5 }, b: free(110, 50), type: 'hug' });
	const o1 = ME.exportRoadnet(m1, t).output;
	const zOf = new Map(o1.nodes.map((n) => [n.id, n.z]));
	assert.ok(Math.abs(zOf.get(o1.roads[0].from) - 0.5) < 1e-6);
	assert.ok(Math.abs(zOf.get(o1.roads[o1.roads.length - 1].to)) < 1e-3);
	// 平地路压在山上有警告；设成地形高度的节点+平地仍然对不上(中间是山)
	const m2 = ME.newModel(128, 128);
	ME.addRoad(m2, { a: free(20, 50), b: free(110, 50), type: 'ground' });
	assert.ok(ME.validate(m2, t).some((w) => w.includes('平地')));
}

// 6b. 水和平地路：地面以下的水(岸边平原格)不报警；路面低于水面(铺在湖底)才报警。
{
	const t = new ME.Terrain(64, 64);
	t.beginStroke(); t.brush('water', 30, 30, 4, { level: -0.1 }); t.endStroke();
	const m = ME.newModel(64, 64);
	ME.addRoad(m, { a: free(10, 30), b: free(50, 30), type: 'ground' }); // 平原上z=0，水面-0.1在路面以下
	assert.ok(!ME.validate(m, t).some((w) => w.includes('水')));
	const t2 = new ME.Terrain(64, 64);
	t2.beginStroke(); t2.brush('paint', 30, 30, 4, { terrain: 'lake' }); t2.brush('flatten', 30, 30, 4, { target: -2, strength: 1 }); t2.brush('water', 30, 30, 4, { level: -0.1 }); t2.endStroke();
	const m2 = ME.newModel(64, 64);
	ME.addRoad(m2, { a: { x: 10, y: 30, z: -2 }, b: { x: 50, y: 30, z: -2 }, type: 'ground' }); // 湖底的路：路面-2在水面-0.1以下
	assert.ok(ME.validate(m2, t2).some((w) => w.includes('泡在水里')));
	assert.ok(!ME.validate(m, t2).some((w) => w.includes('泡在水里'))); // z=0的路在水面以上(只会报和地形对不上)
}

// 6c. 旧版(version 1)文件迁移：隧道按旧布局拆成 mouth/mouth/tunnel/mouth/mouth，节点带z。
{
	const old = { mapW: 256, mapH: 256, nextId: 4, nodes: [{ id: 1, x: 10, y: 50 }, { id: 2, x: 90, y: 50 }], lots: [],
		roads: [{ id: 3, a: 1, b: 2, ctrl: null, type: 'tunnel', lanes: [1, 0, 1, 1, 0, 1], height: -1, name: 't' }] };
	const m = ME.migrateModel(old, { height: () => 0 });
	assert.strictEqual(m.version, 2);
	assert.deepStrictEqual(m.roads.map((r) => r.type), ['mouth', 'mouth', 'tunnel', 'mouth', 'mouth']);
	assert.ok(m.roads.every((r) => r.height === undefined));
	assert.ok(m.nodes.some((n) => n.z === -1));
	const { output } = ME.exportRoadnet(m, null);
	assert.strictEqual(output.roads.length, 5);
	assert.deepStrictEqual(ME.migrateModel(m).roads.length, 5); // 新版不再迁移
}

// 7. 城外连接点：extern节点导出为extern类型。
{
	const m = ME.newModel(256, 256);
	ME.addRoad(m, { a: free(0, 100, true), b: free(60, 100) });
	const { output, warnings } = ME.exportRoadnet(m, null);
	assert.strictEqual(output.nodes.filter((n) => n.type === 'extern').length, 1);
	assert.ok(!warnings.some((w) => w.includes('边界')));
}

// 8. 读写往返。
{
	const m = ME.newModel(256, 256);
	ME.addRoad(m, { a: free(10, 50), b: free(90, 50) });
	const { output } = ME.exportRoadnet(m, null);
	const text = ME.writeRoadnet(m, output);
	const back = ME.readRoadnet(text);
	assert.deepStrictEqual(back.source.roads.length, m.roads.length);
	assert.strictEqual(back.output.roads.length, output.roads.length);
}

// 吸附角度(直角+平角)：从已连了路的节点/路中间往外拉。
{
	const m = ME.newModel(256, 256);
	const r1 = ME.addRoad(m, { a: free(10, 50), b: free(50, 50) }).road; // 横路，终点(50,50)度1
	const end = ME.nodeOf(m, r1.b);
	// 鼠标在正下方偏5度：吸到(50, y)垂直线上
	const ang = 5 * Math.PI / 180;
	let ra = ME.angleSnapAt(m, { node: end.id }, 50 + Math.sin(ang) * 20, 50 + Math.cos(ang) * 20, 10);
	assert.ok(ra);
	assert.ok(Math.abs(ra.point[0] - 50) < 1e-6 && ra.point[1] > 50);
	assert.ok(Math.abs(ra.point[1] - (50 + Math.hypot(Math.sin(ang) * 20, Math.cos(ang) * 20) * Math.cos(ang))) < 1e-6);
	// 向上也行(两侧都算)
	ra = ME.angleSnapAt(m, { node: end.id }, 52, 30, 10);
	assert.ok(ra && Math.abs(ra.point[0] - 50) < 1e-6 && ra.point[1] < 50);
	// 夹角太大：不吸附
	assert.strictEqual(ME.angleSnapAt(m, { node: end.id }, 70, 70, 10), null);
	// 没连路的节点：不适用
	const lone = ME.addNode(m, 100, 100);
	assert.strictEqual(ME.angleSnapAt(m, { node: lone.id }, 100, 120, 10), null);
	// 节点上连了两条路(横+竖)：取夹角最小的垂线
	ME.addRoad(m, { a: { node: end.id }, b: free(50, 90) });
	ra = ME.angleSnapAt(m, { node: end.id }, 80, 52, 10); // 靠近水平方向：垂直于竖路 → 水平
	assert.ok(ra && Math.abs(ra.point[1] - 50) < 1e-6 && ra.point[0] > 50);
	// 曲线的切线用控制点：控制点在(50,0)偏上的曲线，端点处切线竖直 → 垂线水平
	const m2 = ME.newModel(256, 256);
	const c = ME.addRoad(m2, { a: free(10, 50), b: free(50, 50), ctrl: [50, 10] }).road;
	const e2 = ME.nodeOf(m2, c.b);
	ra = ME.angleSnapAt(m2, { node: e2.id }, 80, 53, 10);
	assert.ok(ra && Math.abs(ra.point[1] - 50) < 1e-6);
	// 路中间(T口起点)：垂直于该处切线
	const m3 = ME.newModel(256, 256);
	const h = ME.addRoad(m3, { a: free(10, 50), b: free(90, 50) }).road;
	ra = ME.angleSnapAt(m3, { road: h.id, t: 0.5 }, 51, 70, 10);
	assert.ok(ra && Math.abs(ra.point[0] - 50) < 1e-6 && Math.abs(ra.origin[0] - 50) < 1e-6);
	// 平角：节点上沿已连道路延长线直走(节点上的横路 → 向右延长)
	const m4 = ME.newModel(256, 256);
	const s1 = ME.addRoad(m4, { a: free(10, 50), b: free(50, 50) }).road;
	const sEnd = ME.nodeOf(m4, s1.b);
	ra = ME.angleSnapAt(m4, { node: sEnd.id }, 80, 53, 10);
	assert.ok(ra && ra.kind === 'straight' && Math.abs(ra.point[1] - 50) < 1e-6 && ra.point[0] > 50);
	// 往回(压在原路上)不吸附成平角
	assert.strictEqual(ME.angleSnapAt(m4, { node: sEnd.id }, 20, 51, 10), null);
	// 路中间只有直角，没有平角
	assert.strictEqual(ME.angleSnapAt(m3, { road: h.id, t: 0.5 }, 80, 51, 10), null);
	assert.strictEqual(ME.angleSnapAt(m4, { node: sEnd.id }, 52, 80, 10).kind, 'right');
}

// 地形贴合路面(fit)：路面高度取节点z(首尾、中间都是1)，不随地形起伏，也不报"高度和地形对不上"(游戏侧会把地形贴合到路面)；
// 对比平地(ground)在起伏地形上会报警告，贴合地形(hug)的路面则跟着地形走。
{
	const t = new ME.Terrain(128, 128);
	for (let y = 0; y < 128; y++) for (let x = 0; x < 128; x++) t.height[y * 128 + x] = (x >= 40 && x <= 60) ? 1 + 1.5 * Math.sin((x - 40) / 20 * Math.PI) : 1;
	const build = (type) => {
		const m = ME.newModel(128, 128);
		const a = ME.addNode(m, 10, 50, false, 1), b = ME.addNode(m, 100, 50, false, 1);
		ME.addRoad(m, { a: { node: a.id }, b: { node: b.id }, type });
		return ME.exportRoadnet(m, t);
	};
	const fit = build('fit'), ground = build('ground'), hug = build('hug');
	assert.ok(fit.output.roads.length === 1 && fit.output.roads[0].kind === 'fit', JSON.stringify(fit.output.roads.map((r) => r.kind)));
	const zs = (o) => (o.roads[0].controls || []).map((c) => c[2]);
	assert.ok(zs(fit.output).every((z) => Math.abs(z - 1) < 1e-6), '路面一直是z=1: ' + zs(fit.output));
	assert.ok(!fit.warnings.some((w) => w.includes('对不上')), '地形贴合路面不报"高度和地形对不上"');
	assert.ok(ground.warnings.some((w) => w.includes('对不上')), '平地在起伏地形上仍报警告');
	assert.ok(hug.output.roads.length > 1 && hug.output.roads.every((r) => r.kind === 'hug' || r.kind === 'ground'));
	// fit路可从中间切开(T口)，tag照样跟着路走
	const m2 = ME.newModel(128, 128);
	const r = ME.addRoad(m2, { a: { x: 10, y: 50 }, b: { x: 100, y: 50 }, type: 'fit', tree: 'default' }).road;
	assert.ok(ME.splitRoadAt(m2, r.id, 0.5), 'fit路可切开');
}
console.log('test_roadnet OK');

// 贴合段接平路的一端缓入到水平：终点斜率为0(最后一段第二个控制点z=终点z)，
{
	const t = new ME.Terrain(128, 128);
	t.beginStroke(); t.brush('paint', 64, 50, 20, { terrain: 'terrain_mountain' }); t.endStroke();
	t.beginStroke(); for (let i = 0; i < 6; i++) t.brush('raise', 64, 50, 15, { strength: 1 }); t.endStroke();
	const m = ME.newModel(128, 128);
	const h0 = t.sampleHeight(40, 50), h1 = t.sampleHeight(56, 50);
	const hug = ME.addRoad(m, { a: { x: 40, y: 50, z: h0 }, b: { x: 56, y: 50, z: h1 }, type: 'hug' }).road;
	ME.addRoad(m, { a: { node: hug.b }, b: { x: 72, y: 50, z: h1 }, type: 'ground' });
	const out = ME.exportRoadnet(m, t).output;
	const hugs = out.roads.filter((x) => x.kind === 'hug');
	assert.ok(hugs.length > 10 && h1 > 0.5);
	const last = hugs[hugs.length - 1];
	const zOf = new Map(out.nodes.map((n) => [n.id, n.z]));
	assert.strictEqual(last.controls.length, 2);
	assert.ok(Math.abs(last.controls[1][2] - zOf.get(last.to)) < 1e-6, '终点斜率应为0');
}

// 度数2的节点：拐角导成intersection(有缩进和路口mesh)，近似直线的仍是plain。
{
	const m = ME.newModel(256, 256);
	const r1 = ME.addRoad(m, { a: free(10, 50), b: free(50, 50), type: 'ground' }).road;
	const r2 = ME.addRoad(m, { a: { node: r1.b }, b: free(50, 90), type: 'ground' }).road; // 90度拐角
	const r3 = ME.addRoad(m, { a: { node: r2.b }, b: free(50, 130), type: 'ground' }).road; // 直行
	const nodes = ME.exportRoadnet(m, null).output.nodes;
	const typeOf = (id) => nodes.find((n) => n.id === 'n' + id).type;
	assert.strictEqual(typeOf(r1.b), 'intersection');
	assert.strictEqual(typeOf(r2.b), 'plain');
}

// 行道树/路灯tag：写进road，T口/切分后两半都继承；每个路段(不分类型)都导出treeTag/lampTag，tag为空导出空串。
{
	const m = ME.newModel(256, 256);
	const r1 = ME.addRoad(m, { a: free(10, 50), b: free(90, 50), type: 'ground', tree: 'default', lamp: 'default' }).road;
	ME.addRoad(m, { a: free(10, 90), b: free(90, 90), type: 'ground' });   // 没填tag
	ME.splitRoadAt(m, r1.id, 0.5);
	assert.strictEqual(m.roads.filter((r) => r.tree === 'default' && r.lamp === 'default').length, 2);
	const out = ME.exportRoadnet(m, null).output;
	const tagged = out.roads.filter((r) => r.treeTag === 'default' && r.lampTag === 'default');
	assert.strictEqual(tagged.length, 2);
	const untagged = out.roads.filter((r) => r.treeTag === '' && r.lampTag === '');
	assert.strictEqual(untagged.length, 1);
	assert.ok(out.roads.every((r) => r.kind === 'ground'));	// 非平面路段(隧道/桥/贴合)也按路自己的tag导出
	const m2 = ME.newModel(256, 256);
	ME.addRoad(m2, { a: free(10, 50), b: free(90, 50), type: 'bridge', tree: 'default', lamp: '' });
	ME.addRoad(m2, { a: free(10, 90), b: free(90, 90), type: 'hug', tree: '', lamp: 'default' });
	const out2 = ME.exportRoadnet(m2, new ME.Terrain(256, 256)).output;
	const bridge = out2.roads.filter((r) => r.kind === 'bridge'), hug = out2.roads.filter((r) => r.kind !== 'bridge');   // 平地上的hug路段导出成ground，tag照样跟着路走
	assert.ok(bridge.length && bridge.every((r) => r.treeTag === 'default' && r.lampTag === ''));
	assert.ok(hug.length && hug.every((r) => r.treeTag === '' && r.lampTag === 'default'));
}

// 车道资产 + 车道类型：新路默认 asset=default，导出带 asset；旧文件(没有asset)迁移后补 default；车道类型带方向(forward=a→b 车道数, backward=b→a 车道数)。
{
	const m = ME.newModel(256, 256);
	const r = ME.addRoad(m, { a: { x: 10, y: 10 }, b: { x: 60, y: 10 }, lanes: ME.LANE_PRESETS[4].lanes }).road;
	assert.strictEqual(r.asset, 'default');
	assert.strictEqual(ME.exportRoadnet(m, null).output.roads[0].asset, 'default');
	delete r.asset;
	assert.strictEqual(ME.migrateModel(m).roads[0].asset, 'default');
	ME.roadOf(m, r.id).asset = 'city';
	assert.strictEqual(ME.exportRoadnet(m, null).output.roads[0].asset, 'city');
	// 每个车道类型的资产id和lanes一致，单行道一侧车道为0
	for (const p of ME.LANE_PRESETS) assert.strictEqual(p.id, p.lanes.join('_'));
	const oneWay = ME.LANE_PRESETS.filter((p) => p.forward === 0 || p.backward === 0);
	assert.strictEqual(oneWay.length, 2);
	assert.ok(oneWay.some((p) => p.forward === 2) && oneWay.some((p) => p.backward === 2));
}
