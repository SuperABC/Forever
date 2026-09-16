#pragma once

#include <string>
#include <vector>

class Lot;
class Zone;
class Building;
class Room;

// 照抄老工程Person(E:\Projects\Forever_UE\Source\Core\populace\person.h)的GENDER_TYPE，
// 名字保持不变。
enum GENDER_TYPE : int { GENDER_FEMALE, GENDER_MALE };

// Citizen：老工程里叫Person，这次进入populace域第一次迁移，只搬"姓名/性别/生日"这三项+
// 这次要求的"所在lot/园区/建筑/房间"位置字段+"3D坐标"——老工程Person身上的
// relatives/personality/acquaintances/assets/jobs/scheduler/educationExperiences等全部
// 不搬，依赖还没迁移的Society/Industry/Job域，等对应域迁移到了再回来加，详见
// Source/Core/populace/populace.md。
//
// 纯Core类型，不#include任何UE头文件(和Building/Room同一个约定)——Lot/Zone/Building/Room
// 这里只做前向声明，3D坐标用裸float三元组而不是FVector，转UE坐标的工作留给Forever层的
// ACitizenElement，和ComputeWorldPosition一类函数把map单位转UE单位的既有分工一致。
class Citizen {
public:
	Citizen(const std::string& name, GENDER_TYPE gender, int birthYear, int birthMonth, int birthDay);

	const std::string& GetName() const;
	GENDER_TYPE GetGender() const;
	int GetBirthYear() const;
	int GetBirthMonth() const;
	int GetBirthDay() const;

	// 粗略年龄(按年份差，不精确到月/日——这次没有真正的日历/游戏时钟系统，够用来给
	// Map::Checkin()做成年/未成年判断)。
	int GetAge(int currentYear) const;

	// 配偶/子女——Populace::GenerateCitizens()模拟结束后，只把幸存双方都物化成Citizen的
	// 婚姻/亲子关系保留下来(见populace.cpp)，供Map::Checkin()决定"配偶/未成年子女要不要
	// 跟着一起搬进同一间"。这次不保留父母/兄弟姐妹等其它亲属关系——Checkin只需要这两种。
	Citizen* GetSpouse() const;
	void SetSpouse(Citizen* value);
	const std::vector<Citizen*>& GetChildren() const;
	void AddChild(Citizen* child);

	// 所在lot/园区/建筑/房间——这是"家"(tenancy)，Map::Checkin()分配住处时一次性设好，
	// 这次没有"搬家"逻辑，不需要单独的SetStatus级联清空(老工程那套是给"人可能在
	// zone/building/room间移动"场景用的，这次citizen分配后不会再变，保留最简单的setter
	// 就够)。**这不代表citizen当前人在这里**——人在家还是在别处是另一个概念，见
	// GetCurrentRoom()。
	Lot* GetLot() const;
	void SetLot(Lot* value);
	Zone* GetZone() const;
	void SetZone(Zone* value);
	Building* GetBuilding() const;
	void SetBuilding(Building* value);
	Room* GetRoom() const;
	void SetRoom(Room* value);

	// 当前物理位置所在的房间——和上面的GetRoom()(家/tenancy)是两个独立的概念，不能互相
	// 替代：一个citizen可能人不在家(以后有真正移动AI之后，currentRoom可能是别的房间)，
	// 也可能待在一个自己并不租住的房间里(以后有访客/工作场景时会出现)。这次没有真正的
	// 移动AI，citizen只会在Map::Checkin()分配住处的同时"住进去"，两者初始状态天然重合，
	// 但数据结构上必须分开存。对应Room::GetOccupants()——citizen当前在哪个房间，就应该
	// 出现在那个房间的occupants列表里，两边由调用方(目前是Map::Checkin())同步维护，这个
	// 类自己不做级联同步。
	Room* GetCurrentRoom() const;
	void SetCurrentRoom(Room* value);

	// 3D坐标——留空(hasPosition为false)表示"换房间后从未在场景里实例化过"；首次实例化时
	// Forever层按房间中心+随机偏移算出来后写回，此后一直复用这份记录。
	bool HasPosition() const;
	void GetPosition(float& outX, float& outY, float& outZ) const;
	void SetPosition(float x, float y, float z);

private:
	std::string name;
	GENDER_TYPE gender;
	int birthYear;
	int birthMonth;
	int birthDay;

	Citizen* spouse = nullptr;
	std::vector<Citizen*> children;

	Lot* lot = nullptr;
	Zone* zone = nullptr;
	Building* building = nullptr;
	Room* room = nullptr;
	Room* currentRoom = nullptr;

	bool hasPosition = false;
	float posX = 0.f;
	float posY = 0.f;
	float posZ = 0.f;
};
