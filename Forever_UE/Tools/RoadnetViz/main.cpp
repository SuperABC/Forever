// 地形路网可视化测试工具：跑Core的InitTerrains/InitRoadnet，把地形、路网、lot画成PNG，用法见README.md。
#include "map/map.h"
#include "map/roadnet.h"
#include "common/config.h"
#include "common/registry.h"
#include "../ModHost/png_writer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <map>
#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>


using namespace std;

namespace viz {
	struct VizColor { uint8_t r, g, b; };

	class VizCanvas {
	public:
		VizCanvas(int width, int height) : width(width), height(height), pixels(static_cast<size_t>(width) * height * 4, 255) {}

		void Set(int x, int y, VizColor c, float alpha = 1.f) {
			if (x < 0 || y < 0 || x >= width || y >= height) return;
			uint8_t* p = &pixels[(static_cast<size_t>(y) * width + x) * 4];
			p[0] = static_cast<uint8_t>(p[0] + (c.b - p[0]) * alpha);
			p[1] = static_cast<uint8_t>(p[1] + (c.g - p[1]) * alpha);
			p[2] = static_cast<uint8_t>(p[2] + (c.r - p[2]) * alpha);
			p[3] = 255;
		}

		void Disc(float cx, float cy, float radius, VizColor c, float alpha = 1.f) {
			int x0 = static_cast<int>(floorf(cx - radius)), x1 = static_cast<int>(ceilf(cx + radius));
			int y0 = static_cast<int>(floorf(cy - radius)), y1 = static_cast<int>(ceilf(cy + radius));
			for (int y = y0; y <= y1; y++) {
				for (int x = x0; x <= x1; x++) {
					float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
					if (dx * dx + dy * dy <= radius * radius) Set(x, y, c, alpha);
				}
			}
		}

		// 凸四边形填充(顶点按环绕顺序)。
		void FillQuad(const float px[4], const float py[4], VizColor c, float alpha) {
			int x0 = static_cast<int>(floorf(min({ px[0], px[1], px[2], px[3] })));
			int x1 = static_cast<int>(ceilf(max({ px[0], px[1], px[2], px[3] })));
			int y0 = static_cast<int>(floorf(min({ py[0], py[1], py[2], py[3] })));
			int y1 = static_cast<int>(ceilf(max({ py[0], py[1], py[2], py[3] })));
			for (int y = y0; y <= y1; y++) {
				for (int x = x0; x <= x1; x++) {
					float tx = x + 0.5f, ty = y + 0.5f;
					bool neg = false, pos = false;
					for (int i = 0; i < 4; i++) {
						int j = (i + 1) % 4;
						float cr = (px[j] - px[i]) * (ty - py[i]) - (py[j] - py[i]) * (tx - px[i]);
						if (cr < 0.f) neg = true;
						if (cr > 0.f) pos = true;
					}
					if (!(neg && pos)) Set(x, y, c, alpha);
				}
			}
		}

		const vector<uint8_t>& Pixels() const { return pixels; }
		vector<uint8_t>& MutablePixels() { return pixels; }
		int Width() const { return width; }
		int Height() const { return height; }

	private:
		int width, height;
		vector<uint8_t> pixels;  // BGRA
	};

	VizColor TerrainColor(const string& terrain, float height) {
		if (terrain == "terrain_ocean") return { 40, 80, 150 };
		if (terrain == "terrain_river") return { 70, 130, 200 };
		if (terrain == "terrain_mountain") {
			float shade = max(0.f, min(1.f, height / 40.f));
			return { static_cast<uint8_t>(120 + 90 * shade), static_cast<uint8_t>(100 + 90 * shade), static_cast<uint8_t>(80 + 90 * shade) };
		}
		if (terrain == "construction") return { 214, 228, 188 };
		if (terrain == "plain") return { 170, 200, 140 };
		return { 30, 30, 30 };
	}

