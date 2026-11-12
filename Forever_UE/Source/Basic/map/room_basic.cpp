#include "room_basic.h"

using namespace std;

int ResidenceRoom::count = 0;

ResidenceRoom::ResidenceRoom() : id(count++) {
}

const char* ResidenceRoom::GetName() {
	name = "ResidenceRoom" + to_string(id);
	return name.data();
}

void ResidenceRoom::SetProperty() {
	isResidence = true;
	residenceCapacity = 1;
	rent = 2000; // 每日房租20元
	// 挂示例脚本entity_event(进出事件只发给这个实体自己的Script)，见Resource/Story/entity_event.script。
	milestoneNames = { "entity_event" };
	// 住宅房间的门：四种高密度住宅layout里single/row的门洞标签统一是"home"，用和其他房间一样的读卡器门。
	doorSpecs["home"].mesh = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/SKM_door_with_cardreader.SKM_door_with_cardreader";
	doorSpecs["home"].animClass = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/ABP_Door.ABP_Door_C";
}

int ShopRoom::count = 0;

ShopRoom::ShopRoom() : id(count++) {
}

const char* ShopRoom::GetName() {
	name = "ShopRoom" + to_string(id);
	return name.data();
}

void ShopRoom::SetProperty() {
	isWorkspace = true;
	workspaceCapacity = 5;
	rent = 2000;

	doorSpecs["room"].mesh = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/SKM_door_with_cardreader.SKM_door_with_cardreader";
	doorSpecs["room"].animClass = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/ABP_Door.ABP_Door_C";
	// 商店房间的门有名字、挂脚本，可以交互("敲门")。商店用的一楼模板(preset_lobby_linear_fg+/
	// preset_circle_double_fg+-)里门标签全是"room"、没有"entrance"，ShopBuilding的
	// doorSpecs["entrance"]对不上任何门，所以挂在这里，见Resource/Story/door_shop_entrance.script。
	doorSpecs["room"].name = "商店大门";
	doorSpecs["room"].scriptModName = "script_empty";
	doorSpecs["room"].milestoneNames = { "door_shop_entrance" };
}

int RestaurantRoom::count = 0;

RestaurantRoom::RestaurantRoom() : id(count++) {
}

const char* RestaurantRoom::GetName() {
	name = "RestaurantRoom" + to_string(id);
	return name.data();
}

void RestaurantRoom::SetProperty() {
	isWorkspace = true;
	workspaceCapacity = 5;
	rent = 2000;
}

int OfficeRoom::count = 0;

OfficeRoom::OfficeRoom() : id(count++) {
}

const char* OfficeRoom::GetName() {
	name = "OfficeRoom" + to_string(id);
	return name.data();
}

void OfficeRoom::SetProperty() {
	isWorkspace = true;
	workspaceCapacity = 5;
	rent = 2000;

	doorSpecs["room"].mesh = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/SKM_door_with_cardreader.SKM_door_with_cardreader";
	doorSpecs["room"].animClass = "/Game/3rdParty/Door_with_cardreader/StaticMeshes/ABP_Door.ABP_Door_C";
}

int WarehouseRoom::count = 0;

WarehouseRoom::WarehouseRoom() : id(count++) {
}

const char* WarehouseRoom::GetName() {
	name = "WarehouseRoom" + to_string(id);
	return name.data();
}

void WarehouseRoom::SetProperty() {
	isStorage = true;
	storageType = "storage_shop";
	// 仓库也有管理员：容纳量1，Organization按容纳量满编定员。
	isWorkspace = true;
	workspaceCapacity = 1;
	rent = 2000;
}

int ParkingRoom::count = 0;

ParkingRoom::ParkingRoom() : id(count++) {
}

const char* ParkingRoom::GetName() {
	name = "ParkingRoom" + to_string(id);
	return name.data();
}

void ParkingRoom::SetProperty() {
	isParking = true;
	// 房间正中心一个车位，朝向跟房间一致；种类里的"bus"对应原先InitParkedVehicles里"一半概率换成
	// 可驾驶公交"的临时测试逻辑(现在是数据)，不想要公交就从这里删掉"bus"。
	parkingSpots = { { {0.5f, 0.f, 0.5f, 0.f}, 0.f, { "car", "bus", "truck" } } };
}

int TrainStationRoom::count = 0;

TrainStationRoom::TrainStationRoom() : id(count++) {
}

const char* TrainStationRoom::GetName() {
	name = "TrainStationRoom" + to_string(id);
	return name.data();
}

int AirportRoom::count = 0;

AirportRoom::AirportRoom() : id(count++) {
}

const char* AirportRoom::GetName() {
	name = "AirportRoom" + to_string(id);
	return name.data();
}

void AirportRoom::SetProperty() {
	// 房间沿正面墙(u轴)的两侧各一个停机位。用房间局部坐标系(见ParkingSpot注释)，房间按朝向旋转时车位跟着转：
	// 第一对是u轴，ratio相对房间角点，0/1正好是房间沿正面墙的两端边缘，-0.15/1.15落在房间外侧
	// (估计值，后面按实际机翼宽度调)；第二对是v轴，0.5=和房间同一条中线。
	// rotationDegrees=0表示朝向房间正面(朝路)；跑道在房间深处(AirStation的depthRatio 0.65/0.85)，
	// 所以停机位朝向跑道是180度。
	constexpr float kLeftAlong = -0.15f;
	constexpr float kRightAlong = 1.15f;
	constexpr float kExtraOffset = 0.5f; // 再往两侧挪的绝对距离(地图单位，1单位=10米)，不受房间尺寸影响
	isParking = true;
	parkingSpots = {
		{ {kLeftAlong, -kExtraOffset, 0.5f, 0.f}, 180.f, { "plane" } },
		{ {kRightAlong, kExtraOffset, 0.5f, 0.f}, 180.f, { "plane" } },
	};
}

int FarmRoom::count = 0;

FarmRoom::FarmRoom() : id(count++) {
}

const char* FarmRoom::GetName() {
	name = "FarmRoom" + to_string(id);
	return name.data();
}

void FarmRoom::SetProperty() {
	isManufacture = true;
	manufactureTypes = { "manufacture_farm" };
	isWorkspace = true;
	workspaceCapacity = 10;
	rent = 2000;
}

int RanchRoom::count = 0;

RanchRoom::RanchRoom() : id(count++) {
}

const char* RanchRoom::GetName() {
	name = "RanchRoom" + to_string(id);
	return name.data();
}

void RanchRoom::SetProperty() {
	isManufacture = true;
	manufactureTypes = { "manufacture_ranch" };
	isWorkspace = true;
	workspaceCapacity = 10;
	rent = 2000;
}

int FoodFactoryRoom::count = 0;

FoodFactoryRoom::FoodFactoryRoom() : id(count++) {
}

const char* FoodFactoryRoom::GetName() {
	name = "FoodFactoryRoom" + to_string(id);
	return name.data();
}

void FoodFactoryRoom::SetProperty() {
	isManufacture = true;
	manufactureTypes = { "manufacture_food_factory" };
	isWorkspace = true;
	workspaceCapacity = 10;
	rent = 2000;
}
