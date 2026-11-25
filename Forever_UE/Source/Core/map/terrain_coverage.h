#pragma once

#include <cstdint>
#include <vector>


// 地形覆盖图烘焙(纯CPU、只依赖标准库)，详见terrain_coverage.md。
// 输入：每个element的地形种类下标(0..typeCount-1)。输出：每种地形一张"覆盖度"图，4种地形打包成一个RGBA8切片。
// 做法：每个element展开成texelsPerElement^2个texel的one-hot掩码 -> 可分离高斯模糊(sigma以element为单位)。
//   模糊把"一格一格"的阶梯边界磨成圆滑曲线(含斜线边界)，各种地形的覆盖度之和始终为1(量化前)。

struct TerrainCoverageParams {
	int texelsPerElement = 2;
	float sigma = 1.0f;       // 高斯sigma，单位element
	float radiusSigmas = 3.0f; // 核半径=radiusSigmas*sigma
};

struct TerrainCoverage {
	int width = 0;            // mip0的texel宽高(=mapWidth*texelsPerElement)
	int height = 0;
	int texelsPerElement = 0;
	int typeCount = 0;
	int sliceCount = 0;       // ceil(typeCount/4)
	int mipCount = 0;
	// data[slice][mip]：RGBA8，通道c对应地形种类 slice*4+c；超出typeCount的通道为0。
	std::vector<std::vector<std::vector<uint8_t>>> data;
};

// ids：mapWidth*mapHeight个种类下标，行优先(x变化最快)。下标>=typeCount按越界处理(抛std::invalid_argument)。
// 出界取最近的格子(边界延伸)。buildMips=false时只生成mip0。
TerrainCoverage BuildTerrainCoverage(const uint8_t* ids, int mapWidth, int mapHeight, int typeCount,
	const TerrainCoverageParams& params, bool buildMips = true);

// 一次2x2盒式降采样RGBA8(宽高各减半，至少为1)。srgb=true时在线性空间平均(给颜色贴图用)，false时直接平均(给覆盖图/法线用)。
void DownsampleRgba8(const uint8_t* src, int width, int height, bool srgb, std::vector<uint8_t>& out, int& outWidth, int& outHeight);

// 完整mip链的层数(到1x1为止)。
int FullMipCount(int width, int height);
