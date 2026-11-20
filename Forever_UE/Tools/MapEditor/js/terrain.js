// 地形数据与笔刷(纯逻辑，无DOM)。格式与约束见 Source/Basic/map/terrain_file.md。
// 一个element = 地图上10m x 10m的格子；字符串为plain/construction的格子是"平原"：高度锁0(建筑假定建在高度0平原上)。
// 水可以放在任意格子(含平原)：岸边的平原格也要有水面，否则水面和地面之间会露出缝隙。
(function (root) {
	const ME = root.ME = root.ME || {};

	const SEA_LEVEL = -0.1;

	function isFlatName(name) { return name === 'plain' || name === 'construction'; }

	class Terrain {
		constructor(w, h) {
			this.w = w;
			this.h = h;
			this.palette = ['plain'];            // 下标0固定plain
			this.idx = new Uint16Array(w * h);   // 每格的调色板下标，行优先 y*w+x
			this.height = new Float32Array(w * h);
			this.water = new Float32Array(w * h).fill(NaN); // NaN=无水
			this.stroke = null;
		}

		paletteIndex(name) {
			let i = this.palette.indexOf(name);
			if (i < 0) { this.palette.push(name); i = this.palette.length - 1; }
			return i;
		}
		nameAt(i) { return this.palette[this.idx[i]]; }
		isFlat(i) { return isFlatName(this.palette[this.idx[i]]); }
		inside(x, y) { return x >= 0 && y >= 0 && x < this.w && y < this.h; }

		// 任意连续坐标的高度(同Core Map::SampleHeight：格子按"自适应对角线"拆成两个三角形线性插值——
		// 对角线取两端高度差较小的那条，斜着走的交界就是一道笔直的斜坡；右/下越界按边缘夹取)。
		sampleHeight(x, y) {
			const x0 = Math.floor(x), y0 = Math.floor(y);
			const fx = x - x0, fy = y - y0;
			const cx = (v) => Math.min(this.w - 1, Math.max(0, v));
			const cy = (v) => Math.min(this.h - 1, Math.max(0, v));
			const h = (xx, yy) => this.height[cy(yy) * this.w + cx(xx)];
			const h00 = h(x0, y0), h10 = h(x0 + 1, y0), h01 = h(x0, y0 + 1), h11 = h(x0 + 1, y0 + 1);
			if (Math.abs(h10 - h01) < Math.abs(h00 - h11)) {
				if (fx + fy <= 1) return h00 + fx * (h10 - h00) + fy * (h01 - h00);
				return h11 + (1 - fx) * (h01 - h11) + (1 - fy) * (h10 - h11);
			}
			if (fx >= fy) return h00 + fx * (h10 - h00) + fy * (h11 - h10);
			return h00 + fy * (h01 - h00) + fx * (h11 - h01);
		}

		// 把平原格的高度清零；水不动。(导出/载入不再自动调用：笔刷的"允许修改平地"可以让平原格有高度，需要时手动调)
		enforceFlat() {
			for (let i = 0; i < this.idx.length; i++) {
				if (this.isFlat(i)) this.height[i] = 0;
			}
		}

		// ---- 笔画(撤销用)：beginStroke后所有改动先记下旧值，endStroke返回可撤销/重做的动作 ----
		beginStroke() { this.stroke = { before: new Map(), paletteLength: this.palette.length }; }
		_touch(i) {
			if (this.stroke && !this.stroke.before.has(i)) this.stroke.before.set(i, [this.idx[i], this.height[i], this.water[i]]);
		}
		endStroke() {
			const stroke = this.stroke;
			this.stroke = null;
			if (!stroke || stroke.before.size === 0) return null;
			const indices = Array.from(stroke.before.keys());
			const before = indices.map((i) => stroke.before.get(i));
			const after = indices.map((i) => [this.idx[i], this.height[i], this.water[i]]);
			return { indices, before, after };
		}
		applyCells(indices, values) {
			for (let k = 0; k < indices.length; k++) {
				const i = indices[k];
				this.idx[i] = values[k][0]; this.height[i] = values[k][1]; this.water[i] = values[k][2];
			}
		}

		// ---- 笔刷 ----
		// op: paint / water / raise / lower / flatten / smooth ；params: {terrain, level, strength, target, erase, only, allowFlat}
		// only: 调色板下标。给了就只改这个地形的格子(抬高/降低/抹平/平滑)；平滑取平均时仍然把所有邻格(含平原/别的地形)都算上，
		// 这样山体和平原的交界处会被平滑地拉向平原高度，而不是越抹边界悬崖越高。
		// (cx,cy)是笔刷中心(element连续坐标)，radius单位element。返回改了多少格。
		brush(op, cx, cy, radius, params) {
			params = params || {};
			// 平滑强度可以大于1：拆成多遍(每遍强度<=1，后一遍吃前一遍的结果)，强度越大抹得越平。
			if (op === 'smooth' && params.strength > 1) {
				const passes = Math.ceil(params.strength);
				const each = Object.assign({}, params, { strength: params.strength / passes });
				let total = 0;
				for (let p = 0; p < passes; p++) total += this.brush(op, cx, cy, radius, each);
				return total;
			}
			let changed = 0;
			const x0 = Math.max(0, Math.floor(cx - radius)), x1 = Math.min(this.w - 1, Math.ceil(cx + radius));
			const y0 = Math.max(0, Math.floor(cy - radius)), y1 = Math.min(this.h - 1, Math.ceil(cy + radius));
			const strength = params.strength === undefined ? 0.5 : params.strength;
			let smoothSource = null;
			if (op === 'smooth') smoothSource = this.height.slice();
			for (let y = y0; y <= y1; y++) {
				for (let x = x0; x <= x1; x++) {
					const dx = x + 0.5 - cx, dy = y + 0.5 - cy;
					const d = Math.sqrt(dx * dx + dy * dy);
					if (d > radius) continue;
					const i = y * this.w + x;
					const t = d / radius;
					const falloff = (1 - t * t) * (1 - t * t); // 中心1，边缘0
					if (op === 'paint') {
						const index = this.paletteIndex(params.terrain || 'plain');
						if (this.idx[i] === index) continue;
						this._touch(i);
						this.idx[i] = index;
						if (isFlatName(this.palette[index])) this.height[i] = 0;
						changed++;
					} else if (op === 'water') {
						const level = params.erase ? NaN : (params.level === undefined ? SEA_LEVEL : params.level);
						if (Object.is(this.water[i], level) || (this.water[i] === level)) continue;
						this._touch(i);
						this.water[i] = level;
						changed++;
					} else if (this.isFlat(i) && !params.allowFlat) {
						continue; // 默认高度只能落在非平原格；params.allowFlat=true时允许改平地的高度
					} else if (params.only !== undefined && this.idx[i] !== params.only) {
						continue; // 限定单一地形：不碰别的地形的格子
					} else if (op === 'raise' || op === 'lower') {
						const delta = (op === 'raise' ? 1 : -1) * strength * falloff;
						if (Math.abs(delta) < 1e-6) continue;
						this._touch(i);
						this.height[i] += delta;
						changed++;
					} else if (op === 'flatten') {
						const target = params.target === undefined ? 0 : params.target;
						const next = this.height[i] + (target - this.height[i]) * Math.min(1, strength * 2 * falloff);
						if (Math.abs(next - this.height[i]) < 1e-6) continue;
						this._touch(i);
						this.height[i] = next;
						changed++;
					} else if (op === 'smooth') {
						let sum = 0, n = 0;
						for (let oy = -1; oy <= 1; oy++) for (let ox = -1; ox <= 1; ox++) {
							const nx = x + ox, ny = y + oy;
							if (!this.inside(nx, ny)) continue;
							sum += smoothSource[ny * this.w + nx]; n++;
						}
						const avg = sum / n;
						const next = this.height[i] + (avg - this.height[i]) * Math.min(1, strength * 2 * falloff);
						if (Math.abs(next - this.height[i]) < 1e-6) continue;
						this._touch(i);
						this.height[i] = next;
						changed++;
					}
				}
			}
			return changed;
		}


		// ---- 一键整理(整张地图，调用方包在beginStroke/endStroke里) ----
		// 平滑高度：每个非平原格点到最近平原格点的欧氏距离d(格点单位)，高度乘 smoothstep(d/width)：
		// 紧贴平原的一圈接近0，随距离缓升，d>=width以外不动。山的抬升和海的下降(负高度)都朝0缓降，
		// 所以交界处的"三高一低/三低一高"格子会消失。距离先做两遍3x3平均，抹掉锯齿台阶带来的距离抖动。返回改了多少格点。
		relaxFlatBoundary(width) {
			width = Math.max(1, width || 1);
			const w = this.w, h = this.h, n = w * h;
			const INF = 1e20;
			let anyFlat = false;
			const g = new Float64Array(n);
			for (let i = 0; i < n; i++) { if (this.isFlat(i)) { g[i] = 0; anyFlat = true; } else g[i] = INF; }
			if (!anyFlat) return 0;
			// Felzenszwalb精确欧氏距离变换：先列后行，输入/输出都是平方距离。
			const size = Math.max(w, h);
			const f = new Float64Array(size), d = new Float64Array(size), vv = new Int32Array(size), z = new Float64Array(size + 1);
			const pass = (len, get, set) => {
				for (let q = 0; q < len; q++) f[q] = get(q);
				let k = 0; vv[0] = 0; z[0] = -INF; z[1] = INF;
				for (let q = 1; q < len; q++) {
					let s;
					while (true) {
						s = ((f[q] + q * q) - (f[vv[k]] + vv[k] * vv[k])) / (2 * q - 2 * vv[k]);
						if (s <= z[k] && k > 0) k--; else break;
					}
					k++; vv[k] = q; z[k] = s; z[k + 1] = INF;
				}
				k = 0;
				for (let q = 0; q < len; q++) { while (z[k + 1] < q) k++; d[q] = (q - vv[k]) * (q - vv[k]) + f[vv[k]]; }
				for (let q = 0; q < len; q++) set(q, d[q]);
			};
			for (let x = 0; x < w; x++) pass(h, (y) => g[y * w + x], (y, v) => { g[y * w + x] = v; });
			for (let y = 0; y < h; y++) pass(w, (x) => g[y * w + x], (x, v) => { g[y * w + x] = v; });
			let dist = new Float32Array(n);
			for (let i = 0; i < n; i++) dist[i] = Math.sqrt(g[i]);
			for (let rep = 0; rep < 2; rep++) {
				const next = new Float32Array(n);
				for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) {
					const i = y * w + x;
					if (dist[i] === 0) { next[i] = 0; continue; }
					let sum = 0, cnt = 0;
					for (let oy = -1; oy <= 1; oy++) for (let ox = -1; ox <= 1; ox++) {
						const nx = x + ox, ny = y + oy;
						if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
						sum += dist[ny * w + nx]; cnt++;
					}
					next[i] = sum / cnt;
				}
				dist = next;
			}
			let changed = 0;
			for (let i = 0; i < n; i++) {
				if (this.isFlat(i)) continue;
				const t = Math.min(1, dist[i] / width);
				if (t >= 1) continue;
				const nh = this.height[i] * (t * t * (3 - 2 * t));
				if (Math.abs(nh - this.height[i]) < 1e-6) continue;
				this._touch(i);
				this.height[i] = nh;
				changed++;
			}
			return changed;
		}

		// 平滑类型：众数滤波，去掉1格宽的毛刺和突出的角。每个格子看自己的3x3(越界不算)，
		// 别的类型占严格多数(超过一半)且比自己这一类多时，改成那个类型。对任何类型字符串都生效。
		// 改成平原类时高度清0；平原变成非平原时高度取3x3里同类型邻格的平均；水不动。返回改了多少格，passes遍。
		smoothTypes(passes) {
			passes = Math.max(1, Math.floor(passes || 1));
			const w = this.w, h = this.h;
			let total = 0;
			const vals = new Int32Array(9);
			for (let p = 0; p < passes; p++) {
				const src = this.idx.slice();
				let changedThisPass = 0;
				for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) {
					const i = y * w + x, cur = src[i];
					let n = 0;
					for (let oy = -1; oy <= 1; oy++) for (let ox = -1; ox <= 1; ox++) {
						const nx = x + ox, ny = y + oy;
						if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
						vals[n++] = src[ny * w + nx];
					}
					let curCount = 0, best = cur, bestCount = 0;
					for (let a = 0; a < n; a++) {
						let c = 0;
						for (let b = 0; b < n; b++) if (vals[b] === vals[a]) c++;
						if (vals[a] === cur) curCount = c;
						if (c > bestCount || (c === bestCount && vals[a] < best && best !== cur)) { bestCount = c; best = vals[a]; }
					}
					if (best === cur || bestCount * 2 <= n || bestCount <= curCount) continue;
					this._touch(i);
					const wasFlat = this.isFlat(i);
					this.idx[i] = best;
					if (this.isFlat(i)) this.height[i] = 0;
					else if (wasFlat) {
						let sum = 0, cnt = 0;
						for (let oy = -1; oy <= 1; oy++) for (let ox = -1; ox <= 1; ox++) {
							const nx = x + ox, ny = y + oy;
							if (nx < 0 || ny < 0 || nx >= w || ny >= h || src[ny * w + nx] !== best) continue;
							sum += this.height[ny * w + nx]; cnt++;
						}
						this.height[i] = cnt ? sum / cnt : 0;
					}
					changedThisPass++;
				}
				total += changedThisPass;
				if (!changedThisPass) break;
			}
			return total;
		}

		// 检查：lot覆盖区域要求每个element的3x3邻域都是plain/construction，高度0(水不影响：地面以下有水、岸边地面以上有水都合理)。返回不合格格子数。
		countNonBuildable(x0, y0, x1, y1) {
			let bad = 0;
			for (let y = Math.floor(y0); y < Math.ceil(y1); y++) {
				for (let x = Math.floor(x0); x < Math.ceil(x1); x++) {
					if (!this.inside(x, y)) { bad++; continue; }
					let ok = true;
					for (let oy = -1; oy <= 1 && ok; oy++) for (let ox = -1; ox <= 1 && ok; ox++) {
						const nx = x + ox, ny = y + oy;
						if (!this.inside(nx, ny)) { ok = false; break; }
						const i = ny * this.w + nx;
						if (!this.isFlat(i) || this.height[i] !== 0) ok = false; // 平原格但被抬高/压低过也不能建
					}
					if (!ok) bad++;
				}
			}
			return bad;
		}
	}

	// 等高线：把格点高度场(格点(x,y)的值=height[y*w+x]，和sampleHeight同一套双线性)按interval切成线段。
	// 返回 {segs: Float32Array [x0,y0,x1,y1,major(0/1) ...], interval}。每interval*5一条主等高线。
	// 层数过多时自动加大间距(最多约160层)。
	function contourSegments(terrain, interval) {
		const w = terrain.w, h = terrain.h, H = terrain.height;
		let lo = Infinity, hi = -Infinity;
		for (let i = 0; i < H.length; i++) { const v = H[i]; if (v < lo) lo = v; if (v > hi) hi = v; }
		interval = Math.max(0.05, interval || 1);
		while ((hi - lo) / interval > 160) interval *= 2;
		const out = [];
		if (!(hi > lo)) return { segs: new Float32Array(0), interval };
		for (let y = 0; y + 1 < h; y++) {
			for (let x = 0; x + 1 < w; x++) {
				const a = H[y * w + x], b = H[y * w + x + 1], c = H[(y + 1) * w + x + 1], d = H[(y + 1) * w + x];
				const mn = Math.min(a, b, c, d), mx = Math.max(a, b, c, d);
				if (mx === mn) continue;
				const k0 = Math.ceil(mn / interval - 1e-9), k1 = Math.floor(mx / interval + 1e-9);
				for (let k = k0; k <= k1; k++) {
					const L = k * interval, major = (k % 5 === 0) ? 1 : 0;
					const code = (a >= L ? 1 : 0) | (b >= L ? 2 : 0) | (c >= L ? 4 : 0) | (d >= L ? 8 : 0);
					if (code === 0 || code === 15) continue;
					// 四条边上的交点：0上(a-b)、1右(b-c)、2下(d-c)、3左(a-d)
					const edge = (i) => {
						if (i === 0) return [x + (L - a) / (b - a), y];
						if (i === 1) return [x + 1, y + (L - b) / (c - b)];
						if (i === 2) return [x + (L - d) / (c - d), y + 1];
						return [x, y + (L - a) / (d - a)];
					};
					const seg = (i, j) => { const p = edge(i), q = edge(j); out.push(p[0], p[1], q[0], q[1], major); };
					switch (code) {
						case 1: case 14: seg(3, 0); break;
						case 2: case 13: seg(0, 1); break;
						case 3: case 12: seg(3, 1); break;
						case 4: case 11: seg(1, 2); break;
						case 6: case 9: seg(0, 2); break;
						case 7: case 8: seg(3, 2); break;
						case 5: { const center = (a + b + c + d) / 4 >= L; if (center) { seg(0, 1); seg(2, 3); } else { seg(3, 0); seg(1, 2); } break; }
						case 10: { const center = (a + b + c + d) / 4 >= L; if (center) { seg(3, 0); seg(1, 2); } else { seg(0, 1); seg(2, 3); } break; }
					}
				}
			}
		}
		return { segs: Float32Array.from(out), interval };
	}

	// 稳定的随机颜色(字符串哈希→HSL)，plain固定为偏灰的绿。返回[r,g,b] 0-255。
	function colorOf(name, overrides) {
		if (overrides && overrides[name]) return overrides[name];
		if (name === 'plain') return [146, 168, 134];
		if (name === 'construction') return [170, 170, 150];
		if (name === 'terrain_mountain') return [135, 115, 95];
		if (name === 'terrain_ocean') return [60, 110, 190];
		if (name === 'terrain_river') return [90, 160, 220];
		let hash = 2166136261;
		for (let i = 0; i < name.length; i++) { hash ^= name.charCodeAt(i); hash = Math.imul(hash, 16777619); }
		const hue = ((hash >>> 0) % 360) / 360;
		const s = 0.55, l = 0.5;
		const f = (n) => {
			const k = (n + hue * 12) % 12;
			const a = s * Math.min(l, 1 - l);
			return Math.round(255 * (l - a * Math.max(-1, Math.min(k - 3, 9 - k, 1))));
		};
		return [f(0), f(8), f(4)];
	}

	ME.SEA_LEVEL = SEA_LEVEL;
	ME.Terrain = Terrain;
	ME.isFlatName = isFlatName;
	ME.colorOf = colorOf;
	ME.contourSegments = contourSegments;
	if (typeof module !== 'undefined') module.exports = ME;
})(typeof window !== 'undefined' ? window : globalThis);
