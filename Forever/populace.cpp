#include "utility.h"
#include "error.h"
#include "populace.h"
#include "json.h"

#include <fstream>
#include <filesystem>


using namespace std;

Populace::Populace() {
    jobFactory.reset(new JobFactory());
	nameFactory.reset(new NameFactory());
}

Populace::~Populace() {
    for (auto& mod : modHandles) {
        if (mod) {
            FreeLibrary(mod);
        }
    }
    modHandles.clear();
}

void Populace::InitJobs() {
    jobFactory->RegisterJob(TestJob::GetId(), []() { return make_unique<TestJob>(); });

    HMODULE modHandle = LoadLibraryA(REPLACE_PATH("Mod.dll"));
    if (modHandle) {
        modHandles.push_back(modHandle);
        debugf("Mod dll loaded successfully.\n");

        RegisterModJobsFunc registerFunc = (RegisterModJobsFunc)GetProcAddress(modHandle, "RegisterModJobs");
        if (registerFunc) {
            registerFunc(jobFactory.get());
        }
        else {
            debugf("Incorrect dll content.");
        }
    }
    else {
        debugf("Failed to load mod.dll.");
    }

#ifdef MOD_TEST
    auto jobList = { "test", "mod" };
    for (const auto& jobId : jobList) {
        if (jobFactory->CheckRegistered(jobId)) {
            auto job = jobFactory->CreateJob(jobId);
            debugf(("Created job: " + job->GetName() + " (ID: " + jobId + ")\n").data());
        }
        else {
            debugf("Job not registered: %s\n", jobId);
        }
    }
#endif // MOD_TEST

}

void Populace::InitNames() {
	nameFactory->RegisterName(TestName::GetId(), []() { return make_unique<TestName>(); });

	HMODULE modHandle = LoadLibraryA(REPLACE_PATH("Mod.dll"));
	if (modHandle) {
		modHandles.push_back(modHandle);
		debugf("Mod dll loaded successfully.\n");

		RegisterModNamesFunc registerFunc = (RegisterModNamesFunc)GetProcAddress(modHandle, "RegisterModNames");
		if (registerFunc) {
			registerFunc(nameFactory.get());
		}
		else {
			debugf("Incorrect dll content.");
		}
	}
	else {
		debugf("Failed to load mod.dll.");
	}

#ifdef MOD_TEST
	auto nameList = { "test", "mod" };
	for (const auto& nameId : nameList) {
		if (nameFactory->CheckRegistered(nameId)) {
			auto name = nameFactory->CreateName(nameId);
			debugf(("Created name: " + name->GetName() + " (ID: " + nameId + ")\n").data());
		}
		else {
			debugf("Name not registered: %s\n", nameId);
		}
	}
#endif // MOD_TEST

}

void Populace::ReadConfigs(string path) const {
    if (!filesystem::exists(path)) {
        THROW_EXCEPTION(IOException, "Path does not exist: " + path + ".\n");
    }

    Json::Reader reader;
    Json::Value root;

    ifstream fin(path);
    if (!fin.is_open()) {
        THROW_EXCEPTION(IOException, "Failed to open file: " + path + ".\n");
    }
    if (reader.parse(fin, root)) {
		for (auto job : root["mods"]["job"]) {
			jobFactory->SetConfig(job.asString(), true);
		}
		nameFactory->SetConfig(root["mods"]["name"].asString(), true);
    }
    else {
        fin.close();
        THROW_EXCEPTION(JsonFormatException, "Json syntax error: " + reader.getFormattedErrorMessages() + ".\n");
    }
    fin.close();
}

void Populace::Init(int accomodation) {
    // 生成市民
    GenerateCitizens((int)(accomodation * exp(GetRandom(1000) / 1000.0f - 0.5f)));
}

void Populace::Destroy() {

}

void Populace::Tick() {

}

void Populace::Print() {

}

void Populace::ApplyChange(shared_ptr<Change> change, std::unique_ptr<Script>& script) {
	switch (change->GetType()) {
	case CHANGE_SET_VALUE:
		break;
	case CHANGE_REMOVE_VALUE:
		break;
	default:
		break;
	}
}

void Populace::Load(string path) {

}

void Populace::Save(string path) {

}

Time Populace::GetTime() {
	return time;
}

std::vector<std::shared_ptr<Person>>& Populace::GetCitizens() {
	return citizens;
}