	// 住宅蓝、商业橙、工业紫、办公红；高密度饱和、低密度浅。
	VizColor AreaColor(AREA_TYPE area) {
		static const VizColor kBase[4] = { { 40, 110, 230 }, { 240, 150, 30 }, { 150, 70, 190 }, { 220, 50, 50 } };
		if (area == AREA_GREEN) return { 60, 170, 70 };
		int a = static_cast<int>(area);
		if (a < AREA_RESIDENTIAL_HIGH || a > AREA_OFFICIAL_LOW) return { 128, 128, 128 };
		int kind = (a - AREA_RESIDENTIAL_HIGH) / 3, level = (a - AREA_RESIDENTIAL_HIGH) % 3;
		float mix = level == 0 ? 0.f : level == 1 ? 0.4f : 0.7f;
		const VizColor& base = kBase[kind];
		return { static_cast<uint8_t>(base.r + (255 - base.r) * mix), static_cast<uint8_t>(base.g + (255 - base.g) * mix),
			static_cast<uint8_t>(base.b + (255 - base.b) * mix) };
	}
}

using namespace viz;

namespace {
	struct LegendItem { VizColor color; const wchar_t* label; };

	// 图例画在地图下方的白色条带里：色块用canvas画，文字用GDI(支持中文)画进临时DIB再叠过来。
	void DrawLegend(VizCanvas& canvas, int top, int stripHeight) {
		const int width = canvas.Width();
		// 图例只写四种lot区域的高/中/低密度。
		const LegendItem items[] = {
			{ AreaColor(AREA_RESIDENTIAL_HIGH), L"住宅 高密度" }, { AreaColor(AREA_RESIDENTIAL_MIDDLE), L"住宅 中密度" }, { AreaColor(AREA_RESIDENTIAL_LOW), L"住宅 低密度" },
			{ AreaColor(AREA_COMMERCIAL_HIGH), L"商业 高密度" }, { AreaColor(AREA_COMMERCIAL_MIDDLE), L"商业 中密度" }, { AreaColor(AREA_COMMERCIAL_LOW), L"商业 低密度" },
			{ AreaColor(AREA_GREEN), L"绿化(原工业高密度)" }, { AreaColor(AREA_INDUSTRIAL_MIDDLE), L"工业 中密度" }, { AreaColor(AREA_INDUSTRIAL_LOW), L"工业 低密度" },
			{ AreaColor(AREA_OFFICIAL_HIGH), L"办公 高密度" }, { AreaColor(AREA_OFFICIAL_MIDDLE), L"办公 中密度" }, { AreaColor(AREA_OFFICIAL_LOW), L"办公 低密度" },
		};
		const int itemCount = static_cast<int>(sizeof(items) / sizeof(items[0]));
		const int perRow = 3, cellWidth = width / perRow, rowHeight = 56, swatch = 32, margin = 20;

		BITMAPINFO info = {};
		info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		info.bmiHeader.biWidth = width;
		info.bmiHeader.biHeight = -stripHeight;
		info.bmiHeader.biPlanes = 1;
		info.bmiHeader.biBitCount = 32;
		info.bmiHeader.biCompression = BI_RGB;
		void* bits = nullptr;
		HDC dc = CreateCompatibleDC(nullptr);
		HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
		if (!bitmap || !bits) { DeleteDC(dc); return; }
		HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
		memset(bits, 255, static_cast<size_t>(width) * stripHeight * 4);
		HFONT font = CreateFontW(30, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
			CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei");
		HGDIOBJ oldFont = SelectObject(dc, font);
		SetBkMode(dc, TRANSPARENT);
		SetTextColor(dc, RGB(20, 20, 20));
		for (int i = 0; i < itemCount; i++) {
			int x = margin + (i % perRow) * cellWidth, y = margin / 2 + (i / perRow) * rowHeight;
			TextOutW(dc, x + swatch + 10, y, items[i].label, static_cast<int>(wcslen(items[i].label)));
		}
		GdiFlush();
		// 文字像素(非纯白)叠到canvas条带上，再画色块。
		const uint8_t* text = static_cast<const uint8_t*>(bits);
		vector<uint8_t>& pixels = canvas.MutablePixels();
		for (int y = 0; y < stripHeight; y++) {
			for (int x = 0; x < width; x++) {
				const uint8_t* p = text + (static_cast<size_t>(y) * width + x) * 4;
				uint8_t* q = &pixels[(static_cast<size_t>(top + y) * width + x) * 4];
				q[0] = p[0]; q[1] = p[1]; q[2] = p[2]; q[3] = 255;
			}
		}
		for (int i = 0; i < itemCount; i++) {
			int x = margin + (i % perRow) * cellWidth, y = top + margin / 2 + (i / perRow) * rowHeight;
			const float px[4] = { static_cast<float>(x), static_cast<float>(x + swatch), static_cast<float>(x + swatch), static_cast<float>(x) };
			const float py[4] = { static_cast<float>(y), static_cast<float>(y), static_cast<float>(y + swatch), static_cast<float>(y + swatch) };
			canvas.FillQuad(px, py, items[i].color, 1.f);
		}

		SelectObject(dc, oldFont);
		DeleteObject(font);
		SelectObject(dc, oldBitmap);
		DeleteObject(bitmap);
		DeleteDC(dc);
	}
}
static int RealMain(int argc, char** argv) {
	string configPath = argc > 1 ? argv[1] : "config.json";
	string outPath = argc > 2 ? argv[2] : "roadnet_viz.png";
	// 相对路径的输出统一放到源码旁的output目录(已被git忽略)，不依赖运行时的工作目录
	if (outPath.find(':') == string::npos && outPath[0] != '\\' && outPath[0] != '/') {
		string dir = __FILE__;
		dir = dir.substr(0, dir.find_last_of("\\/")) + "\\output";
		CreateDirectoryA(dir.c_str(), nullptr);
		outPath = dir + "\\" + outPath;
	}
	int mapSize = argc > 3 ? atoi(argv[3]) : 1024;
	const int scale = argc > 4 ? atoi(argv[4]) : 2;  // 每个element的像素数

	Config::ReadConfig(configPath);
	Registry::Get().RefreshMods();

	Map city(mapSize, mapSize);
	city.InitTerrains();

	// 山体统计：8邻域联通分量的面积和最高/平均高度，小的单独列出来(排查"又小又矮的山")。
	{
		vector<int> label(static_cast<size_t>(mapSize) * mapSize, -1);
		struct Stat { int area = 0; float maxH = 0.f, sumH = 0.f; int x = 0, y = 0; };
		vector<Stat> stats;
		for (int sy = 0; sy < mapSize; sy++) {
			for (int sx = 0; sx < mapSize; sx++) {
				if (label[sy * mapSize + sx] >= 0 || city.GetTerrainRef(sx, sy) != "terrain_mountain") continue;
				Stat st; st.x = sx; st.y = sy;
				int id = static_cast<int>(stats.size());
				vector<pair<int, int>> stack{ { sx, sy } };
				label[sy * mapSize + sx] = id;
				while (!stack.empty()) {
					auto [cx, cy] = stack.back(); stack.pop_back();
					float h = city.GetHeight(cx, cy);
					st.area++; st.sumH += h; st.maxH = max(st.maxH, h);
					for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
						int nx = cx + dx, ny = cy + dy;
						if (nx < 0 || ny < 0 || nx >= mapSize || ny >= mapSize || label[ny * mapSize + nx] >= 0) continue;
						if (city.GetTerrainRef(nx, ny) != "terrain_mountain") continue;
						label[ny * mapSize + nx] = id;
						stack.push_back({ nx, ny });
					}
				}
				stats.push_back(st);
			}
		}
		printf("[RoadnetViz] 山体 %zu 块:", stats.size());
		for (const Stat& st : stats) printf(" (面积%d 最高%.1f 平均%.1f @%d,%d)", st.area, st.maxH, st.sumH / st.area, st.x, st.y);
		printf("\n");
	}
	city.InitRoadnet();

