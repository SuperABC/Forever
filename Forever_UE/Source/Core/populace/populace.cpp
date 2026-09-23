#include "populace/populace.h"

#include "common/utility.h"

#include "populace/citizen.h"
#include "populace/name.h"
#include "populace/scheduler.h"
#include "populace/experience.h"
#include "populace/school.h"
#include "common/registry.h"
#include "common/config.h"
#include "common/error.h"
#include "society/job.h"
#include "story/script.h"
#include "story/change.h"

#include <cmath>
#include <algorithm>
#include <iterator>
#include <map>
#include <unordered_map>


using namespace std;

namespace {

// 复刻老工程Populace::GenerateCitizens内部的LIFE_TYPE/Human——只在模拟期间用来推进人口
// 数量，模拟结束后不对外暴露，也不保留进Citizen(这次Citizen字段范围只有姓名/性别/生日，
// 见citizen.h)。
enum LIFE_TYPE : int {
	LIFE_SINGLE,
	LIFE_MARRY,
	LIFE_BIRTH,
	LIFE_DEAD
};

struct Human {
	int idx;
	string name;
	int birth;
	int marry;
	LIFE_TYPE life;
	GENDER_TYPE gender;
	int father;
	int mother;
	int spouse;
	vector<pair<GENDER_TYPE, int>> childs;
};

constexpr int kDatingMinAge = 14;
constexpr int kMarriageMinAge = 18;

// 四类人际关系Relation随机初始化——数值量级参考，均值/方差实现后可再调。人际关系强度
// 约定俗成非负，clamp到[0,1]，这次不引入负值关系。
void RandomizeKinshipRelation(Citizen* citizen, const string& otherName) {
	citizen->SetAcquaintanceValue(otherName, RELATION_FAMILIARITY, clamp(GetRandomNormal(0.8f, 0.15f), 0.0f, 1.0f));
	citizen->SetAcquaintanceValue(otherName, RELATION_RESPECT, clamp(GetRandomNormal(0.6f, 0.2f), 0.0f, 1.0f));
	citizen->SetAcquaintanceValue(otherName, RELATION_FAVOUR, clamp(GetRandomNormal(0.7f, 0.2f), 0.0f, 1.0f));
	citizen->SetAcquaintanceValue(otherName, RELATION_TRUST, clamp(GetRandomNormal(0.7f, 0.2f), 0.0f, 1.0f));
	citizen->SetAcquaintanceValue(otherName, RELATION_COMPETING, clamp(GetRandomNormal(0.2f, 0.15f), 0.0f, 1.0f));
	citizen->SetAcquaintanceValue(otherName, RELATION_RELIABILITY, clamp(GetRandomNormal(0.7f, 0.2f), 0.0f, 1.0f));
}

void RandomizeRomanticRelation(Citizen* citizen, const string& otherName) {
	citizen->SetAcquaintanceValue(otherName, RELATION_FAMILIARITY, clamp(GetRandomNormal(0.85f, 0.1f), 0.0f, 1.0f));
	citizen->SetAcquaintanceValue(otherName, RELATION_RESPECT, clamp(GetRandomNormal(0.6f, 0.2f), 0.0f, 1.0f));
	citizen->SetAcquaintanceValue(otherName, RELATION_FAVOUR, clamp(GetRandomNormal(0.9f, 0.1f), 0.0f, 1.0f));
	citizen->SetAcquaintanceValue(otherName, RELATION_TRUST, clamp(GetRandomNormal(0.75f, 0.15f), 0.0f, 1.0f));
	citizen->SetAcquaintanceValue(otherName, RELATION_COMPETING, clamp(GetRandomNormal(0.1f, 0.1f), 0.0f, 1.0f));
	citizen->SetAcquaintanceValue(otherName, RELATION_RELIABILITY, clamp(GetRandomNormal(0.75f, 0.15f), 0.0f, 1.0f));
}

void RandomizeClassmateRelation(Citizen* citizen, const string& otherName) {
	citizen->SetAcquaintanceValue(otherName, RELATION_FAMILIARITY, clamp(GetRandomNormal(0.5f, 0.3f), 0.0f, 1.0f));
	citizen->SetAcquaintanceValue(otherName, RELATION_RESPECT, clamp(GetRandomNormal(0.5f, 0.25f), 0.0f, 1.0f));
	citizen->SetAcquaintanceValue(otherName, RELATION_FAVOUR, clamp(GetRandomNormal(0.5f, 0.3f), 0.0f, 1.0f));
	citizen->SetAcquaintanceValue(otherName, RELATION_TRUST, clamp(GetRandomNormal(0.4f, 0.25f), 0.0f, 1.0f));
	citizen->SetAcquaintanceValue(otherName, RELATION_COMPETING, clamp(GetRandomNormal(0.3f, 0.2f), 0.0f, 1.0f));
	citizen->SetAcquaintanceValue(otherName, RELATION_RELIABILITY, clamp(GetRandomNormal(0.4f, 0.25f), 0.0f, 1.0f));
}

// 按年龄递减的"这一年开始一段新恋情"基础年概率——年轻人恋爱更频繁，年龄越大频率越低，
// 参考现实约会频率随年龄下降的趋势，不是一个和年龄无关的常数(审阅时反馈：老版本"按总
// 跨度套公式算总数"会让单身时间越长——往往就是年纪越大的人——分配到越多段恋爱史，
// 完全反了；且已婚情人概率是常数，没有随年龄自然衰减，导致老年人和年轻人一样容易有
// 婚外情)。
float DatingHazardForAge(int age) {
	if (age < 20) return 0.10f;
	if (age < 30) return 0.14f;
	if (age < 40) return 0.07f;
	if (age < 50) return 0.03f;
	if (age < 60) return 0.012f;
	return 0.005f;
}

// 两个citizen是否已经是亲属(用来避免恋爱/情人对象选中近亲)——扫描一方的experiences找
// 是否有一条KinshipExperience指向另一方，见populace.md"四类人际关系生成"一节。
bool IsCloseRelative(Citizen* a, Citizen* b) {
	for (Experience* experience : a->GetExperiences()) {
		if (experience->GetCategory() != RELATIONSHIP_KINSHIP) continue;
		if (static_cast<KinshipExperience*>(experience)->GetOther() == b) return true;
	}
	return false;
}

// 是否已经是citizen当前的情人(避免重复选中同一个人)。
bool IsCurrentLoverOf(Citizen* citizen, Citizen* candidate) {
	for (Citizen* lover : citizen->GetCurrentLovers()) {
		if (lover == candidate) return true;
	}
	return false;
}

// 随机找一个满足条件的恋爱/情人对象：异性、atYear时年满kDatingMinAge岁、不是近亲、不是
// citizen自己、不是citizen当前已有的情人。照抄老工程"最多尝试若干次随机候选"的容错写法，
// 找不到就返回nullptr。
Citizen* FindRomanticCandidate(vector<Citizen*>& citizens, Citizen* citizen, int atYear) {
	if (citizens.empty()) return nullptr;
	for (int attempt = 0; attempt < 10; attempt++) {
		Citizen* candidate = citizens[GetRandom(static_cast<int>(citizens.size()))];
		if (candidate == citizen) continue;
		if (candidate->GetGender() == citizen->GetGender()) continue;
		if (candidate->GetAge(atYear) < kDatingMinAge) continue;
		if (IsCloseRelative(citizen, candidate)) continue;
		if (IsCurrentLoverOf(citizen, candidate)) continue;
		return candidate;
	}
	return nullptr;
}

// 兄弟姐妹/配偶反推需要的模拟阶段数据(父母索引/结婚年份)，物化成Citizen后就丢弃，只能
// 在GenerateCitizens函数体内、females/males数组还在作用域内时处理，见populace.md
// "四类人际关系生成"一节。
void GenerateKinshipRelations(vector<Citizen*>& citizens, const vector<Human>& females, const vector<Human>& males) {
	// 配偶的结婚年份——human.marry在模拟阶段已经算好，这里只是转成真实年份原样带出来，
	// 不是重新计算一个结婚时间(一个人只有一个结婚年份，这次不模拟离婚/再婚)。
	unordered_map<Citizen*, int> marriageYearOf;
	for (const Human& human : females) {
		if (human.idx >= 0 && human.spouse >= 0 && static_cast<size_t>(human.spouse) < males.size() &&
			males[human.spouse].idx >= 0) {
			marriageYearOf[citizens[human.idx]] = 2000 + human.marry;
		}
	}
	for (const Human& human : males) {
		if (human.idx >= 0 && human.spouse >= 0 && static_cast<size_t>(human.spouse) < females.size() &&
			females[human.spouse].idx >= 0) {
			marriageYearOf[citizens[human.idx]] = 2000 + human.marry;
		}
	}

	for (Citizen* citizen : citizens) {
		if (Citizen* spouse = citizen->GetSpouse()) {
			if (citizen->GetName() < spouse->GetName()) { // 避免同一对处理两次
				int marriageYear = marriageYearOf[citizen];
				citizen->AddAcquaintance(spouse->GetName(), RELATIONSHIP_KINSHIP);
				RandomizeKinshipRelation(citizen, spouse->GetName());
				citizen->AddExperience(new KinshipExperience(spouse, RELATIVE_SPOUSE, marriageYear));
				spouse->AddAcquaintance(citizen->GetName(), RELATIONSHIP_KINSHIP);
				RandomizeKinshipRelation(spouse, citizen->GetName());
				spouse->AddExperience(new KinshipExperience(citizen, RELATIVE_SPOUSE, marriageYear));
			}
		}
		for (Citizen* child : citizen->GetChildren()) {
			citizen->AddAcquaintance(child->GetName(), RELATIONSHIP_KINSHIP);
			RandomizeKinshipRelation(citizen, child->GetName());
			citizen->AddExperience(new KinshipExperience(child, RELATIVE_CHILD, child->GetBirthYear()));
			child->AddAcquaintance(citizen->GetName(), RELATIONSHIP_KINSHIP);
			RandomizeKinshipRelation(child, citizen->GetName());
			child->AddExperience(new KinshipExperience(citizen, RELATIVE_PARENT, child->GetBirthYear()));
		}
	}

	// 兄弟姐妹——按(father index, mother index)对females/males临时分组，组内两两互相
	// 标记。不持久化"父母是谁"这份数据本身，这个函数结束(GenerateCitizens返回)后
	// females/males就丢弃了。
	map<pair<int, int>, vector<Citizen*>> siblingGroups;
	for (const Human& human : females) {
		if (human.idx >= 0 && human.father >= 0 && human.mother >= 0) {
			siblingGroups[{human.father, human.mother}].push_back(citizens[human.idx]);
		}
	}
	for (const Human& human : males) {
		if (human.idx >= 0 && human.father >= 0 && human.mother >= 0) {
			siblingGroups[{human.father, human.mother}].push_back(citizens[human.idx]);
		}
	}
	for (auto& [key, siblings] : siblingGroups) {
		for (size_t i = 0; i < siblings.size(); i++) {
			for (size_t j = i + 1; j < siblings.size(); j++) {
				Citizen* a = siblings[i];
				Citizen* b = siblings[j];
				int beginYear = max(a->GetBirthYear(), b->GetBirthYear());
				a->AddAcquaintance(b->GetName(), RELATIONSHIP_KINSHIP);
				RandomizeKinshipRelation(a, b->GetName());
				a->AddExperience(new KinshipExperience(b, RELATIVE_SIBLING, beginYear));
				b->AddAcquaintance(a->GetName(), RELATIONSHIP_KINSHIP);
				RandomizeKinshipRelation(b, a->GetName());
				b->AddExperience(new KinshipExperience(a, RELATIVE_SIBLING, beginYear));
			}
		}
	}
}

// 按年份逐年推进的编年模拟生成过去恋爱史，不是"按总跨度套公式一次性算出总段数再切分"——
// 每年是否开始一段新恋情按DatingHazardForAge(当时的年龄)独立判定，年纪越大频率越低。
// 时长用pow(r,4)让持续时间偏短(照抄老工程GenerateEmotions的取法)。前任也保留在
// acquaintances里——"曾经谈过恋爱"本身就是一种认识关系，不因为关系结束就删除。
void GeneratePastRelationships(vector<Citizen*>& citizens, Citizen* citizen, int boundEndYear) {
	int startYear = citizen->GetBirthYear() + kDatingMinAge;

	for (int year = startYear; year < boundEndYear; year++) {
		float hazard = DatingHazardForAge(citizen->GetAge(year));
		if (GetRandom(10000) / 10000.0f >= hazard) continue; // 这一年没有开始新恋情

		int remaining = boundEndYear - year;
		float r = GetRandom(1000) / 1000.0f;
		int duration = 1 + static_cast<int>(powf(r, 4.0f) * remaining);
		duration = min(duration, remaining);
		int begin = year;
		int end = min(begin + duration, boundEndYear);

		Citizen* partner = FindRomanticCandidate(citizens, citizen, begin);
		if (partner) {
			citizen->AddAcquaintance(partner->GetName(), RELATIONSHIP_ROMANTIC);
			RandomizeRomanticRelation(citizen, partner->GetName());
			citizen->AddExperience(new EmotionExperience(partner, begin, end));
			partner->AddAcquaintance(citizen->GetName(), RELATIONSHIP_ROMANTIC);
			RandomizeRomanticRelation(partner, citizen->GetName());
			partner->AddExperience(new EmotionExperience(citizen, begin, end));
		}
		year = end + GetRandom(2); // 恋情(或本次失败的尝试)结束后间隔一段时间，循环末尾还会+1
	}
}

// 生成一个或多个当前情人——同一个人可以同时有多个情人；已婚概率明显低于未婚，且基础
// 概率按DatingHazardForAge随年龄递减(不是常数)——年纪越大，不管已婚未婚，新恋情/婚外情
// 都应该更少见。已婚只尝试一次(不连续叠加多个婚外情人)，未婚最多连续尝试几次。
void TryAddCurrentLovers(vector<Citizen*>& citizens, Citizen* citizen, bool married, int currentYear) {
	constexpr float kMarriedMultiplier = 0.15f; // 已婚情人概率相对同龄未婚大幅降低
	constexpr int kMaxAttemptsMarried = 1;
	constexpr int kMaxAttemptsUnmarried = 3;

	float baseChance = DatingHazardForAge(citizen->GetAge(currentYear));
	float chance = married ? baseChance * kMarriedMultiplier : baseChance;
	int maxAttempts = married ? kMaxAttemptsMarried : kMaxAttemptsUnmarried;

	for (int attempt = 0; attempt < maxAttempts; attempt++) {
		if (GetRandom(10000) / 10000.0f >= chance) break; // 未命中，不再继续尝试

		int minStartYear = citizen->GetBirthYear() + kDatingMinAge;
		for (Experience* experience : citizen->GetExperiences()) {
			if (experience->GetCategory() == RELATIONSHIP_ROMANTIC && !experience->IsOngoing()) {
				minStartYear = max(minStartYear, experience->GetEndYear());
			}
		}
		if (minStartYear >= currentYear) break;
		int startYear = minStartYear + GetRandom(currentYear - minStartYear + 1);

		Citizen* partner = FindRomanticCandidate(citizens, citizen, startYear);
		if (partner) {
			citizen->AddAcquaintance(partner->GetName(), RELATIONSHIP_ROMANTIC);
			RandomizeRomanticRelation(citizen, partner->GetName());
			citizen->AddExperience(new EmotionExperience(partner, startYear));
			partner->AddAcquaintance(citizen->GetName(), RELATIONSHIP_ROMANTIC);
			RandomizeRomanticRelation(partner, citizen->GetName());
			partner->AddExperience(new EmotionExperience(citizen, startYear));
		}

		chance *= 0.5f; // 后续尝试概率递减
	}
}

} // namespace

