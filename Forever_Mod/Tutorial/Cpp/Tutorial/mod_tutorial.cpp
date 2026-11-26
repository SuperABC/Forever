#include "map/terrain_factory.h"
#include "map/roadnet_factory.h"
#include "common/mod_concepts.h"
#include "terrain_small.h"
#include "roadnet_small.h"

#pragma comment(lib, "Dependence.lib")

// 把宿主的随机host和debugf输出函数绑进本DLL自己那份Dependence，见Dependence/common/mod_concepts.md
FOREVER_MOD_BIND_HOST

// 新手教学专用的mod。terrain_small/roadnet_small由Tools/MapEmbed从Forever_UE/Resource/Mod/Basic/small.terrain/.roadnet生成，勿手改。
extern "C" __declspec(dllexport) void* GetModDependencies() {
	static std::vector<std::pair<std::string, std::vector<std::string>>> dependencies = {
		{ SmallTerrain::GetId(), {} },
		{ SmallRoadnet::GetId(), {} },
	};
	return (void*)&dependencies;
}

extern "C" __declspec(dllexport) void* GetModTerrains() {
	static std::vector<std::string> mods = { SmallTerrain::GetId() };
	return (void*)&mods;
}

extern "C" __declspec(dllexport) void RegisterModTerrains(TerrainFactory* factory) {
	factory->RegisterTerrain(SmallTerrain::GetId(),
		[](const std::string& args) -> TerrainMod* { return new SmallTerrain(args); },
		[](TerrainMod* m) { delete m; });
}

extern "C" __declspec(dllexport) void FinishModTerrains(TerrainFactory* factory) {
}

extern "C" __declspec(dllexport) void* GetModRoadnets() {
	static std::vector<std::string> mods = { SmallRoadnet::GetId() };
	return (void*)&mods;
}

extern "C" __declspec(dllexport) void RegisterModRoadnets(RoadnetFactory* factory) {
	factory->RegisterRoadnet(SmallRoadnet::GetId(),
		[](const std::string& args) -> RoadnetMod* { return new SmallRoadnet(args); },
		[](RoadnetMod* m) { delete m; });
}

extern "C" __declspec(dllexport) void FinishModRoadnets(RoadnetFactory* factory) {
}