	const int legendHeight = 260;
	VizCanvas canvas(mapSize * scale, mapSize * scale + legendHeight);

	// 地形
	for (int py = 0; py < mapSize * scale; py++) {
		for (int px = 0; px < mapSize * scale; px++) {
			float x = (px + 0.5f) / scale, y = (py + 0.5f) / scale;
			int ex = static_cast<int>(x), ey = static_cast<int>(y);
			const string& terrain = city.GetTerrainRef(ex, ey);
			float h = terrain == "terrain_mountain" ? city.SampleHeight(x, y) : 0.f;
			canvas.Set(px, py, TerrainColor(terrain, h));
		}
	}

	// lot：GetVertex(0..3)=西北/东北/东南/西南。
	map<int, int> areaCount;
	for (const Lot* lot : city.GetLots()) {
		float px[4], py[4];
		for (int i = 0; i < 4; i++) {
			auto v = lot->GetVertex(i);
			px[i] = v.first * scale;
			py[i] = v.second * scale;
		}
		canvas.FillQuad(px, py, AreaColor(lot->GetArea()), 0.75f);
		areaCount[static_cast<int>(lot->GetArea())]++;
	}

	// 道路：沿曲线采样画圆点；隧道(Z<0)紫色，抬升(Z>0)橙色，平地深灰。
	int tunnelRoads = 0, raisedRoads = 0;
	for (const Road* road : city.GetRoads()) {
		float length = road->CalcDistance();
		int steps = max(2, static_cast<int>(length * scale));
		float radius = max(1.f, road->GetTotalWidth() * 0.5f * scale);
		bool tunnel = false, raised = false;
		for (int s = 0; s <= steps; s++) {
			Node p = road->GetPoint(static_cast<float>(s) / steps);
			VizColor c = { 55, 55, 60 };
			if (p.GetZ() < -0.01f) { c = { 190, 60, 220 }; tunnel = true; }
			else if (p.GetZ() > 0.01f) { c = { 255, 140, 0 }; raised = true; }
			canvas.Disc(p.GetX() * scale, p.GetY() * scale, radius, c);
		}
		if (tunnel) tunnelRoads++;
		if (raised) raisedRoads++;
	}