Populace::Populace() :
	nameFactory(Registry::Get().GetNameFactory()),
	schedulerFactory(Registry::Get().GetSchedulerFactory()),
	scriptFactory(Registry::Get().GetScriptFactory()) {
}

Populace::~Populace() {
	for (Citizen* citizen : citizens) {
		delete citizen;
	}
	for (SchoolClass* schoolClass : schoolClasses) {
		delete schoolClass;
	}
	delete name;
}

void Populace::Init(int accommodation) {
	InitNames();
	// 老工程Populace::Init原公式：target = accommodation * exp(GetRandom(1000)/1000.0f - 0.5f)。
	int target = static_cast<int>(accommodation * expf(GetRandom(1000) / 1000.0f - 0.5f));
	GenerateCitizens(target);
	AssignSchedulers();
	GenerateRomanticRelations();
	GenerateEducations();
}

void Populace::InitNames() {
	// name mod dll的发现/注册不在这里做，属于`Registry`全局一次性注册的范围(见
	// Source/Core/common/registry.md)，nameFactory已经是绑定好的引用成员。

	// Name这个concept只需要唯一一个"当前生效"的取名算法，和Roadnet同一个"单选"语义
	// （不是Terrain/Zone/Building那种按GetPriority()/权重多mod叠加），这次改成和
	// Map::InitRoadnet()同一套写法——不在C++里硬编码具体mod名字，config.json的
	// "name_mods"数组列出哪个id就用哪个：
	for (const auto& [id, args] : Config::GetConceptMods("name_mods")) {
		nameFactory.SetConfig(id, true);
	}
	string activeId = nameFactory.GetName();
	if (activeId.empty()) {
		// config.json没有显式配置name_mods时，退化选第一个被发现注册的mod，避免完全没有
		// 取名算法可用，和Map::InitRoadnet()同一个容错风格。
		auto ids = nameFactory.GetRegisteredIds();
		if (!ids.empty()) {
			nameFactory.SetConfig(ids[0], true);
			activeId = ids[0];
		}
	}
	if (activeId.empty()) {
		// 没有取名算法整个游戏就没法生成任何市民，属于致命配置错误，直接抛异常，交给
		// AForeverFrameworkActor::BeginPlay()的try/catch统一处理（打日志+退出游戏）。
		THROW_EXCEPTION(RuntimeException, "No name mod available.\n");
	}
	name = new Name(&nameFactory, activeId);

	// 主线剧情.script的name_reserve字段——剧情作者显式列出脚本里会用到的姓名，在生成
	// 市民之前占位，姓名生成器之后不会再生成同名结果，见name.md"ReserveName"一节。
	for (const string& reserved : Script::GetNameReserve(Config::GetMainStoryScriptPath())) {
		name->ReserveName(reserved);
	}
}

