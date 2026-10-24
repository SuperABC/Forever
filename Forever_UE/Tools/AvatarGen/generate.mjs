// 批量生成市民头像：DiceBear avataaars(Pablo Stanley，免费个人/商用)程序化拼装 → 128x128 PNG。
// 每张图都带年龄标注(文件名 a{年龄:3位}_{编号:5位}.png)，游戏里按市民年龄挑年龄相符的头像。
// 同一个 (性别, 编号) 永远生成同一张图，可重复。用法见README.md。
import { createAvatar } from "@dicebear/core";
import { avataaars } from "@dicebear/collection";
import { Resvg } from "@resvg/resvg-js";
import fs from "node:fs";
import path from "node:path";
import os from "node:os";
import { Worker, isMainThread, parentPort, workerData } from "node:worker_threads";

const args = Object.fromEntries(process.argv.slice(2).map((a, i, all) =>
	a.startsWith("--") ? [a.slice(2), all[i + 1] && !all[i + 1].startsWith("--") ? all[i + 1] : "true"] : []).filter(e => e.length));
const COUNT = parseInt(args.count ?? "10000"); // 每个性别的张数
const SIZE = parseInt(args.size ?? "128");
const OUT = path.resolve(args.out ?? "../../Resource/Avatars");
const SAMPLE = args.sample ? parseInt(args.sample) : 0; // >0：每个年龄段出这么多张样图，只拼对比图不写 Resource

// ---- 年龄段：min/max 是该段头像标注的年龄范围，weight 是占一个性别 COUNT 张的比例 ----
// 游戏里按市民年龄落到同一段(UForeverAvatarSubsystem里的kBands必须和这里保持一致)再从段内挑。
const BANDS = [
	{ min: 3, max: 5, weight: 0.03, scale: 74, shiftY: 12 },
	{ min: 6, max: 12, weight: 0.09, scale: 82, shiftY: 9 },
	{ min: 13, max: 17, weight: 0.07, scale: 94, shiftY: 3 },
	{ min: 18, max: 29, weight: 0.22, scale: 100, shiftY: 0 },
	{ min: 30, max: 44, weight: 0.22, scale: 100, shiftY: 0 },
	{ min: 45, max: 59, weight: 0.18, scale: 100, shiftY: 0 },
	{ min: 60, max: 74, weight: 0.13, scale: 100, shiftY: 0 },
	{ min: 75, max: 90, weight: 0.06, scale: 100, shiftY: 0 },
];

// ---- 可复现的随机数：mulberry32，种子来自 "gender-index" 的字符串哈希 ----
function hash(str) {
	let h = 2166136261;
	for (let i = 0; i < str.length; i++) { h ^= str.charCodeAt(i); h = Math.imul(h, 16777619); }
	return h >>> 0;
}
function rng(seed) {
	let a = seed;
	return () => {
		a = (a + 0x6D2B79F5) | 0;
		let t = Math.imul(a ^ (a >>> 15), 1 | a);
		t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
		return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
	};
}
const pick = (r, list) => list[Math.floor(r() * list.length)];
const chance = (r, p) => r() < p;

// ---- 男女部件集合：用限定发型/胡须区分性别，其余(肤色/衣服/表情…)共用，保证画风一致 ----
const FEMALE_TOP = ["bob", "bun", "curly", "curvy", "dreads", "frida", "fro", "froBand", "longButNotTooLong",
	"miaWallace", "straight01", "straight02", "straightAndStrand", "bigHair", "bob", "straight01", "longButNotTooLong", "curvy"];
const FEMALE_RARE_TOP = ["hijab", "winterHat1", "winterHat02"];
const FEMALE_KID_TOP = ["bob", "bun", "curly", "curvy", "straight01", "straight02", "bob", "bun", "frida"];
const MALE_TOP = ["shavedSides", "dreads01", "dreads02", "frizzle", "shaggy", "shaggyMullet", "shortCurly", "shortFlat",
	"shortRound", "shortWaved", "sides", "theCaesar", "theCaesarAndSidePart", "shortFlat", "shortRound", "theCaesar", "sides"];