	// 路口度数：按路段两端id统计，度<=1的是断头/孤点，用大的黑点标出来并打印个数。
	map<int, int> degree;
	for (const Road* road : city.GetRoads()) {
		degree[road->GetStart().GetId()]++;
		degree[road->GetEnd().GetId()]++;
	}
	int deadEnds = 0;
	for (const Intersection* node : city.GetIntersections()) {
		bool dead = degree[node->GetId()] <= 1;
		if (dead) deadEnds++;
		canvas.Disc(node->GetX() * scale, node->GetY() * scale, dead ? 7.f : 2.5f, dead ? VizColor{ 0, 0, 0 } : VizColor{ 230, 30, 30 });
	}
	printf("[RoadnetViz] 度<=1的路口(断头): %d\n", deadEnds);

	DrawLegend(canvas, mapSize * scale, legendHeight);

	png::Write(outPath, canvas.Pixels().data(), canvas.Width(), canvas.Height());

	printf("[RoadnetViz] 输出 %s (%dx%d)\n", outPath.c_str(), canvas.Width(), canvas.Height());
	printf("[RoadnetViz] 路口 %zu, 路段 %zu (含隧道的 %d, 含抬升的 %d), lot %zu\n", city.GetIntersections().size(),
		city.GetRoads().size(), tunnelRoads, raisedRoads, city.GetLots().size());
	for (const auto& [area, count] : areaCount) printf("[RoadnetViz]   area %d: %d\n", area, count);
	return 0;
}

int main(int argc, char** argv) {
	try {
		return RealMain(argc, argv);
	}
	catch (const std::exception& e) {
		fprintf(stderr, "[RoadnetViz] exception: %s\n", e.what());
		return 2;
	}
}