void Populace::AssignSchedulers() {
	// 累加所有已注册Scheduler类型的权重建CDF，和Society::Init选Organization类型同一套
	// 算法（society.cpp），不照抄老工程SchedulerFactory::GetPowers()一次性返回全部map的
	// 写法——这个工程已经确立的是SchedulerFactory::GetPower(id)单个查询。
	vector<string> ids = schedulerFactory.GetRegisteredIds();
	if (ids.empty()) return; // 理论上不会发生：至少有scheduler_basic会被注册

	vector<float> weights;
	float total = 0.f;
	for (const string& id : ids) {
		float w = schedulerFactory.GetPower(id);
		weights.push_back(w);
		total += w;
	}

	for (Citizen* citizen : citizens) {
		if (!citizen) continue;
		string selected;
		if (total > 0.f) {
			float roll = (GetRandom(10000) / 10000.f) * total;
			float acc = 0.f;
			for (size_t i = 0; i < ids.size(); i++) {
				acc += weights[i];
				if (roll <= acc) { selected = ids[i]; break; }
			}
		}
		if (selected.empty()) {
			// 所有已注册类型权重都是0时的兜底：退化成均匀随机，保证人人都有Scheduler。
			selected = ids[GetRandom(static_cast<int>(ids.size()))];
		}
		citizen->SetScheduler(new Scheduler(&schedulerFactory, &scriptFactory, selected, citizen));
	}
}