const MALE_RARE_TOP = ["hat", "turban", "winterHat1", "winterHat03", "winterHat04"];
const MALE_KID_TOP = ["shortFlat", "shortRound", "shortCurly", "shortWaved", "sides", "frizzle", "shortFlat", "shortRound"];
const EYES = ["default", "default", "default", "happy", "side", "squint", "surprised", "wink"];
const EYEBROWS = ["defaultNatural", "defaultNatural", "flatNatural", "raisedExcitedNatural", "upDownNatural", "default", "raisedExcited", "sadConcernedNatural", "frownNatural"];
const MOUTH = ["default", "smile", "smile", "twinkle", "serious", "concerned", "disbelief", "grimace", "sad"];
const KID_MOUTH = ["smile", "smile", "default", "twinkle", "smile"];
const CLOTHING = ["blazerAndShirt", "blazerAndSweater", "collarAndSweater", "graphicShirt", "hoodie", "overall", "shirtCrewNeck", "shirtScoopNeck", "shirtVNeck"];
const YOUNG_CLOTHING = ["graphicShirt", "hoodie", "overall", "shirtCrewNeck", "graphicShirt", "hoodie"];
const OLD_CLOTHING = ["blazerAndShirt", "blazerAndSweater", "collarAndSweater", "collarAndSweater", "shirtVNeck", "shirtCrewNeck", "blazerAndSweater"];
const GRAPHIC = ["bat", "bear", "cumbia", "deer", "diamond", "hola", "pizza", "resist", "skull", "skullOutline"];
const ACCESSORIES = ["kurt", "prescription01", "prescription02", "round", "sunglasses", "wayfarers"];
const FACIAL_HAIR = ["beardLight", "beardMajestic", "beardMedium", "moustacheFancy", "moustacheMagnum"];
// 只留写实肤色(去掉默认色板里偏橙/偏黄的卡通肤色)；常见肤色重复几次提高权重。
const SKIN = ["614335", "d08b5b", "ae5d29", "edb98a", "ffdbb4", "edb98a", "d08b5b", "ffdbb4"];
// 自然发色，黑/棕多、金/红少(去掉粉色)；灰白发色单独按年龄挑。
const HAIR = ["2c1b18", "2c1b18", "4a312c", "4a312c", "724133", "724133", "a55728", "b58143", "d6b370", "ecdcbf", "c93305"];
const GREY_HAIR = ["e8e1e1", "e8e1e1", "ecdcbf", "929598"];
const CLOTHES = ["262e33", "65c9ff", "5199e4", "25557c", "e6e6e6", "929598", "3c4f5c", "b1e2ff", "a7ffc4", "ffafb9", "ffffb1", "ff488e", "ff5c5c", "ffffff"];
const BACKGROUND = ["dbeafe", "fde68a", "fecaca", "d9f99d", "e9d5ff", "fbcfe8", "bae6fd", "fed7aa", "ddd6fe", "c7f9cc", "e5e7eb", "ccfbf1"];

// 各年龄段的外观倾向：灰发概率、男性胡须概率、眼镜概率。
function traits(band) {
	if (band.max <= 12) return { grey: 0, beard: 0, glasses: 0.04 };
	if (band.max <= 17) return { grey: 0, beard: 0, glasses: 0.1 };
	if (band.max <= 29) return { grey: 0, beard: 0.3, glasses: 0.2 };
	if (band.max <= 44) return { grey: 0.05, beard: 0.4, glasses: 0.22 };
	if (band.max <= 59) return { grey: 0.35, beard: 0.45, glasses: 0.35 };
	if (band.max <= 74) return { grey: 0.75, beard: 0.45, glasses: 0.55 };
	return { grey: 0.92, beard: 0.4, glasses: 0.65 };
}

function options(gender, index, band, age) {
	const r = rng(hash(`${gender}-${index}`));
	const male = gender === "male";
	const kid = band.max <= 12;
	const t = traits(band);
	const rareTop = !kid && chance(r, 0.08);
	const top = rareTop ? pick(r, male ? MALE_RARE_TOP : FEMALE_RARE_TOP)
		: pick(r, kid ? (male ? MALE_KID_TOP : FEMALE_KID_TOP) : (male ? MALE_TOP : FEMALE_TOP));
	const hairColor = chance(r, t.grey) ? pick(r, GREY_HAIR) : pick(r, HAIR);
	const clothingList = band.max <= 17 ? YOUNG_CLOTHING : band.min >= 45 ? OLD_CLOTHING : CLOTHING;
	const opts = {
		seed: `${gender}-${index}`,
		size: SIZE,
		scale: band.scale,
		translateY: band.shiftY,
		top: [top],
		hairColor: [hairColor],
		hatColor: [pick(r, CLOTHES)],
		skinColor: [pick(r, SKIN)],
		eyes: [pick(r, EYES)],
		eyebrows: [pick(r, EYEBROWS)],
		mouth: [pick(r, kid ? KID_MOUTH : MOUTH)],
		clothing: [pick(r, clothingList)],
		clothingGraphic: [pick(r, GRAPHIC)],
		clothesColor: [pick(r, CLOTHES)],
		backgroundColor: [pick(r, BACKGROUND)],
		style: ["default"],
		topProbability: 100,
		accessories: [pick(r, ACCESSORIES)],
		accessoriesProbability: chance(r, t.glasses) ? 100 : 0,
		accessoriesColor: [pick(r, ["262e33", "3c4f5c", "929598", "e6e6e6", "5199e4", "ff5c5c"])],
		facialHair: [pick(r, FACIAL_HAIR)],
		facialHairProbability: male && chance(r, t.beard) ? 100 : 0,
		facialHairColor: [hairColor],
	};
	return opts;
}

