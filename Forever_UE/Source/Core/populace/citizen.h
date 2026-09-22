#pragma once

#include "class.h"
#include "populace/experience.h" // RELATIONSHIP_CATEGORY——Relation::category字段用

#include <string>
#include <vector>
#include <unordered_map>


// 照抄老工程Person(E:\Projects\Forever_UE\Source\Core\populace\person.h)的GENDER_TYPE，
// 名字保持不变。
enum GENDER_TYPE : int { GENDER_FEMALE, GENDER_MALE };

enum PERSONALITY_TYPE : int {
	PERSONALITY_APPEARANCE, // 颜值
	PERSONALITY_FITNESS, // 身材
	PERSONALITY_ENERGY, // 体能
	PERSONALITY_INTELLIGENCE, // 智商
	PERSONALITY_ELOQUENCE, // 口才
	PERSONALITY_CONFIDENCE, // 自信心
	PERSONALITY_MORALITY, // 德行
	PERSONALITY_MENTALITY, // 心态
	PERSONALITY_IMAGINATION, // 想象力
	PERSONALITY_KNOWLEDGE, // 学识
	PERSONALITY_ART, // 艺术
	PERSONALITY_REASONING, // 理性
	PERSONALITY_PERCEPTION // 感性
};

enum RELATION_TYPE : int {
	RELATION_FAMILIARITY, // 熟悉程度
	RELATION_RESPECT, // 尊敬程度
	RELATION_FAVOUR, // 好感程度
	RELATION_TRUST, // 信任程度
	RELATION_COMPETING, // 竞争程度
	RELATION_RELIABILITY // 依赖程度
};

// 个人属性
struct Personality {
	// 构造个人属性，各项属性随机初始化为[0, 1)区间内的值
	Personality();

	// 按类型访问属性字段（可写）
	float& operator[](PERSONALITY_TYPE type);

	// 按类型访问属性字段（只读）
	float operator[](PERSONALITY_TYPE type) const;

	// 按脚本变量名访问属性字段（可写）
	float& operator()(const std::string& name);

	// 脚本变量名访问属性字段（只读）
	float operator()(const std::string& name) const;

	// 获取属性类型对应的脚本变量名
	static std::string GetFieldName(PERSONALITY_TYPE type);

	// 颜值
	float appearance;

	// 身材
	float fitness;

	// 体能
	float energy;

	// 智商
	float intelligence;

	// 口才
	float eloquence;

	// 自信心
	float confidence;

	// 德行
	float morality;

	// 心态
	float mentality;

	// 想象力
	float imagination;

	// 学识
	float knowledge;

	// 艺术
	float art;

	// 理性
	float reasoning;

	// 感性
	float perception;
};

// 人际关系
struct Relation {
	// 构造人际关系，各项属性初始化为0。category标记这两个人是四类关系（亲属/同学/同事/
	// 情感）里的哪一种——同一个RELATIONSHIP_CATEGORY枚举，见experience.h。配偶标记成
	// RELATIONSHIP_KINSHIP（亲属），不是RELATIONSHIP_ROMANTIC——情感类别专指恋人/情人，
	// 婚姻关系本身归亲属（对应的恋爱阶段仍然会生成一条EmotionExperience，但那是
	// experiences列表里的历史记录，不影响这里的category），见populace.md"四类人际关系
	// 生成"一节。没有无参默认构造——category必须在创建时就明确指定，不存在"还不知道
	// 这是什么关系"的中间状态。
	Relation(RELATIONSHIP_CATEGORY category);

	// 按类型访问关系字段（可写）
	float& operator[](RELATION_TYPE type);

	// 按类型访问关系字段（只读）
	float operator[](RELATION_TYPE type) const;

	// 按脚本变量名访问关系字段（可写）
	float& operator()(const std::string& name);

	// 按脚本变量名访问关系字段（只读）
	float operator()(const std::string& name) const;

	// 获取关系类型对应的脚本变量名
	static std::string GetFieldName(RELATION_TYPE type);

	// 这两个人是四类关系里的哪一种——创建后不再改变。
	RELATIONSHIP_CATEGORY category;

