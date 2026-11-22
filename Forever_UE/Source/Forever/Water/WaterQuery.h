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
}
