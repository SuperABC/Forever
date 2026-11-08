#pragma once

#include "class.h"
#include "populace/experience.h" // RELATIONSHIP_CATEGORY——Relation::category字段用

#include <string>
#include <vector>
#include <unordered_map>


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
	// 各项属性初始化为0。没有无参默认构造：category必须在创建时指定，语义见citizen.md。
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

// 市民数据类：纯Core类型，3D坐标用裸float三元组，设计见citizen.md。
class Citizen {
	// 存档：PopulaceSaveCodec直接读写私有字段(populace_save.md)。
	friend class PopulaceSaveCodec;
public:
	Citizen(const std::string& name, GENDER_TYPE gender, int birthYear, int birthMonth, int birthDay);

	~Citizen(); // delete scheduler和全部experiences

	const std::string& GetName() const;
	GENDER_TYPE GetGender() const;
	int GetBirthYear() const;
	int GetBirthMonth() const;
	int GetBirthDay() const;

	// 头像：avatar是脚本指定的头像名(空=没指定)，avatarIndex是随机的自动头像编号，见citizen.md。
	static constexpr int kAvatarIndexRange = 1000000000;
	const std::string& GetAvatar() const;
	void SetAvatar(const std::string& avatar);
	int GetAvatarIndex() const;

	// 粗略年龄(按年份差，不精确到月/日)。
	int GetAge(int currentYear) const;

	// 配偶/子女：只保留幸存双方都物化成Citizen的关系，不保留父母/兄弟姐妹，见citizen.md。
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

	// 熟人列表整表，供调试/日志遍历；正常逻辑按姓名查询。
	const std::unordered_map<std::string, Relation>& GetAcquaintances() const;

	// 四类人际关系的可追溯历史记录，见experience.md。Citizen取得所有权。
	const std::vector<Experience*>& GetExperiences() const;
	void AddExperience(Experience* experience);

	// 当前情人(可以有多个)：对experiences的派生查询，不是独立字段。
	std::vector<Citizen*> GetCurrentLovers() const;

	// 最后一次毕业的年份，从未上过学/仍在读返回-1。独立存储字段，见citizen.md。
	int GetLastGraduationYear() const;
	void SetLastGraduationYear(int year);

	// 所在lot/园区/建筑/房间是"家"，不代表人当前在这里(见GetCurrentRoom)。
	Lot* GetLot() const;
	void SetLot(Lot* value);
	Zone* GetZone() const;
	void SetZone(Zone* value);
	Building* GetBuilding() const;
	void SetBuilding(Building* value);
	Room* GetRoom() const;
	void SetRoom(Room* value);

	// 所属户（住在家里才有），由Room::AddResident维护。
	Household* GetHousehold() const;
	void SetHousehold(Household* value);

	// 当前物理位置所在的房间，和GetRoom()(家)是两个独立概念，见citizen.md。
	// SetCurrentRoom同步维护房间的occupants。
	Room* GetCurrentRoom() const;
	void SetCurrentRoom(Room* value);

	// 3D坐标：hasPosition为false表示还没有实例化过位置，见citizen.md。
	bool HasPosition() const;
	void GetPosition(float& outX, float& outY, float& outZ) const;
	void SetPosition(float x, float y, float z);

	// 把hasPosition重置回false，不改posX/Y/Z本身。
	void ClearPosition();

	// 没有ACitizenElement的市民在路上时的Core行进模拟，见citizen.md。路径、目的地和速度随人口段存档
	// (存档前由UE侧RebaseWalk改写成"以当前位置为起点的剩余路径")，读档时用BeginWalk(.., 0, ..)恢复。
	struct WalkPoint { float x, y, z; };
	bool IsWalking() const;
	Room* GetWalkDestination() const;
	// 开始行进：路径第一个点就是当前位置(写进SetPosition)。本函数不碰currentRoom。
	void BeginWalk(const std::vector<WalkPoint>& path, Room* destination, double nowSeconds, float speed);
	// 推进到nowSeconds：到达时SetCurrentRoom(destination)+ClearPosition+结束行进并返回true；
	// 否则更新SetPosition返回false。没在行进返回false。
	bool AdvanceWalk(double nowSeconds);
	// 取出剩余路径(第一个点是推算出的当前位置)，供生成Actor后接着走；不改行进状态。
	void GetRemainingWalk(double nowSeconds, std::vector<WalkPoint>& outPath) const;
	void EndWalk();
	float GetWalkSpeed() const;
	// 把行进改写成"从nowSeconds推算出的当前位置起的剩余路径"，起算时间设为nowSeconds：轨迹不变，
	// 但路径首点就是当前位置、已走弧长为0。存档前调用，读档后用它把起算时间对齐到新的游戏时钟。
	void RebaseWalk(double nowSeconds);
	// 读档后把起算时间设成nowSeconds(存档里的路径本来就是以当前位置为起点，已走弧长为0)。
	void RestartWalk(double nowSeconds);

