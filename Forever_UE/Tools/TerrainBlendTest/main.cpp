// 近处地形混合算法的独立验证程序：脱离UE，只依赖标准库和Core/map/terrain_blend.cpp。
// 说明见同目录README.md。

#include "terrain_blend.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

using namespace std;
using Clock = chrono::steady_clock;

namespace {

// ---------------------------------------------------------------- 日志与断言

FILE* gLogFile = nullptr;

void Log(const char* fmt, ...) {
	char buffer[2048];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	fputs(buffer, stdout);
	if (gLogFile) fputs(buffer, gLogFile);
}

struct Checker {
	int checks = 0;
	int failures = 0;

	void Report(const string& name, bool ok, const char* fmt = "", ...) {
		char detail[1024];
		va_list args;
		va_start(args, fmt);
		vsnprintf(detail, sizeof(detail), fmt, args);
		va_end(args);
		checks++;
		if (!ok) failures++;
		Log("  [%s] %s%s%s\n", ok ? "PASS" : "FAIL", name.c_str(), detail[0] ? " -- " : "", detail);
	}
};

// ---------------------------------------------------------------- 确定性随机数与噪声

struct Rng {
	uint64_t state;
	explicit Rng(uint64_t seed) : state(seed * 0x9E3779B97F4A7C15ULL + 1442695040888963407ULL) {}
	uint64_t Next() {
		state += 0x9E3779B97F4A7C15ULL;
		uint64_t z = state;
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
		return z ^ (z >> 31);
	}
	float Uniform() { return static_cast<float>(Next() >> 40) / 16777216.f; }
	int Range(int n) { return static_cast<int>(Next() % static_cast<uint64_t>(n)); }
};

float HashFloat(int x, int y, uint32_t seed) {
	uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u + seed * 2246822519u;
	h = (h ^ (h >> 13)) * 1274126177u;
	h ^= h >> 16;
	return static_cast<float>(h & 0xFFFFFFu) / 16777216.f;
}

float ValueNoise(float x, float y, uint32_t seed) {
	int xi = static_cast<int>(floorf(x));
	int yi = static_cast<int>(floorf(y));
	float tx = x - static_cast<float>(xi);
	float ty = y - static_cast<float>(yi);
	tx = tx * tx * (3.f - 2.f * tx);
	ty = ty * ty * (3.f - 2.f * ty);
	float a = HashFloat(xi, yi, seed);
	float b = HashFloat(xi + 1, yi, seed);
	float c = HashFloat(xi, yi + 1, seed);
	float d = HashFloat(xi + 1, yi + 1, seed);
	return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
}

float Fbm(float x, float y, uint32_t seed, int octaves) {
	float amplitude = 0.5f, frequency = 1.f, sum = 0.f, norm = 0.f;
	for (int o = 0; o < octaves; o++) {
		sum += amplitude * ValueNoise(x * frequency, y * frequency, seed + static_cast<uint32_t>(o) * 101u);
		norm += amplitude;
		frequency *= 2.f;
		amplitude *= 0.5f;
	}
	return sum / norm;
}

// ---------------------------------------------------------------- 合成地图

constexpr int ID_PLAIN = 0;
constexpr int ID_OCEAN = 2;
constexpr int ID_MOUNTAIN = 3;
constexpr int MAX_COLOR_IDS = 8;

const uint8_t kColors[MAX_COLOR_IDS][3] = {
	{ 112, 150, 72 },   // 0 plain / construction
	{ 255, 0, 255 },    // 1 未使用
	{ 52, 92, 160 },    // 2 ocean
	{ 128, 122, 116 },  // 3 mountain
	{ 214, 176, 64 },   // 4
	{ 180, 70, 70 },    // 5
	{ 96, 64, 140 },    // 6
	{ 40, 170, 160 },   // 7
};

struct TerrainMap {
	int width = 0;
	int height = 0;
	vector<uint8_t> ids;
	vector<uint8_t> construction;

	uint8_t& Id(int x, int y) { return ids[static_cast<size_t>(y) * width + x]; }
	uint8_t Id(int x, int y) const { return ids[static_cast<size_t>(y) * width + x]; }

