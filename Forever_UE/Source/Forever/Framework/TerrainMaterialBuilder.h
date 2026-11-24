#pragma once

#include "CoreMinimal.h"

#include <vector>

class Map;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// 地形材质的数据侧：按各地形mod声明的材质(Map::GetTerrainAppearances)，创建M_Terrain需要的全部纹理(层贴图数组=拷贝cook后的压缩块、覆盖图数组、
// 地形类型表/层表、噪声)并实例化成MID。详见ForeverTerrainFrameworkComponent.md"地形材质"一节。
// 没有任何兜底：材质没声明/贴图格式不符或读不到/地图里出现没有声明的地形，都只打UE_LOG(Error)，对应地形渲染成纯灰(一眼可见的配置错误)。
class FTerrainMaterialBuilder {
public:
	// 水深在顶点色R里的编码范围(米)：R = (depthMeters - Min) / (Max - Min)，depth=水面高度-地面高度，正=在水下。
	// 材质通过TerrainWaterEncoding参数读同样的范围，C++是唯一来源。
	static constexpr float WaterDepthMinMeters = -4.f;
	static constexpr float WaterDepthMaxMeters = 20.f;
	// 一个element的米数(地图高度单位也是element)
	static constexpr float MetersPerElement = 10.f;
	// 没有水的标记
	static constexpr float NoWater = -1.0e30f;
	// 一次材质最多支持的不同地形种类数(覆盖图2个切片，每片4种)
	static constexpr int32 MaxTerrainTypes = 8;

	// 成功返回已设好全部参数的MID(outer持有)；失败(无基础材质/配置文件缺失或解析失败)返回nullptr，已打Error。
	static UMaterialInstanceDynamic* Build(UObject* outer, Map* map, UMaterialInterface* baseMaterial);

	// 每个element的"最近水面高度"(element单位，行优先)。有水的element取自己的水位，其余向外按4邻域扩散radius格取最近水位，
	// 更远的为NoWater。用于顶点水深(河/海/沙滩按水线分层)。
	static void BuildWaterLevelField(Map* map, int32 radius, std::vector<float>& outField);

	static uint8 EncodeWaterDepth(float depthMeters);
};