	// 行走实例化概率：每次开始走路（寻路落地时）按这个概率抽一次，决定这次行走途中经过玩家附近时要不要生成Actor
	// (RollWalkInstantiable)。抽中才参与实例化，没抽中的这趟路全程只在Core里推算位置，不占Actor名额，
	// 街上就不会被走路的人挤满上限。默认-1(跟随UE侧按人流动态算的全局概率)；脚本spawn出来的故事市民设成1。随人口段存档。
	float GetWalkInstantiateProbability() const;
	void SetWalkInstantiateProbability(float value);
	// globalProbability：自己没单独设过概率(默认-1)时采用的全局概率。
	void RollWalkInstantiable(float globalProbability);
	bool IsWalkInstantiable() const;

	// 存档时这个市民是否有Actor(已实例化)。只在存档/读档之间有意义：存档前由UE侧按场景写入，读档后
	// UE侧据此强制先生成这些市民，生成完清掉。不参与任何游戏逻辑。
	bool WasSpawned() const;
	void SetWasSpawned(bool value);

	// 动态属性(目前只有饱食度)：只有statsActive的市民参与Populace::Tick的衰减，见citizen.md。
	bool IsStatsActive() const;
	void ActivateStats(); // 已实例化则什么也不做
	float GetSatiety() const; // 对外保留两位小数(内部存精确值，否则每帧衰减量小于0.005时永远降不下去)
	void AddSatiety(float delta); // 截断到[0,1]，超出不惩罚
	void DecaySatiety(double days); // 衰减：乘0.5^days(days=经过的游戏天数)，一天减半

	// 是否接入经济系统，见citizen.md。不存档。
	bool IsInEconomy() const;
	void SetInEconomy(bool value);

	// 当前持有的工作——Job由Organization持有所有权，这里只存一个不持有所有权的指针。
	Job* GetJob() const;
	void SetJob(Job* value);

	// 负责下班之后行为的调度器，Citizen独占持有所有权，见scheduler.md。
	Scheduler* GetScheduler() const;
	void SetScheduler(Scheduler* value);

	// 当前可供玩家点击的对话选项文本，由Populace::ApplyChange处理add_option/remove_option。
	const std::vector<std::string>& GetOptions() const;
	void AddOption(const std::string& option);
	void RemoveOption(const std::string& option);

	// 生命值与死亡，见citizen.md。
	float GetHealth() const;
	float GetMaxHealth() const;
	// TakeDamage把血量扣到0时调用Die()；Heal不会让死人复活，复活只能走Revive()。
	float TakeDamage(float amount);
	float Heal(float amount);
	bool IsDead() const;
	// 死亡：isDead置位、血量归零、结束行进、离开当前房间并清位置；不动“家”。
	void Die();
	// 复活：清除isDead，血量恢复(<=0或超过maxHealth取满血)；不设房间/位置。
	void Revive(float health = -1.f);

	// 离开城市(Emigration)：departed置位、断开工作、结束行进、离开当前房间并清位置；不动“家”和户，
	// 账户由Emigration结算。不可逆。
	bool IsDeparted() const;
	void Depart();
	// 还在城里活动的人：没死、没离城。经济/流式生成等“人在不在场”的判断统一用它。
	bool IsActive() const;

private:
	std::string name;
	GENDER_TYPE gender;
	int birthYear;
	int birthMonth;
	int birthDay;
	int avatarIndex;
	std::string avatar;

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
	Household* household = nullptr;
	Room* currentRoom = nullptr;

	bool hasPosition = false;
	float posX = 0.f;
	float posY = 0.f;
	float posZ = 0.f;

	bool statsActive = false;
	float satiety = 0.f;
	bool inEconomy = true;

	bool wasSpawned = false;
	float walkInstantiateProbability = -1.f; // <0=跟随全局动态概率
	bool walkInstantiable = true; // 当前这趟行走是否参与实例化，开始走路时按概率抽，见RollWalkInstantiable

	// 行进模拟状态，见BeginWalk。walkDistances[i]是路径起点到第i个点的累计弧长。
	Room* walkDestination = nullptr;
	std::vector<WalkPoint> walkPath;
	std::vector<float> walkDistances;
	double walkStartTime = 0.0;
	float walkSpeed = 0.f;
	// 推算(nowSeconds)对应的位置和已经走过的路径段下标。
	void SampleWalk(double nowSeconds, WalkPoint& outPoint, size_t& outSegment, bool& outFinished) const;

	Job* job = nullptr;
	Scheduler* scheduler = nullptr;

	std::vector<std::string> options;

	// 100点是随手给的默认值，待PIE试手感再调。
	float health = 100.f;
	float maxHealth = 100.f;
	bool isDead = false;
	bool departed = false;
};
