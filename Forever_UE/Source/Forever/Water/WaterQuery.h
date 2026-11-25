#pragma once

#include "CoreMinimal.h"

class Map;

namespace ForeverWater {
	// 世界坐标处的水面高度查询：玩家游泳(AForeverCharacter::QueryWater)和船的浮力(ABoatElement)共用这一份。
	// 格子有水返回true：outSurfaceZ=水位+波浪(cm)，outBedZ=水底/地面高度(cm)，outBaseSurfaceZ(可空)=不含波浪的水位。
	// 没水/越界/没有地图返回false(输出不动)。
	// 注意：沙滩格子也算有水(水位略高于地面)，此时outBedZ可能高于outSurfaceZ。
	bool QuerySurface(Map* map, const FVector& worldLocation, double timeSeconds,
		float& outSurfaceZ, float& outBedZ, float* outBaseSurfaceZ = nullptr);

	// 世界坐标是否在地形表面(Map::SampleHeight，含1/16细化高度)以下——海底隧道里的玩家/相机就是这种情况：
	// 格子虽然有水，但人在地面之下的隧道里，不在水中。玩家游泳和水下后处理用它把"在地下"的位置排除出水体；
	// 船的浮力不用(船的探测点贴着岸边地形时可能略低于地面，仍要有浮力)。越界/没有地图返回false。
	bool IsBelowTerrain(Map* map, const FVector& worldLocation);
}
