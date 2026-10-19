#pragma once

#include "class.h"

#include <string>
#include <vector>


class ScriptFactory;

// 一台电梯(轿厢)——每条ElevatorCabinSpec(BuildingMod::AssignElevatorCabin)对应一台。持有自己的Script
// (spec里指定scriptModName/milestoneNames时才有)、交互选项、当前所在楼层和行程状态。UE层只负责按这里
// 的状态摆放轿厢、推进行程(ABuildingElement::Tick)，状态本身在Core，LOD重建/远景/存档都不丢。
// 楼层用人类直觉层号：0=1楼，负数=地下。高度都是地图单位(不含UE的BUILDING_HEIGHT_EPSILON)。
// 详见cabin.md。
class Cabin {
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;

public:
	// @interactName: 全局唯一的交互名("<building名> 电梯<n>")，脚本里的$$self.name、交互选项、
	//                launch_elevator都按它找这台电梯
	// @specIndex: 在building->GetCabins()里对应第几条spec
	// @initialZ: 初始楼层(currentLevel)的楼层基准高度
	Cabin(Building* building, int specIndex, const std::string& interactName, int shaftIndex,
		int minLevel, int maxLevel, int initialLevel, float initialZ,
		const std::string& scriptModName, const std::vector<std::string>& milestoneNames, ScriptFactory* scriptFactory);
	~Cabin(); // delete script

	Building* GetBuilding() const { return building; }
	int GetSpecIndex() const { return specIndex; }
	const std::string& GetInteractName() const { return interactName; }
	int GetShaftIndex() const { return shaftIndex; }
	int GetMinLevel() const { return minLevel; }
	int GetMaxLevel() const { return maxLevel; }

	// 停着时所在的楼层；运行中是出发前所在的楼层(到站时才更新成targetLevel)。
	int GetCurrentLevel() const { return currentLevel; }
	bool IsMoving() const { return moving; }
	int GetTargetLevel() const { return targetLevel; }
	float GetFromZ() const { return fromZ; }
	float GetElapsed() const { return elapsed; }
	// 轿厢当前高度——停着时是currentLevel的楼层基准高度，运行中由UE层每帧写回(SetCurrentZ)。
	float GetCurrentZ() const { return currentZ; }

	// 让电梯前往level(夹到[min,max])。从当前高度重新出发(运行中再次Launch=改目标)；已经停在这一层
	// 时什么都不做。
	void Launch(int level);

	// UE层推进行程用：累加本次行程时间、写回当前高度、到站。
	void Advance(float deltaSeconds) { elapsed += deltaSeconds; }
	void SetCurrentZ(float z) { currentZ = z; }
	void Arrive(float targetZ);

	Script* GetScript() const { return script; }
	const std::vector<std::string>& GetOptions() const { return options; }
	void AddOption(const std::string& option);
	void RemoveOption(const std::string& option);

private:
	Building* building = nullptr;
	int specIndex = 0;
	std::string interactName;
	int shaftIndex = 0;
	int minLevel = 0;
	int maxLevel = 0;

	int currentLevel = 0;
	float currentZ = 0.f;
	bool moving = false;
	float fromZ = 0.f;
	int targetLevel = 0;
	float elapsed = 0.f;

	Script* script = nullptr;
	std::vector<std::string> options;
};
