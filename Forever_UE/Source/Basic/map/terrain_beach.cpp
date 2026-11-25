#include "terrain_beach.h"

using namespace std;

int BeachTerrain::count = 0;

BeachTerrain::BeachTerrain(const string& args) : id(count++) {
	(void)args;
}

BeachTerrain::~BeachTerrain() {

}

const char* BeachTerrain::GetId() {
	return "terrain_beach";
}

const char* BeachTerrain::GetType() const {
	return "terrain_beach";
}

const char* BeachTerrain::GetName() {
	name = "沙滩地形" + to_string(id);
	return name.data();
}

float BeachTerrain::GetPriority() const {
	// 不生成任何东西，优先级无所谓；放在最低，万一以后加了自动生成也在山/海/河之后。
	return 0.1f;
}

void BeachTerrain::SetupTexture() {
	SetMapColor(222, 204, 150);
	// 高出水面0.4米以上是干沙，水线(深度0)处已是湿沙。
	// 贴图是工程里的Megascans Surface资产(颜色/法线/ORM三张)，字段顺序：name, diffuse, normal, orm, roughness, tileMeters, triplanar。
	const TerrainMaterialLayer drySand{ "dry_beach_sand",
		"/Game/3rdParty/Megascans/Surfaces/Trampled_Beach_Sand_vmghdejo/Medium/vmghdejo_tier_2/Textures/T_vmghdejo_2K_B.T_vmghdejo_2K_B",
		"/Game/3rdParty/Megascans/Surfaces/Trampled_Beach_Sand_vmghdejo/Medium/vmghdejo_tier_2/Textures/T_vmghdejo_2K_N.T_vmghdejo_2K_N",
		"/Game/3rdParty/Megascans/Surfaces/Trampled_Beach_Sand_vmghdejo/Medium/vmghdejo_tier_2/Textures/T_vmghdejo_2K_ORM.T_vmghdejo_2K_ORM",
		"", 3.f, false };
	const TerrainMaterialLayer wetSand{ "wet_sand",
		"/Game/3rdParty/Megascans/Surfaces/Wet_Sand_rldapzp0/Medium/rldapzp0_tier_2/Textures/T_rldapzp0_2K_B.T_rldapzp0_2K_B",
		"/Game/3rdParty/Megascans/Surfaces/Wet_Sand_rldapzp0/Medium/rldapzp0_tier_2/Textures/T_rldapzp0_2K_N.T_rldapzp0_2K_N",
		"/Game/3rdParty/Megascans/Surfaces/Wet_Sand_rldapzp0/Medium/rldapzp0_tier_2/Textures/T_rldapzp0_2K_ORM.T_rldapzp0_2K_ORM",
		"", 3.f, false };
	SetMaterial(drySand, wetSand, TerrainMaterialSelector::WaterDepth, -0.4f, 0.0f);
}

void BeachTerrain::DistributeTerrain(int width, int height,
	const function<const string&(int, int)>& getTerrain, const function<bool(int, int, const string&)>& setTerrain,
	const function<float(int, int)>& getHeight, const function<bool(int, int, float)>& setHeight,
	const function<bool(int, int, bool, float)>& setWater,
	const function<bool(int, int, const float*)>& setFineHeights) const {
	// 沙滩由地图编辑器手绘，这里不自动生成。
	(void)width; (void)height; (void)getTerrain; (void)setTerrain; (void)getHeight; (void)setHeight; (void)setWater; (void)setFineHeights;
}
