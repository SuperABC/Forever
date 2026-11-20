// 文件读写(纯逻辑，无DOM)。.terrain二进制格式见 Source/Basic/map/terrain_file.md，.roadnet见 Source/Basic/map/roadnet_file.md。
(function (root) {
	const ME = root.ME = root.ME || {};

	// ---- .terrain ----
	// 魔数FTRN；u32 w,h；u16调色板条数，每条u16长度+UTF8；u16[w*h]下标；f32[w*h]高度；f32[w*h]水位(NaN=无水)。小端。
	function writeTerrain(terrain) {
		const encoder = new TextEncoder();
		const names = terrain.palette.map((name) => encoder.encode(name));
		let header = 4 + 4 + 4 + 2;
		for (const bytes of names) header += 2 + bytes.length;
		const cells = terrain.w * terrain.h;
		const buffer = new ArrayBuffer(header + cells * 10);
		const view = new DataView(buffer);
		const bytes8 = new Uint8Array(buffer);
		let o = 0;
		for (const ch of 'FTRN') bytes8[o++] = ch.charCodeAt(0);
		view.setUint32(o, terrain.w, true); o += 4;
		view.setUint32(o, terrain.h, true); o += 4;
		view.setUint16(o, names.length, true); o += 2;
		for (const bytes of names) {
			view.setUint16(o, bytes.length, true); o += 2;
			bytes8.set(bytes, o); o += bytes.length;
		}
		for (let i = 0; i < cells; i++) { view.setUint16(o, terrain.idx[i], true); o += 2; }
		for (let i = 0; i < cells; i++) { view.setFloat32(o, terrain.height[i], true); o += 4; }
		for (let i = 0; i < cells; i++) { view.setFloat32(o, terrain.water[i], true); o += 4; }
		return buffer;
	}

	function readTerrain(buffer) {
		const view = new DataView(buffer);
		const bytes8 = new Uint8Array(buffer);
		let o = 0;
		let magic = '';
		for (let i = 0; i < 4; i++) magic += String.fromCharCode(bytes8[o++]);
		if (magic !== 'FTRN') throw new Error('不是.terrain文件(魔数不对)');
		const w = view.getUint32(o, true); o += 4;
		const h = view.getUint32(o, true); o += 4;
		const count = view.getUint16(o, true); o += 2;
		const decoder = new TextDecoder();
		const palette = [];
		for (let i = 0; i < count; i++) {
			const length = view.getUint16(o, true); o += 2;
			palette.push(decoder.decode(bytes8.subarray(o, o + length))); o += length;
		}
		const cells = w * h;
		if (o + cells * 10 > buffer.byteLength) throw new Error('.terrain文件被截断');
		const terrain = new ME.Terrain(w, h);
		terrain.palette = palette.length ? palette : ['plain'];
		for (let i = 0; i < cells; i++) { terrain.idx[i] = view.getUint16(o, true); o += 2; }
		for (let i = 0; i < cells; i++) { terrain.height[i] = view.getFloat32(o, true); o += 4; }
		for (let i = 0; i < cells; i++) { terrain.water[i] = view.getFloat32(o, true); o += 4; }
		return terrain;
	}

	// ---- .roadnet ----
	// {source:编辑模型(编辑器自用), output:展开后的路网(游戏mod读)}。宽松JSON，数字保留6位小数。
	function writeRoadnet(source, output) {
		const round = (key, value) => (typeof value === 'number' ? Math.round(value * 1e6) / 1e6 : value);
		return JSON.stringify({ source, output }, round, '\t');
	}
	function readRoadnet(text) {
		// 宽松JSON：去掉对象/数组结尾的多余逗号和注释再解析。
		const cleaned = text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/(^|[^:"])\/\/.*$/gm, '$1').replace(/,(\s*[}\]])/g, '$1');
		const data = JSON.parse(cleaned);
		if (!data || typeof data !== 'object' || !data.output) throw new Error('不是.roadnet文件(没有output段)');
		return data;
	}

	ME.writeTerrain = writeTerrain;
	ME.readTerrain = readTerrain;
	ME.writeRoadnet = writeRoadnet;
	ME.readRoadnet = readRoadnet;
	if (typeof module !== 'undefined') module.exports = ME;
})(typeof window !== 'undefined' ? window : globalThis);