void Populace::GenerateCitizens(int num) {
	enum LIFE_TYPE {
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

	// 分配姓名
	auto name = nameFactory->GetName();
	if (!name) {
		THROW_EXCEPTION(InvalidConfigException, "No enabled name in config.\n");
	}

	// 临时男女数组及年表
	vector<Human> females(1, { -1, "", 0, 0, LIFE_DEAD });
	vector<Human> males(1, { -1, "", 0, 0, LIFE_DEAD });
	vector<int> maleBirths(4096, -1);
	int currentBirth = 0;
	vector<vector<pair<int, LIFE_TYPE>>> chronology(4096);

	// 初始添加100男100女
	int year = 1;
	for (int i = 1; i <= 100; i++) {
		auto n = name->GenerateName(false, true);
		females.push_back({ -1, n, GetRandom(20), -1, LIFE_SINGLE, GENDER_FEMALE, -1, -1, -1, {} });
		chronology[females.back().birth + 20 + GetRandom(15)].push_back(make_pair((int)females.size() - 1, LIFE_MARRY));
		chronology[females.back().birth + 60 + GetRandom(40)].push_back(make_pair((int)females.size() - 1, LIFE_DEAD));
	}
	for (int i = 1; i <= 100; i++) {
		auto n = name->GenerateName(true, false);
		males.push_back({ -1, n, GetRandom(20), -1, LIFE_SINGLE, GENDER_MALE, -1, -1, -1, {} });
	}
	sort(males.begin(), males.end(), [](Human x, Human y) {return x.birth < y.birth; });
	maleBirths[0] = 0;
	for (int i = 1; i <= 100; i++) {
		if (males[i].birth > currentBirth) {
			for (int j = currentBirth + 1; j <= males[i].birth; j++) {
				maleBirths[j] = i;
			}
			currentBirth = males[i].birth;
		}
		chronology[males[i].birth + 60 + GetRandom(40)].push_back(make_pair(-i, LIFE_DEAD));
	}

	// 自动迭代繁殖最多4096年
	while (year < 4096 && females.size() + males.size() < num) {
		for (auto event : chronology[year]) {
			if (event.first >= 0) {
				switch (event.second) {
				case LIFE_MARRY: {
					int lowId = max(0, females[event.first].birth - 10);
					int highId = max(0, females[event.first].birth + 5);
					if (maleBirths[lowId] == maleBirths[highId])break;
					if (maleBirths[highId] == -1)break;
					int selectId = -1;
					for (int i = 0; i < 10; i++) {
						selectId = maleBirths[lowId] + GetRandom(maleBirths[highId] - maleBirths[lowId]);
						if (males[selectId].spouse >= 0) {
							selectId = -1;
							continue;
						}
						else {
							break;
						}
					}
					if (selectId < 0)break;
					females[event.first].spouse = selectId;
					females[event.first].life = LIFE_MARRY;
					females[event.first].marry = year;
					males[selectId].spouse = event.first;
					males[selectId].life = LIFE_MARRY;
					males[selectId].marry = year;
					int childs = ((GetRandom(8) == 0) ? 0 : (1 + GetRandom(6)));
					int interval = 1 + GetRandom(3);
					for (int i = 0; i < childs; i++) {
						if (year + interval - females[event.first].birth > 45)break;
						chronology[year + interval].push_back(make_pair(event.first, LIFE_BIRTH));
						interval += 1 + GetRandom(3);
					}
					break;
				}
				case LIFE_BIRTH: {
					int gender = GetRandom(2);
					auto n = name->GenerateName(name->GetSurname(males[females[event.first].spouse].name),
						gender == GENDER_MALE, gender == GENDER_FEMALE);
					if (gender == GENDER_FEMALE) {
						females[event.first].childs.push_back(make_pair(GENDER_FEMALE, (int)females.size()));
						males[females[event.first].spouse].childs.push_back(make_pair(GENDER_FEMALE, (int)females.size()));
						females.push_back({ -1, n, year, -1, LIFE_SINGLE, GENDER_FEMALE, females[event.first].spouse, event.first, -1, {} });
						if (GetRandom(10) > 0)
							chronology[females.back().birth + 20 + GetRandom(15)].push_back(make_pair((int)females.size() - 1, LIFE_MARRY));
						chronology[females.back().birth + 60 + GetRandom(40)].push_back(make_pair((int)females.size() - 1, LIFE_DEAD));
					}
					else {
						females[event.first].childs.push_back(make_pair(GENDER_MALE, (int)males.size()));
						males[females[event.first].spouse].childs.push_back(make_pair(GENDER_MALE, (int)males.size()));
						males.push_back({ -1, n, year, -1, LIFE_SINGLE, GENDER_MALE, females[event.first].spouse, event.first, -1, {} });
						if (males.back().birth > currentBirth) {
							for (int j = currentBirth + 1; j <= males.back().birth; j++) {
								maleBirths[j] = (int)males.size() - 1;
							}
							currentBirth = males.back().birth;
						}
						chronology[males.back().birth + 60 + GetRandom(40)].push_back(make_pair(-((int)males.size() - 1), LIFE_DEAD));
					}
					break;
				}
				case LIFE_DEAD:
					females[event.first].life = LIFE_DEAD;
					break;
				default:
					break;
				}
			}
			else {
				switch (event.second) {
				case LIFE_DEAD:
					males[-event.first].life = LIFE_DEAD;
					break;
				default:
					break;
				}
			}
		}
		year++;
	}
	time.SetYear(year + 2000);

	// 将活着的男女加入市民列表
	for (int i = 1; i < females.size(); i++) {
		if (females[i].life != LIFE_DEAD && GetRandom(20) > 0) {
			shared_ptr<Person> person = make_shared<Person>();
			person->SetId((int)citizens.size());
			females[i].idx = (int)citizens.size();
			person->SetName(females[i].name);
			person->SetGender(GENDER_FEMALE);
			int month = 1 + GetRandom(12);
			person->SetBirthday({ 2000 + females[i].birth, month, 1 + GetRandom(Time::DaysInMonth(2000 + females[i].birth, month)) });
			citizens.push_back(person);
		}
	}
	for (int i = 1; i < males.size(); i++) {
		if (males[i].life != LIFE_DEAD && GetRandom(20) > 0) {
			shared_ptr<Person> person = make_shared<Person>();
			person->SetId((int)citizens.size());
			males[i].idx = (int)citizens.size();
			person->SetName(males[i].name);
			person->SetGender(GENDER_MALE);
			int month = 1 + GetRandom(12);
			person->SetBirthday({ 2000 + males[i].birth, month, 1 + GetRandom(Time::DaysInMonth(2000 + males[i].birth, month)) });
			citizens.push_back(person);
		}
	}

	// 记录有配偶的市民的结婚日期
	for (int i = 1; i < females.size(); i++) {
		if (females[i].spouse >= 0 && females[i].idx >= 0 && males[females[i].spouse].idx >= 0) {
			int month = GetRandom(12) + 1;
			Time marryday = Time(2000 + females[i].marry, month, 1 + GetRandom(Time::DaysInMonth(2000 + females[i].marry, month)));
			citizens[females[i].idx]->SetMarryday(marryday);
			citizens[males[females[i].spouse].idx]->SetMarryday(marryday);
		}
	}

	// 记录亲属关系
	for (auto female : females) {
		if (female.idx >= 0) {
			shared_ptr<Person> person = citizens[female.idx];
			if (female.mother >= 0 && females[female.mother].idx >= 0)
				person->AddRelative(RELATIVE_MOTHER, citizens[females[female.mother].idx]);
			if (female.father >= 0 && males[female.father].idx >= 0)
				person->AddRelative(RELATIVE_FATHER, citizens[males[female.father].idx]);
			if (female.spouse >= 0 && males[female.spouse].idx >= 0) {
				person->AddRelative(RELATIVE_HUSBAND, citizens[males[female.spouse].idx]);
			}
			for (auto child : female.childs) {
				if (child.first == GENDER_FEMALE && females[child.second].idx >= 0)
					person->AddRelative(RELATIVE_DAUGHTER, citizens[females[child.second].idx]);
				else if (child.first == GENDER_MALE && males[child.second].idx >= 0)
					person->AddRelative(RELATIVE_SON, citizens[males[child.second].idx]);
			}
		}
	}
	for (auto male : males) {
		if (male.idx >= 0) {
			shared_ptr<Person> person = citizens[male.idx];
			if (male.mother >= 0 && females[male.mother].idx >= 0)
				person->AddRelative(RELATIVE_MOTHER, citizens[females[male.mother].idx]);
			if (male.father >= 0 && males[male.father].idx >= 0)
				person->AddRelative(RELATIVE_FATHER, citizens[males[male.father].idx]);
			if (male.spouse >= 0 && females[male.spouse].idx >= 0) {
				person->AddRelative(RELATIVE_WIFE, citizens[females[male.spouse].idx]);
			}
			for (auto child : male.childs) {
				if (child.first == GENDER_FEMALE && females[child.second].idx >= 0)
					person->AddRelative(RELATIVE_DAUGHTER, citizens[females[child.second].idx]);
				else if (child.first == GENDER_MALE && males[child.second].idx >= 0)
					person->AddRelative(RELATIVE_SON, citizens[males[child.second].idx]);
			}
		}
	}

	debugf("Generate %d citizens.\n", citizens.size());
}