function renderPng(gender, index, band, age) {
	const svg = createAvatar(avataaars, options(gender, index, band, age)).toString();
	return new Resvg(svg, { fitTo: { mode: "width", value: SIZE } }).render().asPng();
}

// 把 COUNT 张按权重分配到各年龄段(最后一段补足余数)，返回每张图的 {band, age}，按年龄从小到大。
function plan(gender) {
	const counts = BANDS.map(b => Math.round(COUNT * b.weight));
	counts[counts.length - 1] += COUNT - counts.reduce((a, b) => a + b, 0);
	const list = [];
	BANDS.forEach((band, b) => {
		for (let i = 0; i < counts[b]; i++) {
			// 年龄在段内按顺序均匀铺开，保证每个整数年龄都有图。
			const age = band.min + Math.floor(i * (band.max - band.min + 1) / counts[b]);
			list.push({ band, age });
		}
	});
	return list.map((e, index) => ({ ...e, index }));
}

const fileName = (age, index) => `a${String(age).padStart(3, "0")}_${String(index).padStart(5, "0")}.png`;

if (!isMainThread) {
	// 工作线程：渲染分给自己的那一份，写完通知主线程。
	for (const e of workerData.items) {
		fs.writeFileSync(path.join(workerData.dir, fileName(e.age, e.index)), renderPng(workerData.gender, e.index, BANDS[e.bandIndex], e.age));
		parentPort.postMessage(1);
	}
}
else if (SAMPLE > 0) {
	// 样图：每个年龄段每个性别 SAMPLE 张，左半男右半女，每行一个年龄段(从小到大)。
	const width = 2 * SAMPLE * SIZE, height = BANDS.length * SIZE;
	let body = `<rect width="${width}" height="${height}" fill="#ffffff"/>`;
	["male", "female"].forEach((gender, g) => {
		const all = plan(gender);
		BANDS.forEach((band, b) => {
			const members = all.filter(e => e.band === band);
			for (let i = 0; i < SAMPLE; i++) {
				const e = members[Math.floor(i * members.length / SAMPLE)];
				const png = renderPng(gender, e.index, e.band, e.age).toString("base64");
				body += `<image x="${(g * SAMPLE + i) * SIZE}" y="${b * SIZE}" width="${SIZE}" height="${SIZE}" href="data:image/png;base64,${png}"/>`;
			}
		});
	});
	const sheet = new Resvg(`<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="${width}" height="${height}">${body}</svg>`).render().asPng();
	const file = path.resolve(args.sheet ?? "sample.png");
	fs.writeFileSync(file, sheet);
	console.log(`sample sheet: ${file} (${width}x${height}); left male, right female; rows = ages ${BANDS.map(b => `${b.min}-${b.max}`).join(", ")}`);
}
else {
	// 单线程渲染一张要几百毫秒，2万张太慢，按CPU核数开工作线程并行(留两个核给系统)。
	const threads = Math.max(1, os.cpus().length - 2);
	let done = 0;
	const total = COUNT * 2;
	for (const gender of ["male", "female"]) {
		const dir = path.join(OUT, gender);
		fs.rmSync(dir, { recursive: true, force: true }); // 清掉旧文件，避免残留旧命名/旧张数
		fs.mkdirSync(dir, { recursive: true });
		const items = plan(gender).map(e => ({ index: e.index, age: e.age, bandIndex: BANDS.indexOf(e.band) }));
		await Promise.all(Array.from({ length: threads }, (_, t) => new Promise((resolve, reject) => {
			const slice = items.filter((_, n) => n % threads === t);
			const worker = new Worker(new URL(import.meta.url), { workerData: { gender, dir, items: slice }, argv: process.argv.slice(2) });
			worker.on("message", () => { if (++done % 1000 === 0) console.log(`${done}/${total}`); });
			worker.on("error", reject);
			worker.on("exit", resolve);
		})));
	}
	console.log(`done: ${COUNT} x 2 -> ${OUT}`);
}
