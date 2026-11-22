#pragma once

#include <cmath>

// 水面波浪高度(厘米)，只有垂直位移，是世界xy(厘米)和时间t(秒)的显函数。
// 表达式只有一份：WaterWave.inc(函数体)，这里直接#include进来，材质里的Custom节点由Tools/WaterMaterial脚本
// 把同一份文本贴进去，所以C++(浮力/相机)和shader(水面WPO/后处理)各自实时算出来的结果一致。详见WaterWave.md。
namespace ForeverWater {
	// iterations：octave数(浮点，>=1)。浮力/相机判断一律用kFullIterations，和shader里近处顶点用的值一致。
	constexpr float kFullIterations = 12.f;

	inline float WaveHeight(float x, float y, float t, float iterations = kFullIterations) {
#include "Water/WaterWave.inc"
	}
}