const vector<Citizen*>& Populace::GetCitizens() const { return citizens; }
int Populace::GetCurrentYear() const { return currentYear; }

void Populace::Tick(const Time& currentTime, bool crossedDay,
	const function<void(Citizen*, const vector<Change*>&)>& onActions, PostHandle* post) {
	if (crossedDay) {
		for (Citizen* citizen : citizens) {
			Job* job = citizen->GetJob();
			if (!job) continue;
			job->DailyPlan(currentTime, post);
			for (const auto& [node, time] : job->GetPlans()) {
				jobTimerSet.insert({ time, citizen, node });
			}
		}
	}

	int jobCount = 0;
	while (jobCount < kMaxJobTimersPerTick && !jobTimerSet.empty()) {
		auto it = jobTimerSet.begin();
		const auto& [target, citizen, node] = *it;
		if (currentTime < target) break;
		Job* job = citizen->GetJob();
		if (job) {
			vector<Change*> changes = job->ExecNode(node, post);
			onActions(citizen, changes);
		}
		jobTimerSet.erase(it);
		jobCount++;
	}

	// Scheduler自己独立的一套timer，负责citizen下班之后的行为，和上面Job的timer完全平行——
	// 结构、上限、驱动方式都照抄，只是换成citizen->GetScheduler()，共用同一个onActions
	// 回调（回调签名本来就是通用的(Citizen*, const vector<Change*>&)，不关心Change是
	// Job产的还是Scheduler产的）。
	if (crossedDay) {
		for (Citizen* citizen : citizens) {
			Scheduler* scheduler = citizen->GetScheduler();
			if (!scheduler) continue;
			scheduler->DailyPlan(currentTime, post);
			for (const auto& [node, time] : scheduler->GetPlans()) {
				schedulerTimerSet.insert({ time, citizen, node });
			}
		}
	}

	int schedulerCount = 0;
	while (schedulerCount < kMaxSchedulerTimersPerTick && !schedulerTimerSet.empty()) {
		auto it = schedulerTimerSet.begin();
		const auto& [target, citizen, node] = *it;
		if (currentTime < target) break;
		Scheduler* scheduler = citizen->GetScheduler();
		if (scheduler) {
			vector<Change*> changes = scheduler->ExecNode(node, post);
			onActions(citizen, changes);
		}
		schedulerTimerSet.erase(it);
		schedulerCount++;
	}
}

