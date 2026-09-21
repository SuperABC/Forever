#include "populace/populace.h"

#include "common/utility.h"

#include "populace/citizen.h"
#include "populace/name.h"
#include "populace/scheduler.h"
#include "common/registry.h"
#include "society/job.h"

#include <cmath>
#include <algorithm>
#include <iterator>


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
	delete name;
}

void Populace::Init(int accommodation) {
	InitNames();
	// 老工程Populace::Init原公式：target = accommodation * exp(GetRandom(1000)/1000.0f - 0.5f)。
	int target = static_cast<int>(accommodation * expf(GetRandom(1000) / 1000.0f - 0.5f));
	GenerateCitizens(target);
	AssignSchedulers();
}

void Populace::InitNames() {
	// name mod dll的发现/注册不在这里做，属于`Registry`全局一次性注册的范围(见
	// Source/Core/common/registry.md)，nameFactory已经是绑定好的引用成员。

	// 这次固定用"chinese"(Source/Basic/populace/name_chinese.h的ChineseName)这个具体
	// 实现，不像Map::InitZones()那样把所有已注册id一视同仁——Name这个concept只需要唯一
	// 一个"当前生效"的取名算法，和ResidenceRoom/ResidenceBuilding在Building::Layout()里
	// 直接按id引用具体类型是同一个"直接耦合到当前默认内容"的做法，不是通用的enable/
	// disable机制(这个项目目前没有这套机制，config.json的"name_mods"数组只提供按id的
	// 参数字符串，不做启用过滤)。
	name = new Name(&nameFactory, "chinese");
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
	// 占位，等Populace域真的有需要处理的Change子类时再补，见populace.h声明处注释。
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
}
