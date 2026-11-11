#pragma once

#include "common/mod_state.h"
#include "map/feature_spec.h"
#include "map/general_geometry.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <tuple>
#include <utility>


// (component名字, id)分组key的hash函数对象,singles/rows用。
struct BuildingComponentKeyHash {
	size_t operator()(const std::pair<std::string, int>& key) const {
		return std::hash<std::string>()(key.first) ^ (std::hash<int>()(key.second) << 1);
	}
};

// 楼体在Building自身Quad里的位置,全是比例(中心比例0.5=居中)。故意不复用Quad类型,
// 避免把"中心点+尺寸"语义挪用成"比例袋子"。
struct BuildingFootprintSpec {
	float centerRatioX = 0.5f;
	float centerRatioY = 0.5f;
	float sizeRatioX = 1.f;
	float sizeRatioY = 1.f;
};

// 一层楼可配置的渲染资产,留空的字段由Forever层用组件级默认值兜底。
struct FloorAssetSpec {
	std::string wallMaterial;
	std::string floorMaterial;
	std::string ceilingMaterial;
	std::string stairMeshPath;
	// 近处外观(墙/地板/天花板)是否由框架构造,见building_mod.md。
	bool buildShell = true;
	std::string rampMeshPath;
};

// 一层楼的.layout模板、朝向、资产。
struct FloorLayoutSpec {
	std::string templateName;
	int face = FACE_SOUTH;
	FloorAssetSpec assets;
};

// 一台电梯轿厢。shaftIndex是每层.layout模板"elevators"数组里的下标,假定跨楼层指向同一口井道。
struct ElevatorCabinSpec {
	int shaftIndex = 0;
	int minFloor = 0;  // 人类直觉层号(0=1楼，负数=地下)，含
	int maxFloor = 0;  // 含
	std::string cabinMeshPath; // 留空用组件默认Elevator mesh
	// milestoneNames非空才创建Script,见Core/map/cabin.md。
	std::string scriptModName = "script_empty";
	std::vector<std::string> milestoneNames;
};

// 设计见building_mod.md。
class BuildingMod {
public:
	BuildingMod() = default;
	virtual ~BuildingMod() = default;

	virtual const char* GetType() const = 0;

	virtual const char* GetName() = 0;

	// 被政府(公有)拥有的概率[0,1],Map::Checkin读取。
	float stateOwnedChance = 0.2f;

	// 落地后由Building::Layout()调用一次;direction是引用参数,语义见building_mod.md。
	virtual void Layout(int& direction, const Quad& quad,
		const std::unordered_map<int, Road*>& boundaryRoads) {}

	// 整张地图生成完后由Map::InitVegetation调用一次,在这里用AddTree*提交树请求。默认不种树。
	virtual void LayoutTrees(const Quad& quad, const std::unordered_map<int, Road*>& boundaryRoads) {}

	// 楼体位置/楼层/LOD材质,在Layout(...)里设置。
	BuildingFootprintSpec footprint;
	int basements = 0;
	int layers = 1;
	// 楼层高度(地图单位),自最深地下室到最高层,长度应为basements+layers,不对由Building::Layout()补默认值。
	std::vector<float> floorHeights;
	// 远处LOD cube的材质软路径,留空用默认灰色。
	std::string lodMaterial;

	// 这栋楼承载的StationMod id(如"station_bus"),空=非站点建筑,见building_mod.md。
	std::string stationMod;

	// 建筑的Script,milestoneNames非空才创建,见Core/map/map.md"实体脚本"一节。在Layout(...)里设置。
	std::string scriptModName = "script_empty";
	std::vector<std::string> milestoneNames;

	// level:0=1楼,负数=地下室(-1=地下一层);同一level重复调用覆盖。
	void AssignFloor(int level, const std::string& templateName, int face, FloorAssetSpec assets = {});
	// 所有层(从-basements到layers-1)都用同一个模板+资产。
	void AssignFloors(const std::string& templateName, int face, FloorAssetSpec assets = {});
	// 每层各自一个模板+资产,按最深地下室到最高层排列,长度应为basements+layers。
	void AssignFloors(const std::vector<std::string>& templateNames, int face,
		const std::vector<FloorAssetSpec>& assets = {});

	// 第level层第slot个single槽位实例化成room类型的Room,归入(component,id)分组。
	void AssignRoom(int level, int slot, const std::string& room, const std::string& component, int id);
	// 第level层第slot个row槽位按目标面积acreage切成若干Room。
	void ArrangeRow(int level, int slot, const std::string& room, float acreage,
		const std::string& component, int id);

	// 声明一台电梯轿厢,同一口井可声明多台(楼层范围不能重叠,重叠的后一条被跳过)。
	void AssignElevatorCabin(int shaftIndex, int minFloor, int maxFloor, std::string cabinMeshPath = "",
		std::string scriptModName = "script_empty", std::vector<std::string> milestoneNames = {});

	// 子类还必须实现一组static方法(基类不声明),通过RegisterBuilding注册,见building_mod.md。

	// 供Building::Layout()读取,mod通过上面的Assign*/ArrangeRow写入,不直接操作。
	std::unordered_map<int, FloorLayoutSpec> floors;
	// (component,id) -> [(level, slot, roomType)]，AssignRoom记录的single槽位。
	std::unordered_map<std::pair<std::string, int>, std::vector<std::tuple<int, int, std::string>>,
		BuildingComponentKeyHash> singles;
	// (component,id) -> [(level, slot, roomType, acreage)]，ArrangeRow记录的row槽位。
	std::unordered_map<std::pair<std::string, int>, std::vector<std::tuple<int, int, std::string, float>>,
		BuildingComponentKeyHash> rows;
	// AssignElevatorCabin记录的电梯轿厢列表。
	std::vector<ElevatorCabinSpec> cabins;

	// 地板/天花板标签->材质实例软路径(如"roof"->"/Game/Asset/Building/Material/Black.Black"),在Layout()里用
	// SetSurfaceMaterial填写。.layout里ceilings/grounds条目带"tag"才会查这张表;没有标签、或标签没登记,
	// 退回楼层资产/框架默认的白色。Core在Building::ReadFloor实例化地板/天花板时把路径抄到每块上。
	void SetSurfaceMaterial(const std::string& tag, const std::string& materialPath);
	std::unordered_map<std::string, std::string> surfaceMaterials;

	// 非single/row元素(目前只有corridor)上的门,tag->DoorSpec,在Layout()里填写,见feature_spec.h/door.h。
	std::unordered_map<std::string, DoorSpec> doorSpecs;

	// 在LayoutTrees里提交树请求(记进trees),参考系见building_mod.md。
	void AddTreeSingle(const PointParams& position, float scale, const std::string& meshPath);
	void AddTreeLine(const PointParams& from, const PointParams& to, int num, float scale,
		const std::string& meshPath);
	void AddTreeArea(const PointParams& from, const PointParams& to, int num, float scale,
		const std::string& meshPath);
	TreeRequests trees;

	// 放一个自定义StaticMesh(如成品建筑模型),在Layout()里调用,见feature_spec.h的DecorationSpec。
	void AddDecoration(const DecorationSpec& spec);

	// AddDecoration的记录。
	std::vector<DecorationSpec> decorations;

	// ---- 存档(见common/mod_state.md) ----
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	virtual int GetStateVersion() const { return 0; }
	// GetName用的static count,存档保存/读档恢复;没有计数器的类保持默认。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