void Populace::ApplyChange(const Change* change, const ScriptContext& context) {
	if (auto* addOption = dynamic_cast<const AddOptionChange*>(change)) {
		Citizen* target = FindCitizenByName(ToString(EvaluateExpression(addOption->GetName(), context)));
		if (target) target->AddOption(ToString(EvaluateExpression(addOption->GetOption(), context)));
		return;
	}

	// 占位，等Populace域真的有需要处理的其它Change子类时再补，见populace.h声明处注释。
}

Citizen* Populace::FindCitizenByName(const std::string& name) const {
	for (Citizen* citizen : citizens) {
		if (citizen && citizen->GetName() == name) return citizen;
	}
	return nullptr;
}

void Populace::GenerateCitizens(int target) {
	// 临时男女数组及年表，完全照抄老工程Populace::GenerateCitizens的算法(E:\Projects\
	// Forever_UE\Source\Core\populace\populace.cpp:659-859)。
	vector<Human> females(1, { -1, "", 0, 0, LIFE_DEAD, GENDER_FEMALE, -1, -1, -1, {} });
	vector<Human> males(1, { -1, "", 0, 0, LIFE_DEAD, GENDER_MALE, -1, -1, -1, {} });
	vector<int> maleBirths(4096, -1);
	int currentBirth = 0;
	vector<vector<pair<int, LIFE_TYPE>>> chronology(4096);

	// 初始添加100男100女。老工程这里两个性别都传固定的(false,true,true)——只允许
	// 女性+中性词库，男性种子也不例外，读起来像是老工程自己的一个疏漏(和几十行之后
	// LIFE_BIRTH事件里"按孩子实际性别选对应词库、中性词库永远允许"的写法不一致)。这次
	// 迁移时顺手改成和LIFE_BIRTH一致的"按性别选对应词库+中性词库永远允许"，不逐字复刻
	// 这个疑似bug。
	for (int i = 1; i <= 100; i++) {
		string n = name->GenerateName(false, true, true);
		if (n.empty()) continue;
		females.push_back({ -1, n, GetRandom(20), -1, LIFE_SINGLE, GENDER_FEMALE,
			-1, -1, -1, {} });
		int marryIdx = static_cast<int>(females.size()) - 1;
		int marryYear = females.back().birth + 20 + GetRandom(15);
		if (marryYear >= 0 && marryYear < 4096) {
			chronology[marryYear].emplace_back(marryIdx, LIFE_MARRY);
		}
		int deathYear = females.back().birth + 60 + GetRandom(40);
		if (deathYear >= 0 && deathYear < 4096) {
			chronology[deathYear].emplace_back(marryIdx, LIFE_DEAD);
		}
	}
	for (int i = 1; i <= 100; i++) {
		string n = name->GenerateName(true, false, true);
		if (n.empty()) continue;
		males.push_back({ -1, n, GetRandom(20), -1, LIFE_SINGLE, GENDER_MALE,
			-1, -1, -1, {} });
	}
	sort(males.begin() + 1, males.end(),
		[](const Human& x, const Human& y) { return x.birth < y.birth; });
	maleBirths[0] = 0;
	for (int i = 1; i <= 100; i++) {
		if (males[i].birth > currentBirth) {
			for (int j = currentBirth + 1; j <= males[i].birth && j < 4096; j++) {
				maleBirths[j] = i;
			}
			currentBirth = males[i].birth;
		}
		int deathYear = males[i].birth + 60 + GetRandom(40);
		if (deathYear >= 0 && deathYear < 4096) {
			chronology[deathYear].emplace_back(-i, LIFE_DEAD);
		}
	}

	// 自动迭代繁殖最多4096年，至少跑满100年，之后年份<4096且(存活+死亡)总人数达到target
	// 前继续，和老工程完全一致。
	int year = 1;
	while (year < 100 || (year < 4096 &&
		static_cast<int>(females.size()) + static_cast<int>(males.size()) < target)) {
		for (const auto& [eventIdx, lifeType] : chronology[year]) {
			if (eventIdx >= 0) {
				if (eventIdx >= static_cast<int>(females.size())) {
					continue;
				}
				switch (lifeType) {
				case LIFE_MARRY: {
					int lowId = max(0, females[eventIdx].birth - 10);
					int highId = max(0, females[eventIdx].birth + 5);
					if (lowId >= 4096 || highId >= 4096) break;
					if (maleBirths[lowId] == maleBirths[highId]) break;
					if (maleBirths[highId] == -1) break;
					int selectId = -1;
					for (int i = 0; i < 10; i++) {
						int candidate = maleBirths[lowId] +
							GetRandom(maleBirths[highId] - maleBirths[lowId]);
						if (candidate >= 0 && candidate < static_cast<int>(males.size()) &&
							males[candidate].spouse >= 0) {
							continue;
						}
						else {
							selectId = candidate;
							break;
						}
					}
					if (selectId < 0 || selectId >= static_cast<int>(males.size())) break;
					females[eventIdx].spouse = selectId;
					females[eventIdx].life = LIFE_MARRY;
					females[eventIdx].marry = year;
					males[selectId].spouse = eventIdx;
					males[selectId].life = LIFE_MARRY;
					males[selectId].marry = year;
					int childs = ((GetRandom(8) == 0) ? 0 : (1 + GetRandom(6)));
					int interval = 1 + GetRandom(3);
					for (int i = 0; i < childs; i++) {
						if (year + interval - females[eventIdx].birth > 45) break;
						int birthYear = year + interval;
						if (birthYear >= 0 && birthYear < 4096) {
							chronology[birthYear].emplace_back(eventIdx, LIFE_BIRTH);
						}
						interval += 1 + GetRandom(3);
					}
					break;
				}
				case LIFE_BIRTH: {
					int gender = GetRandom(2);
					// 孩子继承父亲的姓——GetSurname()按老工程约定"取姓名的第一个UTF-8
					// 字符"，父亲是females[eventIdx].spouse这个男性。
					string surname = name->GetSurname(males[females[eventIdx].spouse].name);
					string n = name->GenerateName(surname,
						gender == GENDER_MALE, gender == GENDER_FEMALE, true);
					if (n.empty()) break; // 取名失败(理论上不会发生)，这次生育事件作废
					if (gender == GENDER_FEMALE) {
						females[eventIdx].childs.emplace_back(GENDER_FEMALE,
							static_cast<int>(females.size()));
						males[females[eventIdx].spouse].childs.emplace_back(GENDER_FEMALE,
							static_cast<int>(females.size()));
						females.push_back({ -1, n, year, -1, LIFE_SINGLE, GENDER_FEMALE,
							females[eventIdx].spouse, eventIdx, -1, {} });
						int newIdx = static_cast<int>(females.size()) - 1;
						if (GetRandom(10) > 0) {
							int marryYear = females.back().birth + 20 + GetRandom(15);
							if (marryYear >= 0 && marryYear < 4096) {
								chronology[marryYear].emplace_back(newIdx, LIFE_MARRY);
							}
						}
						int deathYear = females.back().birth + 60 + GetRandom(40);
						if (deathYear >= 0 && deathYear < 4096) {
							chronology[deathYear].emplace_back(newIdx, LIFE_DEAD);
						}
					}
					else {
						females[eventIdx].childs.emplace_back(GENDER_MALE,
							static_cast<int>(males.size()));
						males[females[eventIdx].spouse].childs.emplace_back(GENDER_MALE,
							static_cast<int>(males.size()));
						males.push_back({ -1, n, year, -1, LIFE_SINGLE, GENDER_MALE,
							females[eventIdx].spouse, eventIdx, -1, {} });
						int newIdx = static_cast<int>(males.size()) - 1;
						if (males.back().birth > currentBirth) {
							for (int j = currentBirth + 1;
								j <= males.back().birth && j < 4096; j++) {
								maleBirths[j] = newIdx;
							}
							currentBirth = males.back().birth;
						}
						int deathYear = males.back().birth + 60 + GetRandom(40);
						if (deathYear >= 0 && deathYear < 4096) {
							chronology[deathYear].emplace_back(-newIdx, LIFE_DEAD);
						}
					}
					break;
				}
				case LIFE_DEAD:
					females[eventIdx].life = LIFE_DEAD;
					break;
				default:
					break;
				}
			}
			else {
				int maleIdx = -eventIdx;
				if (maleIdx >= 0 && maleIdx < static_cast<int>(males.size())) {
					switch (lifeType) {
					case LIFE_DEAD:
						males[maleIdx].life = LIFE_DEAD;
						break;
					default:
						break;
					}
				}
			}
		}
		year++;
	}
	currentYear = year + 2000;

	// 只把姓名/性别/生日+配偶/子女链接物化进Citizen——父母/兄弟姐妹等其它血缘关系只在上面
	// 的Human临时结构里用来推进人口数量，这次不保留(Citizen这次的字段范围只到配偶/子女，
	// 见citizen.h；Map::Checkin()要靠配偶/子女判断"一家人要不要搬进同一间"，见
	// populace.md)。只有模拟结束时活着(life != LIFE_DEAD)且额外通过老工程同款95%存活
	// 过滤(GetRandom(20) > 0)的个体才会真正物化成Citizen。生日年份沿用老工程"2000+模拟
	// 内部年份"的换算习惯。Human.idx记录物化后在citizens里的下标(照抄老工程Person::idx的
	// 用法)，-1表示没有幸存/物化，下面第二遍关系登记要用它把Human下标翻译成Citizen*。
	for (size_t i = 1; i < females.size(); i++) {
		if (females[i].life != LIFE_DEAD && GetRandom(20) > 0) {
			int month = 1 + GetRandom(12);
			int day = 1 + GetRandom(Time::DaysInMonth(2000 + females[i].birth, month));
			females[i].idx = static_cast<int>(citizens.size());
			citizens.push_back(new Citizen(females[i].name, GENDER_FEMALE,
				2000 + females[i].birth, month, day));
		}
	}
	for (size_t i = 1; i < males.size(); i++) {
		if (males[i].life != LIFE_DEAD && GetRandom(20) > 0) {
			int month = 1 + GetRandom(12);
			int day = 1 + GetRandom(Time::DaysInMonth(2000 + males[i].birth, month));
			males[i].idx = static_cast<int>(citizens.size());
			citizens.push_back(new Citizen(males[i].name, GENDER_MALE,
				2000 + males[i].birth, month, day));
		}
	}

	// 记录配偶/子女关系——只有配偶/子女双方都幸存物化(idx>=0)才登记，只有一方活下来的
	// 婚姻/亲子关系这次直接丢弃(和老工程"idx>=0"这一路判断的效果一致：没物化的一方本来
	// 就没有Citizen*可以指)。
	for (size_t i = 1; i < females.size(); i++) {
		if (females[i].idx < 0) continue;
		Citizen* person = citizens[females[i].idx];
		if (females[i].spouse >= 0 && static_cast<size_t>(females[i].spouse) < males.size() &&
			males[females[i].spouse].idx >= 0) {
			person->SetSpouse(citizens[males[females[i].spouse].idx]);
		}
		for (const auto& [gender, childIdx] : females[i].childs) {
			if (gender == GENDER_FEMALE) {
				if (static_cast<size_t>(childIdx) < females.size() && females[childIdx].idx >= 0)
					person->AddChild(citizens[females[childIdx].idx]);
			}
			else {
				if (static_cast<size_t>(childIdx) < males.size() && males[childIdx].idx >= 0)
					person->AddChild(citizens[males[childIdx].idx]);
			}
		}
	}
	for (size_t i = 1; i < males.size(); i++) {
		if (males[i].idx < 0) continue;
		Citizen* person = citizens[males[i].idx];
		if (males[i].spouse >= 0 && static_cast<size_t>(males[i].spouse) < females.size() &&
			females[males[i].spouse].idx >= 0) {
			person->SetSpouse(citizens[females[males[i].spouse].idx]);
		}
		for (const auto& [gender, childIdx] : males[i].childs) {
			if (gender == GENDER_FEMALE) {
				if (static_cast<size_t>(childIdx) < females.size() && females[childIdx].idx >= 0)
					person->AddChild(citizens[females[childIdx].idx]);
			}
			else {
				if (static_cast<size_t>(childIdx) < males.size() && males[childIdx].idx >= 0)
					person->AddChild(citizens[males[childIdx].idx]);
			}
		}
	}

	// 亲属关系(配偶/子女/兄弟姐妹)——需要用到上面females/males数组里的结婚年份/父母索引，
	// 这些数据物化成Citizen后就丢弃，只能在这两个数组还在作用域内、也就是这个函数体结束
	// 之前处理，见populace.md"四类人际关系生成"一节。
	GenerateKinshipRelations(citizens, females, males);
}

