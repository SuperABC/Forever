#include "suites.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <vector>

#include "test_framework.h"

#include "map/terrain_coverage.h"

using namespace std;

// terrain套件：覆盖图烘焙(Core/map/terrain_coverage)。纯函数测试，不需要世界。
//   - 和为1(量化误差内)、没出现的种类全0、均匀地图内部满值；
//   - sigma=0 退化为硬边；对称性；
//   - 45度阶梯边界模糊后沿边界的残余起伏远小于不模糊时；
//   - mip链层数、末级=整图平均；DownsampleRgba8(线性/sRGB)；非法输入抛异常；性能记录。

namespace {
	inline int At(const TerrainCoverage& c, int slice, int mip, int w, int x, int y, int ch) {
		return c.data[slice][mip][(static_cast<size_t>(y) * w + x) * 4 + ch];
	}

	void CheckSumsToOne() {
		TestRun& run = TestRun::Get();
		const int W = 40, H = 30, types = 6;
		vector<uint8_t> ids(W * H);
		srand(7);
		for (auto& id : ids) id = static_cast<uint8_t>(rand() % types);
		TerrainCoverageParams params;
		const TerrainCoverage c = BuildTerrainCoverage(ids.data(), W, H, types, params);
		run.Check(c.width == W * 2 && c.height == H * 2 && c.sliceCount == 2, "dimensions follow texelsPerElement and slice = ceil(types/4)",
			to_string(c.width) + "x" + to_string(c.height) + " slices " + to_string(c.sliceCount));
		int worst = 0;
		bool unusedZero = true;
		for (int y = 0; y < c.height; y++) {
			for (int x = 0; x < c.width; x++) {
				int sum = 0;
				for (int t = 0; t < types; t++) sum += At(c, t / 4, 0, c.width, x, y, t % 4);
				worst = max(worst, abs(sum - 255));
				// 第2个切片只有2个通道有用，后2个通道必须为0
				if (At(c, 1, 0, c.width, x, y, 2) != 0 || At(c, 1, 0, c.width, x, y, 3) != 0) unusedZero = false;
			}
		}
		// 每个通道最多±0.5的量化误差，6个通道最多偏差3
		run.Check(worst <= 3, "coverage of all types sums to 255 within quantization error", "worst deviation " + to_string(worst));
		run.Check(unusedZero, "channels beyond typeCount stay zero");
	}

	void CheckUniformAndAbsent() {
		TestRun& run = TestRun::Get();
		const int W = 16, H = 16;
		vector<uint8_t> ids(W * H, 2);
		const TerrainCoverage c = BuildTerrainCoverage(ids.data(), W, H, 5, TerrainCoverageParams());
		bool ok = true;
		for (int y = 0; y < c.height && ok; y++) {
			for (int x = 0; x < c.width && ok; x++) {
				ok = At(c, 0, 0, c.width, x, y, 2) == 255 && At(c, 0, 0, c.width, x, y, 0) == 0 && At(c, 0, 0, c.width, x, y, 1) == 0
					&& At(c, 0, 0, c.width, x, y, 3) == 0 && At(c, 1, 0, c.width, x, y, 0) == 0;
			}
		}
		run.Check(ok, "uniform map: the one present type is 255 everywhere (borders included), others are 0");
	}

	void CheckHardEdgeAndSymmetry() {
		TestRun& run = TestRun::Get();
		const int W = 20, H = 8;
		vector<uint8_t> ids(W * H);
		for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) ids[y * W + x] = x < W / 2 ? 0 : 1;

		TerrainCoverageParams hard;
		hard.sigma = 0;
		const TerrainCoverage h = BuildTerrainCoverage(ids.data(), W, H, 2, hard);
		bool hardOk = true;
		for (int x = 0; x < h.width; x++) {
			const int expect0 = x < h.width / 2 ? 255 : 0;
			if (At(h, 0, 0, h.width, x, 3, 0) != expect0 || At(h, 0, 0, h.width, x, 3, 1) != 255 - expect0) hardOk = false;
		}
		run.Check(hardOk, "sigma=0 gives hard element edges");

