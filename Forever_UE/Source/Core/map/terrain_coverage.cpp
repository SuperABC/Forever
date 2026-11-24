#include "map/terrain_coverage.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

using namespace std;

namespace {
	// 归一化的一维高斯核，半径taps、总共2*taps+1个。
	vector<float> MakeKernel(float sigmaTexels, int taps) {
		vector<float> kernel(2 * taps + 1);
		float sum = 0;
		for (int i = -taps; i <= taps; i++) {
			const float v = expf(-0.5f * (i * i) / (sigmaTexels * sigmaTexels));
			kernel[i + taps] = v;
			sum += v;
		}
		for (float& v : kernel) v /= sum;
		return kernel;
	}

	float SrgbToLinear(float c) {
		return c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
	}

	float LinearToSrgb(float c) {
		return c <= 0.0031308f ? c * 12.92f : 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
	}
}

int FullMipCount(int width, int height) {
	int count = 1;
	int size = max(width, height);
	while (size > 1) {
		size >>= 1;
		count++;
	}
	return count;
}

void DownsampleRgba8(const uint8_t* src, int width, int height, bool srgb, vector<uint8_t>& out, int& outWidth, int& outHeight) {
	outWidth = max(1, width / 2);
	outHeight = max(1, height / 2);
	out.assign(static_cast<size_t>(outWidth) * outHeight * 4, 0);

	// sRGB通道查表(256项)，避免每个texel做powf。alpha始终线性平均。
	float toLinear[256];
	for (int i = 0; i < 256; i++) toLinear[i] = srgb ? SrgbToLinear(i / 255.0f) : i / 255.0f;

	for (int y = 0; y < outHeight; y++) {
		for (int x = 0; x < outWidth; x++) {
			// 奇数尺寸时最后一列/行只取存在的那一个，保持平均不偏。
			const int x0 = min(x * 2, width - 1), x1 = min(x * 2 + 1, width - 1);
			const int y0 = min(y * 2, height - 1), y1 = min(y * 2 + 1, height - 1);
			const uint8_t* p[4] = {
				src + (static_cast<size_t>(y0) * width + x0) * 4, src + (static_cast<size_t>(y0) * width + x1) * 4,
				src + (static_cast<size_t>(y1) * width + x0) * 4, src + (static_cast<size_t>(y1) * width + x1) * 4 };
			uint8_t* dst = &out[(static_cast<size_t>(y) * outWidth + x) * 4];
			for (int c = 0; c < 4; c++) {
				float v;
				if (c < 3) v = (toLinear[p[0][c]] + toLinear[p[1][c]] + toLinear[p[2][c]] + toLinear[p[3][c]]) * 0.25f;
				else v = (p[0][c] + p[1][c] + p[2][c] + p[3][c]) * (0.25f / 255.0f);
				if (c < 3 && srgb) v = LinearToSrgb(v);
				dst[c] = static_cast<uint8_t>(clamp(v * 255.0f + 0.5f, 0.0f, 255.0f));
			}
		}
	}
}

TerrainCoverage BuildTerrainCoverage(const uint8_t* ids, int mapWidth, int mapHeight, int typeCount,
	const TerrainCoverageParams& params, bool buildMips) {
	if (!ids || mapWidth <= 0 || mapHeight <= 0 || typeCount <= 0) throw invalid_argument("BuildTerrainCoverage: empty input");
	if (params.texelsPerElement <= 0 || params.sigma < 0) throw invalid_argument("BuildTerrainCoverage: bad params");
	for (int i = 0; i < mapWidth * mapHeight; i++) {
		if (ids[i] >= typeCount) throw invalid_argument("BuildTerrainCoverage: type id out of range");
	}

	TerrainCoverage result;
	const int tpe = params.texelsPerElement;
	result.width = mapWidth * tpe;
	result.height = mapHeight * tpe;
	result.texelsPerElement = tpe;
	result.typeCount = typeCount;
	result.sliceCount = (typeCount + 3) / 4;
	const int W = result.width, H = result.height;
	result.mipCount = buildMips ? FullMipCount(W, H) : 1;
	result.data.assign(result.sliceCount, vector<vector<uint8_t>>(1));
	for (auto& slice : result.data) slice[0].assign(static_cast<size_t>(W) * H * 4, 0);

	// 哪些种类真的出现过(没出现的通道保持0，省掉整遍模糊)
	vector<char> present(typeCount, 0);
	for (int i = 0; i < mapWidth * mapHeight; i++) present[ids[i]] = 1;

	const float sigmaTexels = params.sigma * tpe;
	const int taps = sigmaTexels > 0 ? max(1, static_cast<int>(ceilf(params.radiusSigmas * sigmaTexels))) : 0;
	const vector<float> kernel = taps > 0 ? MakeKernel(sigmaTexels, taps) : vector<float>{ 1.0f };

	vector<float> mask(static_cast<size_t>(W) * H), temp(static_cast<size_t>(W) * H);
	for (int type = 0; type < typeCount; type++) {
		if (!present[type]) continue;
		// one-hot：texel(tx,ty)属于element(tx/tpe, ty/tpe)
		for (int ty = 0; ty < H; ty++) {
			const uint8_t* row = ids + static_cast<size_t>(ty / tpe) * mapWidth;
			float* dst = &mask[static_cast<size_t>(ty) * W];
			for (int tx = 0; tx < W; tx++) dst[tx] = row[tx / tpe] == type ? 1.0f : 0.0f;
		}
		// 水平模糊(边界取最近texel)
		for (int ty = 0; ty < H; ty++) {
			const float* src = &mask[static_cast<size_t>(ty) * W];
			float* dst = &temp[static_cast<size_t>(ty) * W];
			for (int tx = 0; tx < W; tx++) {
				float sum = 0;
				for (int k = -taps; k <= taps; k++) sum += kernel[k + taps] * src[clamp(tx + k, 0, W - 1)];
				dst[tx] = sum;
			}
		}
		// 垂直模糊并量化进切片通道
		const int slice = type / 4, channel = type % 4;
		vector<uint8_t>& out = result.data[slice][0];
		vector<float> acc(W);
		for (int ty = 0; ty < H; ty++) {
			fill(acc.begin(), acc.end(), 0.0f);
			for (int k = -taps; k <= taps; k++) {
				const float w = kernel[k + taps];
				const float* row = &temp[static_cast<size_t>(clamp(ty + k, 0, H - 1)) * W];
				for (int tx = 0; tx < W; tx++) acc[tx] += w * row[tx];
			}
			for (int tx = 0; tx < W; tx++) {
				out[(static_cast<size_t>(ty) * W + tx) * 4 + channel] = static_cast<uint8_t>(clamp(acc[tx] * 255.0f + 0.5f, 0.0f, 255.0f));
			}
		}
	}

	if (buildMips) {
		for (auto& slice : result.data) {
			int w = W, h = H;
			for (int mip = 1; mip < result.mipCount; mip++) {
				vector<uint8_t> next;
				int nw, nh;
				DownsampleRgba8(slice[mip - 1].data(), w, h, false, next, nw, nh);
				slice.push_back(move(next));
				w = nw;
				h = nh;
			}
		}
	}
	return result;
}