void Populace::GenerateRomanticRelations() {
	// 已婚：恋爱开始年份 + 更早的恋爱史 + 婚外情人
	for (Citizen* citizen : citizens) {
		Citizen* spouse = citizen->GetSpouse();
		if (!spouse || citizen->GetName() >= spouse->GetName()) continue; // 避免同一对处理两次

		// 结婚年份直接读GenerateKinshipRelations()已经写好的KinshipExperience(RELATIVE_
		// SPOUSE)，不重新计算——一个人只有一个结婚年份。
		int marriageYear = currentYear;
		for (Experience* experience : citizen->GetExperiences()) {
			if (experience->GetCategory() != RELATIONSHIP_KINSHIP) continue;
			KinshipExperience* kinship = static_cast<KinshipExperience*>(experience);
			if (kinship->GetRelativeType() == RELATIVE_SPOUSE && kinship->GetOther() == spouse) {
				marriageYear = kinship->GetBeginYear();
				break;
			}
		}
		// 防御性下限——模拟阶段的结婚年龄公式(20-34岁)本来就满足18岁以上，这里只是兜底
		// 极端情况，确保"结婚要在双方18岁之后"这条硬约束不会被打破。
		marriageYear = max(marriageYear, max(citizen->GetBirthYear(), spouse->GetBirthYear()) + kMarriageMinAge);

		int datingLowerBound = max(citizen->GetBirthYear(), spouse->GetBirthYear()) + kDatingMinAge;
		int datingStartYear = marriageYear;
		if (marriageYear > datingLowerBound) {
			datingStartYear = datingLowerBound + GetRandom(marriageYear - datingLowerBound);
		}

		citizen->AddExperience(new EmotionExperience(spouse, datingStartYear));
		spouse->AddExperience(new EmotionExperience(citizen, datingStartYear));
		RandomizeRomanticRelation(citizen, spouse->GetName());
		RandomizeRomanticRelation(spouse, citizen->GetName());

		GeneratePastRelationships(citizens, citizen, datingStartYear);
		GeneratePastRelationships(citizens, spouse, datingStartYear);

		// 婚外情人——概率明显低于未婚情形，走同一套TryAddCurrentLovers。
		TryAddCurrentLovers(citizens, citizen, /*married=*/true, currentYear);
		TryAddCurrentLovers(citizens, spouse, /*married=*/true, currentYear);
	}

	// 未婚：当前情人(可以有多个)
	for (Citizen* citizen : citizens) {
		if (citizen->GetSpouse()) continue; // 已婚在上面处理过
		TryAddCurrentLovers(citizens, citizen, /*married=*/false, currentYear);
	}

	// 未婚：更早的恋爱史，跨度截止到最早一个当前情人的开始年份(如果有)或currentYear
	for (Citizen* citizen : citizens) {
		if (citizen->GetSpouse()) continue;
		int boundEndYear = currentYear;
		for (Citizen* lover : citizen->GetCurrentLovers()) {
			for (Experience* experience : citizen->GetExperiences()) {
				if (experience->GetCategory() != RELATIONSHIP_ROMANTIC) continue;
				EmotionExperience* emotion = static_cast<EmotionExperience*>(experience);
				if (emotion->GetOther() == lover) boundEndYear = min(boundEndYear, emotion->GetBeginYear());
			}
		}
		GeneratePastRelationships(citizens, citizen, boundEndYear);
	}
}

