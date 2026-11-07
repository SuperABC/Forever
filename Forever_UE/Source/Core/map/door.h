#pragma once

#include "class.h"

#include "map/feature_spec.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


// 门属于哪一类所属对象——决定GetZone()/GetBuilding()/GetRoom()哪一个非空。
enum DOOR_KIND_TYPE : int { DOOR_KIND_ZONE, DOOR_KIND_BUILDING, DOOR_KIND_ROOM };

// 门禁状态——数值和DoorSpec::access的int字面值(0/1/2)保持一致，ResolveDoorSpec/
// Map::CreateDoor按这个顺序转换。
enum DOOR_ACCESS_TYPE : int { DOOR_ACCESS_OPEN, DOOR_ACCESS_OWNER, DOOR_ACCESS_LOCKED };

// 一扇真正放了门扇的门的运行期实体(mesh为空的纯门洞不会创建)，外观/开关方式来自创建时的DoorSpec快照，设计见door.md
class Door {
	// 存档：MapSaveCodec直接读写私有字段(map_save.md)。
	friend class MapSaveCodec;
public:
	Door() = delete;

	// id/interactName/flippedSide由Map::CreateDoor算好传入；interactName非空才创建Script；
	// zone/building/room按kind只有一个非空，Door不持有它们的生命周期
	Door(const std::string& id, DOOR_KIND_TYPE kind, const std::string& tag, const DoorSpec& spec,
		Zone* zone, Building* building, Room* room, const std::string& interactName, bool flippedSide,
		float x, float y, float z, float width, float height, float yaw, int level, bool isVehicleGate,
		ScriptFactory* scriptFactory);
	~Door(); // delete script

	const std::string& GetId() const;
	const std::string& GetName() const;          // spec.name；空=纯自动门，没有交互名/Script
	const std::string& GetInteractName() const;  // name非空时非空，否则恒为空字符串
	const std::string& GetTag() const;            // layout标签；园区门恒为空
	DOOR_KIND_TYPE GetKind() const;
	Zone* GetZone() const;
	Building* GetBuilding() const;
	Room* GetRoom() const;

	const std::string& GetMesh() const;           // 一定非空(为空的根本不会创建Door)
	const std::string& GetFrameMesh() const;
	int GetStyle() const;                         // 0=Slide 1=Swing，见feature_spec.h
	int GetLeaves() const;                        // 1或2
	float GetOpenAmount() const;                  // 0表示"用默认值"，由Forever层解释具体数值
	float GetOpenSeconds() const;

	// 几何(世界坐标、地图单位；Map::CreateDoor创建时算好，之后不会再变)。
	float GetX() const;
	float GetY() const;
	float GetZ() const;                           // 门洞底边中点
	float GetWidth() const;
	float GetHeight() const;
	float GetYaw() const;                         // 墙体法线方向(弧度，世界坐标)
	int GetLevel() const;
	bool IsVehicleGate() const;

	// 单扇门(leaves==1)且spec.randomSide时创建时掷的硬币结果，其余恒为false
	bool IsFlippedSide() const;

	// 门禁判定顺序见door.md"CanPass判定顺序"
	DOOR_ACCESS_TYPE GetAccess() const;
	void SetAccess(DOOR_ACCESS_TYPE access);
	// 按名字登记例外(Map只有名字没有Citizen*)，再次登记覆盖
	void Register(const std::string& citizenName, bool pass);
	bool CanPass(const Citizen* who, bool isPlayer) const;

	Script* GetScript() const;
	const std::vector<std::string>& GetOptions() const;
	void AddOption(const std::string& option);
	void RemoveOption(const std::string& option);

private:
	std::string id;
	DOOR_KIND_TYPE kind;
	std::string tag;
	DoorSpec spec;

	Zone* zone = nullptr;
	Building* building = nullptr;
	Room* room = nullptr;

	std::string interactName;

	float x = 0.f, y = 0.f, z = 0.f;
	float width = 0.f, height = 0.f, yaw = 0.f;
	int level = 0;
	bool vehicleGate = false;

	bool flippedSide = false;

	DOOR_ACCESS_TYPE access = DOOR_ACCESS_OPEN;
	std::unordered_map<std::string, bool> registrations; // 市民名 -> 是否允许通过，见Register

	Script* script = nullptr;
	std::vector<std::string> options;
};

// 建筑门/房间门的放置判定：tag为空/查不到/mesh为空返回nullptr(只是门洞)。园区门不走这个函数，见zone.cpp
const DoorSpec* ResolveDoorSpec(const std::unordered_map<std::string, DoorSpec>& doorSpecs,
	const std::string& tag);