	// 熟悉程度
	float familiarity;

	// 尊敬程度
	float respect;

	// 好感程度
	float favour;

	// 信任程度
	float trust;

	// 竞争程度
	float competing;

	// 依赖程度
	float reliability;
};

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

	// Scheduler由Citizen自己独占持有(和Job由Organization持有不同——Scheduler没有类似
	// "组织"这样的自然归属者，见scheduler.md)，因此需要一个真正的析构函数delete它，
	// 不再是纯POD式默认析构。
	~Citizen();

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

	// 个人属性与熟人列表接口
	const Personality& GetPersonality() const;
	void SetPersonalityValue(PERSONALITY_TYPE type, float value);
	void AdjustPersonalityValue(PERSONALITY_TYPE type, float delta);
	void AddAcquaintance(const std::string& name, RELATIONSHIP_CATEGORY category);
	const Relation& GetAcquaintance(const std::string& name) const;
	void SetAcquaintanceValue(const std::string& name, RELATION_TYPE type, float value);
	void AdjustAcquaintanceValue(const std::string& name, RELATION_TYPE type, float delta);

	// 熟人列表整表——供调试/日志用途遍历全部acquaintances（比如T键打印附近市民关系数据，
	// 见CitizenElement.md），正常游戏逻辑按姓名查询用上面几个方法即可，不需要整表遍历。
	const std::unordered_map<std::string, Relation>& GetAcquaintances() const;

	// 四类人际关系(亲属/同学/同事/情感)的可追溯历史记录，和acquaintances对应——见
	// experience.h、populace.md"四类人际关系生成"一节。取得Experience的所有权，
	// ~Citizen()统一delete。
	const std::vector<Experience*>& GetExperiences() const;
	void AddExperience(Experience* experience);

	// 当前情人(可以同时有多个)——对experiences的派生查询，不是独立存储字段：找所有
	// category是RELATIONSHIP_ROMANTIC、IsOngoing()为true、且GetOther()不是当前配偶的
	// EmotionExperience，收集其GetOther()。
	std::vector<Citizen*> GetCurrentLovers() const;

	// 这个人自己的求学阶段——最后一次毕业的年份，从未上过学/仍在读时返回-1。独立存储
	// 字段(不是experiences的派生查询)：这是citizen自己的事实性数据，不是和某个具体他人
	// 的关系，见populace.md同一节"关键设计决策"。Society::GenerateEmploymentHistory()
	// 用它给入职年份定下限。
	int GetLastGraduationYear() const;
	void SetLastGraduationYear(int year);

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

	// 把hasPosition重置回false(和"换房间后从未在场景里实例化过"是同一个状态)，不改
	// posX/Y/Z本身(反正hasPosition==false时不会被读取)。给"citizen当前没有对应
	// ACitizenElement、瞬移到新room"这个场景用，见UForeverPopulaceFrameworkComponent::
	// RequestWalk。
	void ClearPosition();

	// 当前持有的工作——Job由Organization持有所有权，这里只存一个不持有所有权的指针。
	Job* GetJob() const;
	void SetJob(Job* value);

	// 负责citizen下班之后行为的调度器——由Populace::AssignSchedulers()在生成citizen之后
	// 加权随机分配一个，Citizen自己独占持有所有权(和Job不同，见~Citizen()注释)。
	Scheduler* GetScheduler() const;
	void SetScheduler(Scheduler* value);

private:
	std::string name;
	GENDER_TYPE gender;
	int birthYear;
	int birthMonth;
	int birthDay;

	Citizen* spouse = nullptr;
	std::vector<Citizen*> children;

	Personality personality;
	std::unordered_map<std::string, Relation> acquaintances;
	std::vector<Experience*> experiences;
	int lastGraduationYear = -1;

	Lot* lot = nullptr;
	Zone* zone = nullptr;
	Building* building = nullptr;
	Room* room = nullptr;
	Room* currentRoom = nullptr;

	bool hasPosition = false;
	float posX = 0.f;
	float posY = 0.f;
	float posZ = 0.f;

	Job* job = nullptr;
	Scheduler* scheduler = nullptr;
};