void Populace::GenerateEducations() {
	constexpr int kElementaryStartAge = 6;
	constexpr int kMiddleStartAge = 12;
	constexpr int kUniversityStartAge = 18;
	constexpr int kElementaryDuration = kMiddleStartAge - kElementaryStartAge; // 6年
	constexpr int kMiddleDuration = kUniversityStartAge - kMiddleStartAge; // 6年
	constexpr int kUniversityDuration = 4;
	constexpr float kUniversityAttendanceRate = 0.5f;
	constexpr int kMaxClassSize = 35;

	// 每个citizen最多3个学历阶段各一条记录：{level, class}
	unordered_map<Citizen*, unordered_map<int, SchoolClass*>> enrollment;
	unordered_map<Citizen*, bool> universityRolled;

	int earliestYear = currentYear;
	for (Citizen* citizen : citizens) {
		earliestYear = min(earliestYear, citizen->GetBirthYear() + kElementaryStartAge);
	}

	auto EnrollInto = [&](Citizen* citizen, EDUCATION_LEVEL level, int year, const char* namePrefix) -> SchoolClass* {
		SchoolClass* target = nullptr;
		for (SchoolClass* schoolClass : schoolClasses) {
			if (schoolClass->GetLevel() == level && schoolClass->GetStartYear() == year &&
				static_cast<int>(schoolClass->GetStudents().size()) < kMaxClassSize) {
				target = schoolClass;
				break;
			}
		}
		if (!target) {
			int sameLevelCount = 0;
			for (SchoolClass* schoolClass : schoolClasses) {
				if (schoolClass->GetLevel() == level) sameLevelCount++;
			}
			target = new SchoolClass(namePrefix + to_string(sameLevelCount + 1), level, year);
			schoolClasses.push_back(target);
		}
		target->AddStudent(citizen);
		enrollment[citizen][static_cast<int>(level)] = target;
		return target;
	};

	// 按年份正序推进，逐年判断谁该入学/升学/是否摇号上大学——已经毕业的人自然是模拟推进
	// 到他们毕业那年就不再变化，正在上学的人自然是推进到currentYear时还卡在某个阶段中间，
	// 一套年份循环天然同时覆盖两种情况，见populace.md"四类人际关系生成"一节。
	for (int year = earliestYear; year <= currentYear; year++) {
		for (Citizen* citizen : citizens) {
			int age = citizen->GetAge(year);
			if (age == kElementaryStartAge) {
				EnrollInto(citizen, EDUCATION_ELEMENTARY, year, "第");
			}
			else if (age == kMiddleStartAge && enrollment[citizen].count(EDUCATION_ELEMENTARY)) {
				EnrollInto(citizen, EDUCATION_MIDDLE, year, "第");
			}
			else if (age == kUniversityStartAge && enrollment[citizen].count(EDUCATION_MIDDLE) &&
				!universityRolled[citizen]) {
				universityRolled[citizen] = true;
				if (GetRandom(1000) / 1000.0f < kUniversityAttendanceRate) {
					EnrollInto(citizen, EDUCATION_UNIVERSITY, year, "第");
				}
			}
		}
	}

	// 每个citizen自己的EducationExperience(一对多里"一"的那一份) + 最后一次毕业年份
	for (Citizen* citizen : citizens) {
		int lastGraduationYear = -1;
		for (const auto& [levelInt, schoolClass] : enrollment[citizen]) {
			int duration = kElementaryDuration;
			if (levelInt == EDUCATION_MIDDLE) duration = kMiddleDuration;
			else if (levelInt == EDUCATION_UNIVERSITY) duration = kUniversityDuration;
			int graduationYear = schoolClass->GetStartYear() + duration;
			citizen->AddExperience(new EducationExperience(schoolClass, schoolClass->GetStartYear(), graduationYear));
			if (graduationYear <= currentYear) {
				lastGraduationYear = max(lastGraduationYear, graduationYear);
			}
		}
		citizen->SetLastGraduationYear(lastGraduationYear);
	}

	// 同学关系——从EducationExperience派生出的一对多数据，全班互相认识。
	for (SchoolClass* schoolClass : schoolClasses) {
		const vector<Citizen*>& students = schoolClass->GetStudents();
		for (size_t i = 0; i < students.size(); i++) {
			for (size_t j = i + 1; j < students.size(); j++) {
				students[i]->AddAcquaintance(students[j]->GetName(), RELATIONSHIP_CLASSMATE);
				RandomizeClassmateRelation(students[i], students[j]->GetName());
				students[j]->AddAcquaintance(students[i]->GetName(), RELATIONSHIP_CLASSMATE);
				RandomizeClassmateRelation(students[j], students[i]->GetName());
			}
		}
	}
}