	NearBlendRegion Region(int originX, int originY, int size) const {
		NearBlendRegion region;
		region.originX = originX;
		region.originY = originY;
		region.size = size;
		region.mapWidth = width;
		region.mapHeight = height;
		region.sliceIds = ids.data();
		region.isConstruction = construction.data();
		return region;
	}
};

TerrainMap MakeMap(int width, int height, uint8_t fill) {
	TerrainMap map;
	map.width = width;
	map.height = height;
	map.ids.assign(static_cast<size_t>(width) * height, fill);
	map.construction.assign(static_cast<size_t>(width) * height, 0);
	return map;
}

// 照抄Map::InitTerrains的晋升规则：3x3邻域全是plain/construction(槽位0)且不越界。
void ApplyConstructionRule(TerrainMap& map) {
	for (int y = 0; y < map.height; y++) {
		for (int x = 0; x < map.width; x++) {
			bool all = true;
			for (int dy = -1; dy <= 1 && all; dy++) {
				for (int dx = -1; dx <= 1 && all; dx++) {
					int nx = x + dx, ny = y + dy;
					if (nx < 0 || nx >= map.width || ny < 0 || ny >= map.height) all = false;
					else if (map.Id(nx, ny) != ID_PLAIN) all = false;
				}
			}
			map.construction[static_cast<size_t>(y) * map.width + x] = all ? 1 : 0;
		}
	}
}

TerrainMap GenerateMainMap(int size, uint32_t seed) {
	TerrainMap map = MakeMap(size, size, ID_PLAIN);
	const float scale = static_cast<float>(size) / 1024.f;
	for (int y = 0; y < size; y++) {
		for (int x = 0; x < size; x++) {
			float fx = static_cast<float>(x) / scale, fy = static_cast<float>(y) / scale;
			float mountain = Fbm(fx / 90.f, fy / 90.f, seed + 1, 4);
			float coast = Fbm(fx / 50.f, fy / 50.f, seed + 2, 3);
			float lake = Fbm(fx / 70.f, fy / 70.f, seed + 3, 3);

			int id = ID_PLAIN;
			if (mountain > 0.58f) id = ID_MOUNTAIN;
			float westCoast = (70.f + 120.f * (coast - 0.5f)) * scale;
			float southCoast = (50.f + 120.f * (coast - 0.5f)) * scale;
			if (static_cast<float>(x) < westCoast || static_cast<float>(y) < southCoast) id = ID_OCEAN;
			else if (lake > 0.70f && id != ID_MOUNTAIN) id = ID_OCEAN;
			map.Id(x, y) = static_cast<uint8_t>(id);
		}
	}
	ApplyConstructionRule(map);
	return map;
}

TerrainMap GenerateStressMap(int size, uint32_t seed) {
	TerrainMap map = MakeMap(size, size, ID_PLAIN);
	Rng rng(seed + 77);
	const int ids[7] = { 0, 2, 3, 4, 5, 6, 7 };

	// 抖动网格的Voronoi大块
	const int grid = max(size / 32, 2);
	const float cell = static_cast<float>(size) / static_cast<float>(grid);
	struct Site { float x, y; int id; };
	vector<Site> sites;
	for (int gy = 0; gy < grid; gy++) {
		for (int gx = 0; gx < grid; gx++) {
			sites.push_back({ (static_cast<float>(gx) + rng.Uniform()) * cell, (static_cast<float>(gy) + rng.Uniform()) * cell, ids[rng.Range(7)] });
		}
	}
	for (int y = 0; y < size; y++) {
		for (int x = 0; x < size; x++) {
			int gx = min(static_cast<int>(static_cast<float>(x) / cell), grid - 1);
			int gy = min(static_cast<int>(static_cast<float>(y) / cell), grid - 1);
			float best = 1e30f;
			int bestId = 0;
			for (int sy = max(gy - 2, 0); sy <= min(gy + 2, grid - 1); sy++) {
				for (int sx = max(gx - 2, 0); sx <= min(gx + 2, grid - 1); sx++) {
					const Site& site = sites[static_cast<size_t>(sy) * grid + sx];
					float dx = static_cast<float>(x) + 0.5f - site.x, dy = static_cast<float>(y) + 0.5f - site.y;
					float d = dx * dx + dy * dy;
					if (d < best) { best = d; bestId = site.id; }
				}
			}
			map.Id(x, y) = static_cast<uint8_t>(bestId);
		}
	}

	// 乱码块：每个element随机一种id，专门制造"半径内多于四种地形"
	int confetti = max(size / 50, 2);
	for (int i = 0; i < confetti; i++) {
		int w = 48, h = 48;
		int ox = rng.Range(max(size - w, 1)), oy = rng.Range(max(size - h, 1));
		for (int y = oy; y < min(oy + h, size); y++)
			for (int x = ox; x < min(ox + w, size); x++)
				map.Id(x, y) = static_cast<uint8_t>(ids[rng.Range(7)]);
	}
	// 棋盘格小块
	int checker = max(size / 80, 2);
	for (int i = 0; i < checker; i++) {
		int w = 12, h = 12;
		int ox = rng.Range(max(size - w, 1)), oy = rng.Range(max(size - h, 1));
		int a = ids[rng.Range(7)], b = ids[rng.Range(7)];
		for (int y = oy; y < min(oy + h, size); y++)
			for (int x = ox; x < min(ox + w, size); x++)
				map.Id(x, y) = static_cast<uint8_t>(((x + y) & 1) ? a : b);
	}
	// 单格孤岛和一格宽的细线
	int specks = size * size / 3000;
	for (int i = 0; i < specks; i++) map.Id(rng.Range(size), rng.Range(size)) = static_cast<uint8_t>(ids[rng.Range(7)]);
	int lines = max(size / 25, 2);
	for (int i = 0; i < lines; i++) {
		int id = ids[rng.Range(7)];
		bool horizontal = rng.Range(2) == 0;
		int x = rng.Range(size), y = rng.Range(size), length = size / 5;
		for (int k = 0; k < length; k++) {
			int px = horizontal ? min(x + k, size - 1) : x;
			int py = horizontal ? y : min(y + k, size - 1);
			map.Id(px, py) = static_cast<uint8_t>(id);
		}
	}
	ApplyConstructionRule(map);
	return map;
}

// ---------------------------------------------------------------- 图像输出

struct Image {
	int width = 0;
	int height = 0;
	vector<uint8_t> rgb;

