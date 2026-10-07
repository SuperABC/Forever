#pragma once

#include "class.h"

#include "map/door_spec.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


// 门属于哪一类所属对象——决定GetZone()/GetBuilding()/GetRoom()哪一个非空。
enum DOOR_KIND_TYPE : int { DOOR_KIND_ZONE, DOOR_KIND_BUILDING, DOOR_KIND_ROOM };

// 门禁状态——数值和DoorSpec::access的int字面值(0/1/2)保持一致，ResolveDoorSpec/
// Map::CreateDoor按这个顺序转换。
enum DOOR_ACCESS_TYPE : int { DOOR_ACCESS_OPEN, DOOR_ACCESS_OWNER, DOOR_ACCESS_LOCKED };

// Door：一扇真正放了门扇的门的运行期实体——只有DoorSpec.mesh非空才会被Map::CreateDoor
// 创建，纯门洞(没标签/查不到spec/mesh为空)不会有对应的Door实例。外观/开关方式全部来自
// 创建时传入的DoorSpec快照(不持有spec来源mod的指针，几何/mesh/style等创建后不会再变)，
// 门本身没有需要多态的行为，所以不做DoorMod/DoorFactory这套mod机制。
class Door {
public:
	Door() = delete;

	// id/interactName/flippedSide都由Map::CreateDoor算好传入(分别依赖Map自己维护的全局
	// 计数器/per-owner序号计数器/随机数)，Door自己不重新计算。interactName非空时在构造函数
	// 内部创建Script(照抄Vehicle::Vehicle的模式)，为空则script恒为nullptr。zone/building/
	// room三者按kind只有一个非空，Door不持有它们的生命周期。
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
	int GetStyle() const;                         // 0=Slide 1=Swing，见door_spec.h
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

	// 单扇门(leaves==1)的朝向——spec.randomSide为true时创建时掷过的硬币结果，之后固定
	// 不变；leaves==2或spec.randomSide为false时恒为false，Forever层按false=默认朝向处理。
	bool IsFlippedSide() const;

	// 门禁：Open→任何人都能过；Locked→任何人都不能过；Owner→所属对象(Zone/Building/Room)
	// GetStated()==true时视同Open，否则isPlayer为true按Open处理(占位，这次没有真正的玩家
	// 门禁判定)，否则who是所属对象的GetOwner()或者在Allow()登记过名字的citizen才能过。
	// who为空(调用方无法判定身份，比如空车/公交)时Owner门禁下按false处理(不能过)。
	DOOR_ACCESS_TYPE GetAccess() const;
	void SetAccess(DOOR_ACCESS_TYPE access);
	// 按名字登记允许通过——Map::ApplyChange处理AllowDoorChange时只有一个citizen名字字符串，
	// 没有办法从Map反查到真正的Citizen*(Map不持有Populace引用)，所以用名字比对，不是指针
	// 比对，见CanPass的实现。
	void Allow(const std::string& citizenName);
	bool CanPass(const Citizen* who, bool isPlayer) const;

	// 交互——只有GetScript()!=nullptr(即spec.name非空)的门才有意义。
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
	std::unordered_set<std::string> allowedCitizenNames;

	Script* script = nullptr;
	std::vector<std::string> options;
};

// 建筑门/房间门的放置判定：tag为空、doorSpecs里查不到这个tag、查到了但mesh为空，这三种
// 情况都返回nullptr(墙上的洞照常保留，只是不放门)；其余情况返回对应的DoorSpec。园区门
// 不走这个函数(没有tag概念)，直接在调用处判断gate.door.mesh是否为空，见zone.cpp。
const DoorSpec* ResolveDoorSpec(const std::unordered_map<std::string, DoorSpec>& doorSpecs,
	const std::string& tag);
