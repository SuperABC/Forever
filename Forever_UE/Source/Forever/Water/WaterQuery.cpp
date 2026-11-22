#include "Water/WaterQuery.h"

#include "Water/WaterWave.h"

#include "map/map.h"

bool ForeverWater::QuerySurface(Map* map, const FVector& worldLocation, double timeSeconds,
	float& outSurfaceZ, float& outBedZ, float* outBaseSurfaceZ)
{
	// 地图1单位=10米=1000厘米，和地形/水面mesh生成时的worldScale一致(见ForeverTerrainFrameworkComponent)。
	constexpr float kMapUnitCm = 1000.f;
	if (!map) return false;

	// 先判范围：Map::GetWater越界会打警告，这里每帧调用，不能刷屏。
	auto [width, height] = map->GetSize();
	int cellX = FMath::FloorToInt(worldLocation.X / kMapUnitCm);
	int cellY = FMath::FloorToInt(worldLocation.Y / kMapUnitCm);
	if (cellX < 0 || cellY < 0 || cellX >= width || cellY >= height) return false;

	auto [hasWater, level] = map->GetWater(cellX, cellY);
	if (!hasWater) return false;

	const float baseSurfaceZ = level * kMapUnitCm;
	if (outBaseSurfaceZ) *outBaseSurfaceZ = baseSurfaceZ;
	// 波浪：和水面材质的WPO同一个表达式(ForeverWater::WaveHeight)，时间用世界时间(材质读的MPC WaterTime也是它)。
	outSurfaceZ = baseSurfaceZ + ForeverWater::WaveHeight(worldLocation.X, worldLocation.Y, static_cast<float>(timeSeconds));
	outBedZ = map->SampleHeight(worldLocation.X / kMapUnitCm, worldLocation.Y / kMapUnitCm) * kMapUnitCm;
	return true;
}