		const TerrainCoverage c = BuildTerrainCoverage(ids.data(), W, H, 2, TerrainCoverageParams());
		bool symmetric = true;
		int centerA = At(c, 0, 0, c.width, c.width / 2 - 1, 3, 0), centerB = At(c, 0, 0, c.width, c.width / 2, 3, 0);
		for (int x = 0; x < c.width; x++) {
			// 水平镜像：类型0在x处的覆盖度 = 类型1在镜像位置的覆盖度(±1量化)
			if (abs(At(c, 0, 0, c.width, x, 3, 0) - At(c, 0, 0, c.width, c.width - 1 - x, 3, 1)) > 1) symmetric = false;
		}
		run.Check(symmetric, "mirrored two-type map has mirrored coverage");
		run.Check(centerA > 128 && centerB < 128 && centerA < 255 && centerB > 0, "the blur is soft across the edge (neither side is a hard step)",
			to_string(centerA) + "/" + to_string(centerB));
		// 单调：越过边界，类型0覆盖度不增
		bool monotonic = true;
		for (int x = 1; x < c.width; x++) if (At(c, 0, 0, c.width, x, 3, 0) > At(c, 0, 0, c.width, x - 1, 3, 0) + 1) monotonic = false;
		run.Check(monotonic, "coverage falls monotonically across a straight edge");
	}

	// 45度阶梯边界：type1 = (x>=y)。沿对角线取覆盖度，模糊后应近似恒定，未模糊时在0/255间跳。
	void CheckDiagonalStaircase() {
		TestRun& run = TestRun::Get();
		const int N = 64;
		vector<uint8_t> ids(N * N);
		for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) ids[y * N + x] = x >= y ? 1 : 0;

		auto ripple = [&](const TerrainCoverage& c, int offset) {
			// 沿 texel (i+offset, i) 取type1覆盖度，只看中段避开地图边界；offset=0是几何对角线
			int lo = 255, hi = 0;
			for (int i = c.width / 4; i < c.width * 3 / 4; i++) {
				const int v = At(c, 0, 0, c.width, i + offset, i, 1);
				lo = min(lo, v);
				hi = max(hi, v);
			}
			return hi - lo;
		};
		TerrainCoverageParams hard;
		hard.sigma = 0;
		const TerrainCoverage h = BuildTerrainCoverage(ids.data(), N, N, 2, hard);
		const TerrainCoverage c = BuildTerrainCoverage(ids.data(), N, N, 2, TerrainCoverageParams());
		// 阶梯的平均边界在 x-y=-0.5 texel，取偏移0和-1两条相邻线(各偏半个texel)，较大者为起伏
		const int hardRipple = max(ripple(h, 0), ripple(h, -1)), softRipple = max(ripple(c, 0), ripple(c, -1));
		TestRun::Get().Metric("diagonal ripple unblurred", hardRipple, "of 255");
		TestRun::Get().Metric("diagonal ripple blurred", softRipple, "of 255");
		run.Check(hardRipple > 100, "without blur the staircase edge jumps along the diagonal", to_string(hardRipple));
		run.Check(softRipple <= 8, "with blur the coverage along a 45 degree edge is nearly constant", to_string(softRipple));
		// 等值线(0.5)偏离几何对角线应小于半格(1 texel)：沿对角线偏移±1texel处必须分居两侧
		const int mid = c.width / 2;
		const int inside = At(c, 0, 0, c.width, mid + 3, mid, 1), outside = At(c, 0, 0, c.width, mid - 3, mid, 1);
		run.Check(inside > 128 && outside < 128, "the 50% iso-line stays on the true diagonal", to_string(inside) + "/" + to_string(outside));
	}

	void CheckMips() {
		TestRun& run = TestRun::Get();
		const int W = 32, H = 32;
		vector<uint8_t> ids(W * H);
		for (int i = 0; i < W * H; i++) ids[i] = (i % W) < 8 ? 0 : 1;   // 25% 类型0
		const TerrainCoverage c = BuildTerrainCoverage(ids.data(), W, H, 2, TerrainCoverageParams());
		run.Check(c.mipCount == FullMipCount(64, 64) && c.mipCount == 7, "mip chain goes down to 1x1", to_string(c.mipCount));
		run.Check(c.data[0].size() == static_cast<size_t>(c.mipCount), "every slice stores every mip");
		const int last = At(c, 0, c.mipCount - 1, 1, 0, 0, 0);
		run.Check(abs(last - 64) <= 4, "the 1x1 mip holds the whole-map average (25% -> ~64)", to_string(last));
		const TerrainCoverage nomip = BuildTerrainCoverage(ids.data(), W, H, 2, TerrainCoverageParams(), false);
		run.Check(nomip.mipCount == 1 && nomip.data[0].size() == 1, "buildMips=false keeps only mip0");
		run.Check(FullMipCount(1, 1) == 1 && FullMipCount(1024, 512) == 11 && FullMipCount(5, 3) == 3, "FullMipCount for odd/non-square sizes");
	}

	void CheckDownsample() {
		TestRun& run = TestRun::Get();
		// 2x2：三黑一白(255)，线性平均 = 63.75
		const uint8_t src[16] = { 0, 0, 0, 255, 255, 255, 255, 255, 0, 0, 0, 255, 0, 0, 0, 255 };
		vector<uint8_t> out;
		int w = 0, h = 0;
		DownsampleRgba8(src, 2, 2, false, out, w, h);
		run.Check(w == 1 && h == 1 && abs(out[0] - 64) <= 1 && out[3] == 255, "linear downsample averages the four texels", to_string(out[0]));
		DownsampleRgba8(src, 2, 2, true, out, w, h);
		// sRGB空间：白的线性占1/4 -> 线性0.25 -> sRGB约137
		run.Check(abs(out[0] - 137) <= 2 && out[3] == 255, "sRGB downsample averages in linear space", to_string(out[0]));
		// 1xN 不能降到0
		const uint8_t strip[12] = { 10, 10, 10, 255, 30, 30, 30, 255, 50, 50, 50, 255 };
		DownsampleRgba8(strip, 3, 1, false, out, w, h);
		run.Check(w == 1 && h == 1 && out.size() == 4, "odd/degenerate sizes clamp to 1");
	}

	void CheckInvalid() {
		TestRun& run = TestRun::Get();
		vector<uint8_t> ids(4, 3);
		bool threw = false;
		try { BuildTerrainCoverage(ids.data(), 2, 2, 3, TerrainCoverageParams()); }
		catch (const invalid_argument&) { threw = true; }
		run.Check(threw, "a type id >= typeCount is rejected");
		threw = false;
		try { BuildTerrainCoverage(nullptr, 2, 2, 3, TerrainCoverageParams()); }
		catch (const invalid_argument&) { threw = true; }
		run.Check(threw, "null input is rejected");
	}

	void CheckPerformance(bool quick) {
		TestRun& run = TestRun::Get();
		const int N = quick ? 256 : 1024;
		vector<uint8_t> ids(static_cast<size_t>(N) * N);
		srand(3);
		for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) ids[static_cast<size_t>(y) * N + x] = static_cast<uint8_t>(((x / 37) + (y / 53) + rand() % 2) % 5);
		const auto t0 = chrono::steady_clock::now();
		const TerrainCoverage c = BuildTerrainCoverage(ids.data(), N, N, 5, TerrainCoverageParams());
		const double ms = chrono::duration<double, milli>(chrono::steady_clock::now() - t0).count();
		run.Metric("coverage bake " + to_string(N) + "x" + to_string(N) + " 5 types", ms, "ms");
		run.Check(c.width == N * 2, "perf case produced a coverage map");
		run.Check(ms < 30000, "coverage bake finishes in a sane time", to_string(ms) + " ms");
	}
}

void RunTerrainSuite(const TestOptions& options) {
	TestRun::Get().Begin("terrain", 0, options.selfCommand + " --suite terrain");
	CheckSumsToOne();
	CheckUniformAndAbsent();
	CheckHardEdgeAndSymmetry();
	CheckDiagonalStaircase();
	CheckMips();
	CheckDownsample();
	CheckInvalid();
	CheckPerformance(options.quick);
}