	Image(int w, int h) : width(w), height(h), rgb(static_cast<size_t>(w) * h * 3, 0) {}
	// 地图坐标系(y向上)写入：mapY=0在图像最下面。
	void Set(int x, int mapY, const uint8_t c[3]) {
		size_t index = (static_cast<size_t>(height - 1 - mapY) * width + x) * 3;
		rgb[index] = c[0];
		rgb[index + 1] = c[1];
		rgb[index + 2] = c[2];
	}
};

uint32_t gCrcTable[256];

void InitCrc() {
	for (uint32_t n = 0; n < 256; n++) {
		uint32_t c = n;
		for (int k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
		gCrcTable[n] = c;
	}
}

uint32_t Crc32(const uint8_t* data, size_t length, uint32_t crc = 0xFFFFFFFFu) {
	for (size_t i = 0; i < length; i++) crc = gCrcTable[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
	return crc;
}

void PutU32(vector<uint8_t>& out, uint32_t v) {
	out.push_back(static_cast<uint8_t>(v >> 24));
	out.push_back(static_cast<uint8_t>(v >> 16));
	out.push_back(static_cast<uint8_t>(v >> 8));
	out.push_back(static_cast<uint8_t>(v));
}

void WriteChunk(FILE* file, const char* type, const vector<uint8_t>& data) {
	vector<uint8_t> header;
	PutU32(header, static_cast<uint32_t>(data.size()));
	fwrite(header.data(), 1, 4, file);
	vector<uint8_t> body(type, type + 4);
	body.insert(body.end(), data.begin(), data.end());
	fwrite(body.data(), 1, body.size(), file);
	vector<uint8_t> crc;
	PutU32(crc, ~Crc32(body.data(), body.size()));
	fwrite(crc.data(), 1, 4, file);
}

// 最小PNG写出器：无压缩的deflate存储块，不依赖任何库。
bool WritePng(const string& path, const Image& image) {
	FILE* file = fopen(path.c_str(), "wb");
	if (!file) return false;
	const uint8_t signature[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
	fwrite(signature, 1, 8, file);

	vector<uint8_t> ihdr;
	PutU32(ihdr, static_cast<uint32_t>(image.width));
	PutU32(ihdr, static_cast<uint32_t>(image.height));
	ihdr.push_back(8);
	ihdr.push_back(2);
	ihdr.push_back(0);
	ihdr.push_back(0);
	ihdr.push_back(0);
	WriteChunk(file, "IHDR", ihdr);

	vector<uint8_t> raw;
	raw.reserve(static_cast<size_t>(image.height) * (static_cast<size_t>(image.width) * 3 + 1));
	for (int y = 0; y < image.height; y++) {
		raw.push_back(0);
		raw.insert(raw.end(), image.rgb.begin() + static_cast<size_t>(y) * image.width * 3,
			image.rgb.begin() + static_cast<size_t>(y + 1) * image.width * 3);
	}

	vector<uint8_t> z;
	z.push_back(0x78);
	z.push_back(0x01);
	size_t position = 0;
	while (position < raw.size()) {
		size_t length = min<size_t>(65535, raw.size() - position);
		bool last = position + length >= raw.size();
		z.push_back(last ? 1 : 0);
		z.push_back(static_cast<uint8_t>(length & 0xFF));
		z.push_back(static_cast<uint8_t>(length >> 8));
		z.push_back(static_cast<uint8_t>(~length & 0xFF));
		z.push_back(static_cast<uint8_t>((~length >> 8) & 0xFF));
		z.insert(z.end(), raw.begin() + static_cast<long>(position), raw.begin() + static_cast<long>(position + length));
		position += length;
	}
	uint32_t a = 1, b = 0;
	for (uint8_t byte : raw) {
		a = (a + byte) % 65521u;
		b = (b + a) % 65521u;
	}
	PutU32(z, (b << 16) | a);
	WriteChunk(file, "IDAT", z);
	WriteChunk(file, "IEND", {});
	fclose(file);
	return true;
}

void Save(const string& dir, const string& name, const Image& image) {
	string path = dir + "/" + name;
	if (!WritePng(path, image)) Log("  !! 写出失败: %s\n", path.c_str());
	else Log("  写出 %s (%dx%d)\n", name.c_str(), image.width, image.height);
}

void HeatColor(float v, uint8_t out[3]) {
	v = min(max(v, 0.f), 1.f);
	float r = min(1.f, v * 2.f);
	float g = max(0.f, v * 2.f - 1.f);
	float b = max(0.f, v * 4.f - 3.f);
	out[0] = static_cast<uint8_t>(r * 255.f);
	out[1] = static_cast<uint8_t>(g * 255.f);
	out[2] = static_cast<uint8_t>(b * 255.f);
}

// ---------------------------------------------------------------- 比较与对照实现

int WeightOfId(const uint8_t* ids, const uint8_t* weights, uint8_t id) {
	int sum = 0;
	for (int k = 0; k < 4; k++) if (ids[k] == id) sum += weights[k];
	return sum;
}

// 按"每个id的权重"(而不是槽位序号)比较两个texel，返回最大差值(单位1/255)。
int TexelDiff(const uint8_t* ia, const uint8_t* wa, const uint8_t* ib, const uint8_t* wb) {
	if (memcmp(ia, ib, 4) == 0 && memcmp(wa, wb, 4) == 0) return 0;
	int best = 0;
	for (int k = 0; k < 4; k++) {
		best = max(best, abs(WeightOfId(ia, wa, ia[k]) - WeightOfId(ib, wb, ia[k])));
		best = max(best, abs(WeightOfId(ia, wa, ib[k]) - WeightOfId(ib, wb, ib[k])));
	}
	return best;
}

// 用double按定义独立实现一遍(不查表、不走快速路径、不做槽位整理)，用来校验生产代码。
map<int, double> BruteForce(const TerrainMap& terrain, double px, double py, double radius, double rampWidth) {
	map<int, double> acc;
	double total = 0.0;
	int x0 = static_cast<int>(floor(px - radius)) - 1, x1 = static_cast<int>(floor(px + radius)) + 1;
	int y0 = static_cast<int>(floor(py - radius)) - 1, y1 = static_cast<int>(floor(py + radius)) + 1;
	for (int y = y0; y <= y1; y++) {
		for (int x = x0; x <= x1; x++) {
			double d = hypot(px - (x + 0.5), py - (y + 0.5));
			double t = d / radius;
			if (t >= 1.0) continue;
			double w = (1.0 - t * t) * (1.0 - t * t);
			if (w <= 0.0) continue;
			int cx = min(max(x, 0), terrain.width - 1), cy = min(max(y, 0), terrain.height - 1);
			acc[terrain.Id(cx, cy)] += w;
			total += w;
		}
	}
	vector<pair<double, int>> sorted;
	for (auto& entry : acc) sorted.push_back({ entry.second / total, entry.first });

	// construction边界的拉力(只在radius>1.5时启用)：权重向最近construction的槽位线性拉升
	if (rampWidth > 0.0 && radius > 1.5) {
		int reach = static_cast<int>(ceil(radius + 0.5)) - 1;
		int ex = static_cast<int>(floor(px)), ey = static_cast<int>(floor(py));
		double ux = px - ex, uy = py - ey, best = 0.0;
		int bestId = 0;
		for (int dy = -reach; dy <= reach; dy++) {
			for (int dx = -reach; dx <= reach; dx++) {
				int cx = min(max(ex + dx, 0), terrain.width - 1), cy = min(max(ey + dy, 0), terrain.height - 1);
				if (!terrain.construction[static_cast<size_t>(cy) * terrain.width + cx]) continue;
				double gapX = max(max(dx - ux, ux - (dx + 1)), 0.0), gapY = max(max(dy - uy, uy - (dy + 1)), 0.0);
				double pull = 1.0 - hypot(gapX, gapY) / rampWidth;
				if (pull > best) { best = pull; bestId = terrain.Id(cx, cy); }
			}
		}
		if (best > 0.0) {
			bool found = false;
			for (auto& entry : sorted) {
				entry.first *= (1.0 - best);
				if (entry.second == bestId) { entry.first += best; found = true; }
			}
			if (!found) sorted.push_back({ best, bestId });
		}
	}
	sort(sorted.begin(), sorted.end(), [](const pair<double, int>& a, const pair<double, int>& b) {
		if (a.first != b.first) return a.first > b.first;
		return a.second < b.second;
		});
	map<int, double> result;
	if (sorted.size() <= 4) {
		for (auto& entry : sorted) result[entry.second] = entry.first;
		return result;
	}
	double fifth = sorted[4].first, sum = 0.0;
	double shifted[4];
	for (int i = 0; i < 4; i++) {
		shifted[i] = max(sorted[i].first - fifth, 0.0);
		sum += shifted[i];
	}
	if (sum > 1e-9) {
		for (int i = 0; i < 4; i++) result[sorted[i].second] = shifted[i] / sum;
	}
	else {
		double rawSum = 0.0;
		for (int i = 0; i < 4; i++) rawSum += sorted[i].first;
		for (int i = 0; i < 4; i++) result[sorted[i].second] = sorted[i].first / rawSum;
	}
	return result;
}

// 生产代码的一个texel与BruteForce的最大差异(单位1/255)。
double CompareWithBruteForce(const TerrainMap& terrain, const NearBlendRegion& region, int tpe, double radius,
	int texelX, int texelY, const uint8_t* ids, const uint8_t* weights, double rampWidth = 0.0) {
	double px = region.originX + (texelX + 0.5) / tpe;
	double py = region.originY + (texelY + 0.5) / tpe;
	map<int, double> expected = BruteForce(terrain, px, py, radius, rampWidth);
	double worst = 0.0;
	map<int, bool> seen;
	for (auto& entry : expected) {
		seen[entry.first] = true;
		double actual = WeightOfId(ids, weights, static_cast<uint8_t>(entry.first)) / 255.0;
		worst = max(worst, fabs(actual - entry.second) * 255.0);
	}
	for (int k = 0; k < 4; k++) {
		if (!seen.count(ids[k]) && weights[k] > 0) worst = max(worst, static_cast<double>(weights[k]));
	}
	return worst;
}

void ColorOf(const uint8_t* ids, const uint8_t* weights, uint8_t out[3]) {
	float r = 0.f, g = 0.f, b = 0.f;
	for (int k = 0; k < 4; k++) {
		if (weights[k] == 0) continue;
		const uint8_t* c = kColors[ids[k] % MAX_COLOR_IDS];
		float w = static_cast<float>(weights[k]) / 255.f;
		r += w * c[0];
		g += w * c[1];
		b += w * c[2];
	}
	out[0] = static_cast<uint8_t>(min(r + 0.5f, 255.f));
	out[1] = static_cast<uint8_t>(min(g + 0.5f, 255.f));
	out[2] = static_cast<uint8_t>(min(b + 0.5f, 255.f));
}

// ---------------------------------------------------------------- 解析用例

constexpr double kTolerance = 3.0;  // 与double对照的容差，单位1/255(量化、排序并列造成的差异)

void RunAnalyticTests(double radius, int tpe, Checker& checker) {
	NearBlendParams params;
	params.texelsPerElement = tpe;
	params.radius = static_cast<float>(radius);
	params.constructionGuard = false;
	NearTerrainBlend blend(params);
	Log("解析用例 (radius=%.2f, texels/element=%d)\n", radius, tpe);

	// A. 同质区域
	{
		TerrainMap map = MakeMap(40, 40, 5);
		NearBlendRegion region = map.Region(10, 10, 8);
		vector<uint8_t> ids, weights, ids2, weights2;
		blend.BuildRegion(region, ids, weights, nullptr, true);
		blend.BuildRegion(region, ids2, weights2, nullptr, false);
		bool pure = true;
		for (size_t i = 0; i < ids.size(); i += 4) {
			if (ids[i] != 5 || weights[i] != 255 || weights[i + 1] || weights[i + 2] || weights[i + 3]) pure = false;
		}
		checker.Report("A 同质区域输出纯色", pure);
		checker.Report("A 同质区域快速/慢速路径一致", ids == ids2 && weights == weights2);
	}

	// B. 一条直边：x<32为id0，其余为id3
	{
		TerrainMap map = MakeMap(64, 64, 0);
		for (int y = 0; y < 64; y++) for (int x = 32; x < 64; x++) map.Id(x, y) = 3;
		NearBlendRegion region = map.Region(16, 16, 32);
		const int res = 32 * tpe;
		vector<uint8_t> ids, weights;
		blend.BuildRegion(region, ids, weights);
		const int row = res / 2;
		vector<int> w3(res);
		double worst = 0.0;
		for (int tx = 0; tx < res; tx++) {
			size_t base = (static_cast<size_t>(row) * res + tx) * 4;
			w3[tx] = WeightOfId(&ids[base], &weights[base], 3);
			worst = max(worst, CompareWithBruteForce(map, region, tpe, radius, tx, row, &ids[base], &weights[base]));
		}
		bool monotone = true, symmetric = true;
		for (int tx = 1; tx < res; tx++) if (w3[tx] < w3[tx - 1] - 1) monotone = false;
		int worstSymmetry = 0;
		for (int tx = 0; tx < res; tx++) {
			int d = abs(w3[tx] + w3[res - 1 - tx] - 255);
			worstSymmetry = max(worstSymmetry, d);
			if (d > 2) symmetric = false;
		}
		int left = w3[res / 2 - 1], right = w3[res / 2];
		int x5 = -1, x95 = -1;
		for (int tx = 0; tx < res; tx++) {
			if (x5 < 0 && w3[tx] >= 0.05 * 255) x5 = tx;
			if (x95 < 0 && w3[tx] >= 0.95 * 255) x95 = tx;
		}
		checker.Report("B 直边: 权重单调", monotone);
		checker.Report("B 直边: 左右对称", symmetric, "最大偏差 %d/255", worstSymmetry);
		checker.Report("B 直边: 边界两侧texel约50/50", left >= 0.45 * 255 && left <= 0.55 * 255 && right >= 0.45 * 255 && right <= 0.55 * 255,
			"左 %d 右 %d (/255)", left, right);
		checker.Report("B 直边: 与double对照", worst <= kTolerance, "最大差 %.2f/255", worst);
		Log("    过渡带(权重5%%~95%%)宽度 = %.2f element = %.1f 米\n", static_cast<double>(x95 - x5) / tpe, static_cast<double>(x95 - x5) / tpe * 10.0);
	}

	// C. 单个孤立element
	{
		TerrainMap map = MakeMap(64, 64, 0);
		map.Id(32, 32) = 3;
		NearBlendRegion region = map.Region(30, 30, 4);
		const int res = 4 * tpe;
		vector<uint8_t> ids, weights;
		blend.BuildRegion(region, ids, weights);
		int center = static_cast<int>(2.5 * tpe - 0.5);
		size_t base = (static_cast<size_t>(center) * res + center) * 4;
		double worst = CompareWithBruteForce(map, region, tpe, radius, center, center, &ids[base], &weights[base]);
		int own = WeightOfId(&ids[base], &weights[base], 3);
		checker.Report("C 孤立单格: 与double对照", worst <= kTolerance, "中心附近自身权重 %.3f, 最大差 %.2f/255", own / 255.0, worst);
	}

	// D. 一个9种地形挤在3x3里的路口：触发前四大截断
	{
		TerrainMap map = MakeMap(64, 64, 0);
		uint8_t id = 1;
		for (int y = 31; y <= 33; y++) for (int x = 31; x <= 33; x++) map.Id(x, y) = id++;
		NearBlendRegion region = map.Region(28, 28, 8);
		const int res = 8 * tpe;
		vector<uint8_t> ids, weights;
		NearBlendStats stats;
		blend.BuildRegion(region, ids, weights, &stats);
		double worst = 0.0;
		int worstAdjacent = 0;
		bool sumsOk = true;
		for (int ty = 0; ty < res; ty++) {
			for (int tx = 0; tx < res; tx++) {
				size_t base = (static_cast<size_t>(ty) * res + tx) * 4;
				int sum = weights[base] + weights[base + 1] + weights[base + 2] + weights[base + 3];
				if (sum != 255) sumsOk = false;
				worst = max(worst, CompareWithBruteForce(map, region, tpe, radius, tx, ty, &ids[base], &weights[base]));
				if (tx + 1 < res) worstAdjacent = max(worstAdjacent, TexelDiff(&ids[base], &weights[base], &ids[base + 4], &weights[base + 4]));
				if (ty + 1 < res) worstAdjacent = max(worstAdjacent, TexelDiff(&ids[base], &weights[base], &ids[base + static_cast<size_t>(res) * 4], &weights[base + static_cast<size_t>(res) * 4]));
			}
		}
		checker.Report("D 9种地形路口: 触发了前四大截断", stats.truncatedTexels > 0, "截断texel数 %lld / %lld", static_cast<long long>(stats.truncatedTexels), static_cast<long long>(stats.texelCount));
		checker.Report("D 9种地形路口: 权重和恒为255", sumsOk);
		checker.Report("D 9种地形路口: 与double对照", worst <= kTolerance, "最大差 %.2f/255", worst);
		Log("    [INFO] D 9种地形路口: texel分辨率下最大相邻差 %d/255 (%.1f%%), 连续性见用例E的细化测试\n", worstAdjacent, worstAdjacent / 2.55);
	}

	// E. 连续性的细化测试：场连续则texel越密相邻差越小(近似按1/texel数缩小)，有跳变则不随细化缩小。
	{
		TerrainMap stress = GenerateStressMap(128, 5);
		int bestX = 0, bestY = 0;
		long long bestTruncated = -1;
		{
			NearTerrainBlend probe(params);
			NearTerrainBlend coarse([&] { NearBlendParams q = params; q.texelsPerElement = 4; return q; }());
			for (int oy = 0; oy + 24 <= 128; oy += 12) {
				for (int ox = 0; ox + 24 <= 128; ox += 12) {
					vector<uint8_t> ids, weights;
					NearBlendStats stats;
					coarse.BuildRegion(stress.Region(ox, oy, 24), ids, weights, &stats);
					if (stats.truncatedTexels > bestTruncated) { bestTruncated = stats.truncatedTexels; bestX = ox; bestY = oy; }
				}
			}
		}
		auto ladder = [&](const char* name, const TerrainMap& map, int ox, int oy, int size) {
			int diffs[4];
			const int ladderTpe[4] = { 8, 16, 32, 64 };
			for (int i = 0; i < 4; i++) {
				NearBlendParams q = params;
				q.texelsPerElement = ladderTpe[i];
				NearTerrainBlend b(q);
				vector<uint8_t> ids, weights;
				b.BuildRegion(map.Region(ox, oy, size), ids, weights);
				const int res = size * ladderTpe[i];
				int worst = 0;
				for (int ty = 0; ty < res; ty++) {
					for (int tx = 0; tx < res; tx++) {
						size_t base = (static_cast<size_t>(ty) * res + tx) * 4;
						if (tx + 1 < res) worst = max(worst, TexelDiff(&ids[base], &weights[base], &ids[base + 4], &weights[base + 4]));
						if (ty + 1 < res) {
							size_t up = base + static_cast<size_t>(res) * 4;
							worst = max(worst, TexelDiff(&ids[base], &weights[base], &ids[up], &weights[up]));
						}
					}
				}
				diffs[i] = worst;
			}
			checker.Report(string("E 细化后相邻差随texel变密而缩小: ") + name, diffs[3] * 2 <= diffs[1],
				"texels/element=8/16/32/64 的最大相邻差 = %d/%d/%d/%d (/255)", diffs[0], diffs[1], diffs[2], diffs[3]);
		};
		ladder("压力地图(多种地形混杂的区域)", stress, bestX, bestY, 24);
		TerrainMap junction = MakeMap(64, 64, 0);
		{
			uint8_t id = 1;
			for (int y = 31; y <= 33; y++) for (int x = 31; x <= 33; x++) junction.Id(x, y) = id++;
		}
		ladder("9种地形路口", junction, 28, 28, 8);
		TerrainMap edge = MakeMap(64, 64, 0);
		for (int y = 0; y < 64; y++) for (int x = 32; x < 64; x++) edge.Id(x, y) = 3;
		ladder("两种地形的直边", edge, 24, 24, 16);
	}
}

// ---------------------------------------------------------------- 整图实验

struct Options {
	string outDir = "out";
	int tpe = 16;
	uint32_t seed = 1;
	int chunks = 64;
	vector<double> radii = { 1.5, 2.0 };
	bool radiusGiven = false;
	int maxAdjacentDiff = 20;
};

constexpr int CHUNK_ELEMENTS = 16;
constexpr int BLOCK_CHUNKS = 8;

struct RunResult {
	int maxAdjacent = 0;
};

RunResult RunExperiment(const string& mapName, const TerrainMap& map, double radius, bool thorough, bool enforceBound, const Options& options, Checker& checker) {
	const int tpe = options.tpe;
	const int chunksPerSide = options.chunks;
	const int mapSize = chunksPerSide * CHUNK_ELEMENTS;
	const int chunkRes = CHUNK_ELEMENTS * tpe;
	const int blockElements = BLOCK_CHUNKS * CHUNK_ELEMENTS;
	const int blockRes = blockElements * tpe;
	const int blocksPerSide = chunksPerSide / BLOCK_CHUNKS;

	char tag[64];
	snprintf(tag, sizeof(tag), "%s_r%.1f", mapName.c_str(), radius);
	Log("\n==== 整图实验 %s: 地图 %dx%d element, %dx%d 个chunk(每个 %dx%d), radius=%.2f, texels/element=%d ====\n",
		mapName.c_str(), mapSize, mapSize, chunksPerSide, chunksPerSide, CHUNK_ELEMENTS, CHUNK_ELEMENTS, radius, tpe);

	NearBlendParams params;
	params.texelsPerElement = tpe;
	params.radius = static_cast<float>(radius);
	params.constructionGuard = true;
	NearTerrainBlend blend(params);

	vector<array<uint32_t, MAX_COLOR_IDS>> elementWeights(static_cast<size_t>(mapSize) * mapSize);
	for (auto& e : elementWeights) e.fill(0);

	NearBlendStats stats;
	vector<uint8_t> chunkIds, chunkWeights, slowIds, slowWeights, blockIds, blockWeights;
	double chunkSeconds = 0.0, chunkMaxSeconds = 0.0, slowSeconds = 0.0;
	int chunkTimed = 0, slowTimed = 0;
	long long basicFailures = 0, blockMismatches = 0, fastSlowMismatches = 0;
	int blocksCompared = 0;
	int maxAdjacent = 0;
	long long adjacentOverBound = 0;
	string worstWhere;
	string firstBasicFailure;

	vector<vector<uint8_t>> topIds(blocksPerSide), topWeights(blocksPerSide);
	vector<uint8_t> prevRightIds, prevRightWeights;
	bool prevRightValid = false;

	for (int by = 0; by < blocksPerSide; by++) {
		prevRightValid = false;
		for (int bx = 0; bx < blocksPerSide; bx++) {
			bool buildBlock = thorough || ((bx + by) % 4 == 0);
			if (buildBlock) {
				NearBlendRegion region = map.Region(bx * blockElements, by * blockElements, blockElements);
				blend.BuildRegion(region, blockIds, blockWeights);
				blocksCompared++;

				// 连续性：块内相邻texel
				for (int ty = 0; ty < blockRes; ty++) {
					for (int tx = 0; tx < blockRes; tx++) {
						size_t base = (static_cast<size_t>(ty) * blockRes + tx) * 4;
						int d = 0;
						if (tx + 1 < blockRes) d = max(d, TexelDiff(&blockIds[base], &blockWeights[base], &blockIds[base + 4], &blockWeights[base + 4]));
						if (ty + 1 < blockRes) {
							size_t up = base + static_cast<size_t>(blockRes) * 4;
							d = max(d, TexelDiff(&blockIds[base], &blockWeights[base], &blockIds[up], &blockWeights[up]));
						}
						if (d > maxAdjacent) {
							maxAdjacent = d;
							char where[128];
							snprintf(where, sizeof(where), "texel(%d,%d)", bx * blockRes + tx, by * blockRes + ty);
							worstWhere = where;
						}
						if (d > options.maxAdjacentDiff) adjacentOverBound++;
					}
				}
				// 连续性：块与块之间的接缝(只在相邻块都建过时检查)
				if (thorough) {
					if (bx > 0 && prevRightValid) {
						for (int ty = 0; ty < blockRes; ty++) {
							size_t left = static_cast<size_t>(ty) * 4;
							size_t first = static_cast<size_t>(ty) * blockRes * 4;
							int d = TexelDiff(&prevRightIds[left], &prevRightWeights[left], &blockIds[first], &blockWeights[first]);
							maxAdjacent = max(maxAdjacent, d);
							if (d > options.maxAdjacentDiff) adjacentOverBound++;
						}
					}
					if (by > 0 && !topIds[bx].empty()) {
						for (int tx = 0; tx < blockRes; tx++) {
							size_t below = static_cast<size_t>(tx) * 4;
							size_t first = static_cast<size_t>(tx) * 4;
							int d = TexelDiff(&topIds[bx][below], &topWeights[bx][below], &blockIds[first], &blockWeights[first]);
							maxAdjacent = max(maxAdjacent, d);
							if (d > options.maxAdjacentDiff) adjacentOverBound++;
						}
					}
					prevRightIds.resize(static_cast<size_t>(blockRes) * 4);
					prevRightWeights.resize(static_cast<size_t>(blockRes) * 4);
					for (int ty = 0; ty < blockRes; ty++) {
						size_t src = (static_cast<size_t>(ty) * blockRes + (blockRes - 1)) * 4;
						memcpy(&prevRightIds[static_cast<size_t>(ty) * 4], &blockIds[src], 4);
						memcpy(&prevRightWeights[static_cast<size_t>(ty) * 4], &blockWeights[src], 4);
					}
					prevRightValid = true;
					topIds[bx].assign(blockIds.begin() + static_cast<long>(static_cast<size_t>(blockRes - 1) * blockRes * 4), blockIds.begin() + static_cast<long>(static_cast<size_t>(blockRes) * blockRes * 4));
					topWeights[bx].assign(blockWeights.begin() + static_cast<long>(static_cast<size_t>(blockRes - 1) * blockRes * 4), blockWeights.begin() + static_cast<long>(static_cast<size_t>(blockRes) * blockRes * 4));
				}
			}

			for (int cyl = 0; cyl < BLOCK_CHUNKS; cyl++) {
				for (int cxl = 0; cxl < BLOCK_CHUNKS; cxl++) {
					int cx = bx * BLOCK_CHUNKS + cxl, cy = by * BLOCK_CHUNKS + cyl;
					NearBlendRegion region = map.Region(cx * CHUNK_ELEMENTS, cy * CHUNK_ELEMENTS, CHUNK_ELEMENTS);

					auto t0 = Clock::now();
					blend.BuildRegion(region, chunkIds, chunkWeights, &stats, true);
					double seconds = chrono::duration<double>(Clock::now() - t0).count();
					chunkSeconds += seconds;
					chunkMaxSeconds = max(chunkMaxSeconds, seconds);
					chunkTimed++;

					// 快速路径关闭的对照(抽样，同时计时)
					if ((cx + cy) % 16 == 0) {
						auto s0 = Clock::now();
						blend.BuildRegion(region, slowIds, slowWeights, nullptr, false);
						slowSeconds += chrono::duration<double>(Clock::now() - s0).count();
						slowTimed++;
						if (slowIds != chunkIds || slowWeights != chunkWeights) fastSlowMismatches++;
					}

					// 与大块结果逐字节对比：区域划分不同，结果必须相同
					if (buildBlock) {
						for (int ty = 0; ty < chunkRes; ty++) {
							const uint8_t* a = &chunkIds[static_cast<size_t>(ty) * chunkRes * 4];
							const uint8_t* b = &blockIds[(static_cast<size_t>(cyl * chunkRes + ty) * blockRes + cxl * chunkRes) * 4];
							const uint8_t* wa = &chunkWeights[static_cast<size_t>(ty) * chunkRes * 4];
							const uint8_t* wb = &blockWeights[(static_cast<size_t>(cyl * chunkRes + ty) * blockRes + cxl * chunkRes) * 4];
							if (memcmp(a, b, static_cast<size_t>(chunkRes) * 4) != 0 || memcmp(wa, wb, static_cast<size_t>(chunkRes) * 4) != 0) blockMismatches++;
						}
					}

					// 基本性质 + 累加每个element的平均权重
					for (int ty = 0; ty < chunkRes; ty++) {
						for (int tx = 0; tx < chunkRes; tx++) {
							size_t base = (static_cast<size_t>(ty) * chunkRes + tx) * 4;
							const uint8_t* ids = &chunkIds[base];
							const uint8_t* w = &chunkWeights[base];
							int sum = w[0] + w[1] + w[2] + w[3];
							bool dup = false;
							for (int a = 0; a < 4; a++) for (int b = a + 1; b < 4; b++) if (w[a] > 0 && w[b] > 0 && ids[a] == ids[b]) dup = true;
							if (sum != 255 || dup) {
								if (basicFailures == 0) {
									char where[128];
									snprintf(where, sizeof(where), "chunk(%d,%d) texel(%d,%d) sum=%d dup=%d", cx, cy, tx, ty, sum, dup ? 1 : 0);
									firstBasicFailure = where;
								}
								basicFailures++;
							}
							int ex = cx * CHUNK_ELEMENTS + tx / tpe, ey = cy * CHUNK_ELEMENTS + ty / tpe;
							auto& acc = elementWeights[static_cast<size_t>(ey) * mapSize + ex];
							for (int k = 0; k < 4; k++) if (w[k]) acc[ids[k] % MAX_COLOR_IDS] += w[k];
						}
					}
				}
			}
		}
	}

	// ---- 断言汇总
	Log("基本性质与分块无关:\n");
	checker.Report(mapName + " 每个texel权重和为255且无重复id", basicFailures == 0, "违例 %lld, 首个: %s", basicFailures, firstBasicFailure.c_str());
	checker.Report(mapName + " 逐chunk拼接 == 大块(128 element)一次计算, 逐字节相同", blockMismatches == 0 && blocksCompared > 0,
		"对比了 %d/%d 个块%s, 不一致的chunk行 %lld", blocksCompared, blocksPerSide * blocksPerSide, thorough ? "" : "(抽样)", blockMismatches);
	checker.Report(mapName + " 快速路径开/关结果逐字节相同(抽样chunk)", fastSlowMismatches == 0, "抽样 %d 个chunk, 不一致 %lld", slowTimed, fastSlowMismatches);
	if (enforceBound) {
		checker.Report(mapName + " 相邻texel权重差不超过上界", adjacentOverBound == 0,
			"最大相邻差 %d/255 (%.1f%%) 在 %s, 上界 %d, 超限处数 %lld", maxAdjacent, maxAdjacent / 2.55, worstWhere.c_str(), options.maxAdjacentDiff, adjacentOverBound);
	}
	else {
		Log("  [INFO] %s 最大相邻差 %d/255 (%.1f%%) 在 %s (多种地形混杂的压力地图不设固定上界, 连续性由用例E验证)\n", mapName.c_str(), maxAdjacent, maxAdjacent / 2.55, worstWhere.c_str());
	}

	// 与独立的double实现、慢速参考实现随机对拍(含不与chunk对齐的区域)
	{
		Rng rng(options.seed * 31 + 7);
		NearBlendRegion whole = map.Region(0, 0, mapSize);
		long long compared = 0, refMismatch = 0;
		double worstDouble = 0.0;
		vector<uint8_t> ids, weights;
		for (int trial = 0; trial < 120; trial++) {
			int size = (trial % 3 == 0) ? 7 : (trial % 3 == 1 ? 16 : 33);
			int ox = rng.Range(mapSize - size), oy = rng.Range(mapSize - size);
			NearBlendRegion region = map.Region(ox, oy, size);
			blend.BuildRegion(region, ids, weights);
			int res = size * tpe;
			for (int s = 0; s < 150; s++) {
				int tx = rng.Range(res), ty = rng.Range(res);
				size_t base = (static_cast<size_t>(ty) * res + tx) * 4;
				uint8_t refIds[4], refWeights[4];
				whole.originX = 0;
				blend.ComputeTexelReference(whole, ox * tpe + tx, oy * tpe + ty, refIds, refWeights);
				if (memcmp(refIds, &ids[base], 4) != 0 || memcmp(refWeights, &weights[base], 4) != 0) refMismatch++;
				if (!(map.construction[static_cast<size_t>(oy + ty / tpe) * mapSize + (ox + tx / tpe)])) {
					worstDouble = max(worstDouble, CompareWithBruteForce(map, region, tpe, radius, tx, ty, &ids[base], &weights[base], params.constructionRamp));
				}
				compared++;
			}
		}
		checker.Report(mapName + " 与慢速参考实现逐字节一致(任意起点/大小的区域)", refMismatch == 0, "对比 %lld 个texel, 不一致 %lld", compared, refMismatch);
		checker.Report(mapName + " 与独立double实现一致", worstDouble <= kTolerance, "最大差 %.2f/255", worstDouble);
	}

	// construction纯净性：关闭保险后核本身是否已经保证
	{
		NearBlendParams noGuard = params;
		noGuard.constructionGuard = false;
		NearTerrainBlend unguarded(noGuard);
		NearBlendRegion whole = map.Region(0, 0, mapSize);
		Rng rng(options.seed * 17 + 3);
		long long constructionElements = 0, texels = 0;
		int maxForeign = 0;
		for (int sample = 0; sample < 4000; sample++) {
			int ex = rng.Range(mapSize), ey = rng.Range(mapSize);
			if (!map.construction[static_cast<size_t>(ey) * mapSize + ex]) continue;
			constructionElements++;
			for (int sy = 0; sy < tpe; sy++) {
				for (int sx = 0; sx < tpe; sx++) {
					uint8_t ids[4], weights[4];
					unguarded.ComputeTexelReference(whole, ex * tpe + sx, ey * tpe + sy, ids, weights);
					maxForeign = max(maxForeign, 255 - WeightOfId(ids, weights, map.Id(ex, ey)));
					texels++;
				}
			}
		}
		if (radius <= 1.5 + 1e-6) {
			checker.Report(mapName + " construction内部纯净(关闭保险)", maxForeign == 0, "抽样 %lld 个construction element, 非自身地形最大权重 %d/255", constructionElements, maxForeign);
		}
		else {
			Log("  [INFO] %s radius>1.5: construction内部(关闭保险)非自身地形最大权重 %d/255 = %.1f%%, 抽样 %lld 个element; 保险开启时为0\n",
				mapName.c_str(), maxForeign, maxForeign / 2.55, constructionElements);
		}
	}

	// ---- 统计与耗时
	Log("统计:\n");
	double texelsTotal = static_cast<double>(stats.texelCount);
	Log("  texel总数 %lld: 快速路径 %.1f%%, construction保险 %.1f%%, 需要核计算 %.1f%%, 触发前四大截断 %.4f%%\n",
		static_cast<long long>(stats.texelCount), 100.0 * stats.fastPathTexels / texelsTotal, 100.0 * stats.guardTexels / texelsTotal,
		100.0 * stats.slowPathTexels / texelsTotal, 100.0 * stats.truncatedTexels / texelsTotal);
	Log("  核计算texel里不同id个数分布: 1种 %lld, 2种 %lld, 3种 %lld, 4种 %lld, 5种及以上 %lld\n",
		static_cast<long long>(stats.distinctIdHistogram[1]), static_cast<long long>(stats.distinctIdHistogram[2]),
		static_cast<long long>(stats.distinctIdHistogram[3]), static_cast<long long>(stats.distinctIdHistogram[4]),
		static_cast<long long>(stats.distinctIdHistogram[5]));
	Log("  单个chunk(16x16 element, %dx%d texel)重建耗时: 快速路径开 平均 %.3f ms 最大 %.3f ms; 关 平均 %.3f ms (抽样%d个)\n",
		chunkRes, chunkRes, 1000.0 * chunkSeconds / max(chunkTimed, 1), 1000.0 * chunkMaxSeconds, 1000.0 * slowSeconds / max(slowTimed, 1), slowTimed);
	if (mapName == "stress") {
		checker.Report("stress 压力地图触发了前四大截断分支", stats.truncatedTexels > 0, "截断texel数 %lld", static_cast<long long>(stats.truncatedTexels));
	}

	// ---- 图像
	Log("图像:\n");
	{
		Image after(mapSize, mapSize), diff(mapSize, mapSize);
		for (int ey = 0; ey < mapSize; ey++) {
			for (int ex = 0; ex < mapSize; ex++) {
				const auto& acc = elementWeights[static_cast<size_t>(ey) * mapSize + ex];
				double denominator = 255.0 * tpe * tpe;
				double r = 0, g = 0, b = 0;
				for (int id = 0; id < MAX_COLOR_IDS; id++) {
					double w = acc[id] / denominator;
					r += w * kColors[id][0];
					g += w * kColors[id][1];
					b += w * kColors[id][2];
				}
				uint8_t c[3] = { static_cast<uint8_t>(min(r + 0.5, 255.0)), static_cast<uint8_t>(min(g + 0.5, 255.0)), static_cast<uint8_t>(min(b + 0.5, 255.0)) };
				after.Set(ex, ey, c);
				const uint8_t* base = kColors[map.Id(ex, ey) % MAX_COLOR_IDS];
				double d = (abs(c[0] - base[0]) + abs(c[1] - base[1]) + abs(c[2] - base[2])) / (3.0 * 255.0);
				uint8_t h[3];
				HeatColor(static_cast<float>(d * 4.0), h);
				diff.Set(ex, ey, h);
			}
		}
		Save(options.outDir, string(tag) + "_after.png", after);
		Save(options.outDir, string(tag) + "_diff.png", diff);
	}

	// 放大截图：挑4x4个chunk(64x64 element)里交界最密、地形种类最多的位置
	{
		const int windowChunks = 4;
		const int windowElements = windowChunks * CHUNK_ELEMENTS;
		vector<int> chunkBorder(static_cast<size_t>(chunksPerSide) * chunksPerSide, 0);
		vector<uint32_t> chunkMask(static_cast<size_t>(chunksPerSide) * chunksPerSide, 0);
		for (int y = 0; y < mapSize; y++) {
			for (int x = 0; x < mapSize; x++) {
				uint8_t id = map.Id(x, y);
				bool border = (x > 0 && map.Id(x - 1, y) != id) || (x + 1 < mapSize && map.Id(x + 1, y) != id) ||
					(y > 0 && map.Id(x, y - 1) != id) || (y + 1 < mapSize && map.Id(x, y + 1) != id);
				size_t chunkIndex = static_cast<size_t>(y / CHUNK_ELEMENTS) * chunksPerSide + (x / CHUNK_ELEMENTS);
				if (border) chunkBorder[chunkIndex]++;
				chunkMask[chunkIndex] |= (1u << (id % 32));
			}
		}
		int bestScore = -1, bestX = 0, bestY = 0;
		for (int wy = 0; wy + windowChunks <= chunksPerSide; wy++) {
			for (int wx = 0; wx + windowChunks <= chunksPerSide; wx++) {
				int score = 0;
				uint32_t mask = 0;
				for (int dy = 0; dy < windowChunks; dy++)
					for (int dx = 0; dx < windowChunks; dx++) {
						size_t index = static_cast<size_t>(wy + dy) * chunksPerSide + (wx + dx);
						score += chunkBorder[index];
						mask |= chunkMask[index];
					}
				int distinct = __builtin_popcount(mask);
				if (distinct >= 3) score += 100000;
				if (score > bestScore) { bestScore = score; bestX = wx; bestY = wy; }
			}
		}
		Log("  放大截图位置: chunk(%d,%d) 起 4x4 个chunk, element(%d,%d)~(%d,%d)\n", bestX, bestY,
			bestX * CHUNK_ELEMENTS, bestY * CHUNK_ELEMENTS, bestX * CHUNK_ELEMENTS + windowElements - 1, bestY * CHUNK_ELEMENTS + windowElements - 1);

		NearBlendRegion region = map.Region(bestX * CHUNK_ELEMENTS, bestY * CHUNK_ELEMENTS, windowElements);
		vector<uint8_t> ids, weights;
		blend.BuildRegion(region, ids, weights);
		const int res = windowElements * tpe;
		Image cropBefore(res, res), cropAfter(res, res), cropGrid(res, res), cropWeights(res, res), cropSlots(res, res);
		for (int ty = 0; ty < res; ty++) {
			for (int tx = 0; tx < res; tx++) {
				size_t base = (static_cast<size_t>(ty) * res + tx) * 4;
				const uint8_t* i = &ids[base];
				const uint8_t* w = &weights[base];
				int ex = region.originX + tx / tpe, ey = region.originY + ty / tpe;
				cropBefore.Set(tx, ty, kColors[map.Id(ex, ey) % MAX_COLOR_IDS]);

				uint8_t color[3];
				ColorOf(i, w, color);
				cropAfter.Set(tx, ty, color);

				uint8_t gridColor[3] = { color[0], color[1], color[2] };
				bool onLine = (tx % (CHUNK_ELEMENTS * tpe) == 0) || (ty % (CHUNK_ELEMENTS * tpe) == 0);
				if (onLine) for (int k = 0; k < 3; k++) gridColor[k] = static_cast<uint8_t>((color[k] + 255) / 2);
				cropGrid.Set(tx, ty, gridColor);

				uint8_t perId[3] = {
					static_cast<uint8_t>(WeightOfId(i, w, ID_MOUNTAIN)),
					static_cast<uint8_t>(WeightOfId(i, w, ID_PLAIN)),
					static_cast<uint8_t>(WeightOfId(i, w, ID_OCEAN)) };
				cropWeights.Set(tx, ty, perId);
				uint8_t slots[3] = { w[0], w[1], w[2] };
				cropSlots.Set(tx, ty, slots);
			}
		}
		Save(options.outDir, string(mapName) + "_crop_before.png", cropBefore);
		Save(options.outDir, string(tag) + "_crop_after.png", cropAfter);
		Save(options.outDir, string(tag) + "_crop_after_grid.png", cropGrid);
		Save(options.outDir, string(tag) + "_crop_weights.png", cropWeights);
		Save(options.outDir, string(tag) + "_crop_slots.png", cropSlots);
	}

	RunResult result;
	result.maxAdjacent = maxAdjacent;
	return result;
}

void SaveMapImages(const string& mapName, const TerrainMap& map, const string& outDir) {
	Image before(map.width, map.height), mask(map.width, map.height);
	for (int y = 0; y < map.height; y++) {
		for (int x = 0; x < map.width; x++) {
			before.Set(x, y, kColors[map.Id(x, y) % MAX_COLOR_IDS]);
			uint8_t v = map.construction[static_cast<size_t>(y) * map.width + x] ? 255 : 0;
			uint8_t c[3] = { v, v, v };
			mask.Set(x, y, c);
		}
	}
	Save(outDir, mapName + "_before.png", before);
	Save(outDir, mapName + "_construction_mask.png", mask);
}

}  // namespace

int main(int argc, char** argv) {
	Options options;
	for (int i = 1; i < argc; i++) {
		string arg = argv[i];
		if (arg == "--radius" && i + 1 < argc) { options.radii = { atof(argv[++i]) }; options.radiusGiven = true; }
		else if (arg == "--tpe" && i + 1 < argc) options.tpe = atoi(argv[++i]);
		else if (arg == "--seed" && i + 1 < argc) options.seed = static_cast<uint32_t>(atoi(argv[++i]));
		else if (arg == "--chunks" && i + 1 < argc) options.chunks = atoi(argv[++i]);
		else if (arg == "--bound" && i + 1 < argc) options.maxAdjacentDiff = atoi(argv[++i]);
		else if (arg[0] != '-') options.outDir = arg;
		else { fprintf(stderr, "未知参数 %s\n", arg.c_str()); return 2; }
	}
	if (options.chunks % BLOCK_CHUNKS != 0 || options.chunks < BLOCK_CHUNKS) {
		fprintf(stderr, "--chunks 必须是 %d 的倍数\n", BLOCK_CHUNKS);
		return 2;
	}
	InitCrc();
	filesystem::create_directories(options.outDir);
	gLogFile = fopen((options.outDir + "/results.txt").c_str(), "w");

	Log("近处地形混合算法独立验证: seed=%u chunks=%d texels/element=%d\n", options.seed, options.chunks, options.tpe);
	Checker checker;

	const int mapSize = options.chunks * CHUNK_ELEMENTS;
	TerrainMap mainMap = GenerateMainMap(mapSize, options.seed);
	TerrainMap stressMap = GenerateStressMap(mapSize, options.seed);
	Log("\n输入图像:\n");
	SaveMapImages("main", mainMap, options.outDir);
	SaveMapImages("stress", stressMap, options.outDir);

	long long constructionCount = 0, mountainCount = 0, oceanCount = 0;
	for (size_t i = 0; i < mainMap.ids.size(); i++) {
		constructionCount += mainMap.construction[i];
		mountainCount += mainMap.ids[i] == ID_MOUNTAIN;
		oceanCount += mainMap.ids[i] == ID_OCEAN;
	}
	Log("主地图: construction %.1f%%, mountain %.1f%%, ocean %.1f%%\n", 100.0 * constructionCount / mainMap.ids.size(),
		100.0 * mountainCount / mainMap.ids.size(), 100.0 * oceanCount / mainMap.ids.size());

	for (size_t r = 0; r < options.radii.size(); r++) {
		double radius = options.radii[r];
		Log("\n######## radius = %.2f ########\n", radius);
		RunAnalyticTests(radius, options.tpe, checker);
		RunExperiment("main", mainMap, radius, r == 0, true, options, checker);
		RunExperiment("stress", stressMap, radius, r == 0, false, options, checker);
	}

	Log("\n==== 汇总: %d 项检查, %d 项失败 ====\n", checker.checks, checker.failures);
	if (gLogFile) fclose(gLogFile);
	return checker.failures == 0 ? 0 : 1;
}
