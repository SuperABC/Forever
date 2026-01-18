#pragma once

#include "asset.h"
#include "scheduler.h"
#include "job.h"
#include "experience.h"
#include "script.h"
#include "utility.h"
#include "commute.h"

#include <string>
#include <memory>
#include <vector>
#include <unordered_set>

#undef GetJob
#undef AddJob


class Script;
class Story;
class EducationExperience;
class EmotionExperience;
class JobExperience;

enum GENDER_TYPE {
	GENDER_FEMALE, // 女性
	GENDER_MALE // 男性
};

enum RELATIVE_TYPE {
	RELATIVE_FATHER, // 父亲
	RELATIVE_MOTHER, // 母亲
	RELATIVE_WIFE, // 妻子
	RELATIVE_HUSBAND, // 丈夫
	RELATIVE_SON, // 儿子
	RELATIVE_DAUGHTER // 女儿
};

class Person {
public:
	Person();
	~Person();

	// 获取/设置基础信息
	int GetId() const;
	void SetId(int id);
	const std::string& GetName() const;
	void SetName(const std::string& name);
	GENDER_TYPE GetGender() const;
	void SetGender(GENDER_TYPE gender);
	float GetHeight() const;
	void SetHeight(float height);
	float GetWeight() const;
	void SetWeight(float weight);
	const Time& GetBirthday() const;
	void SetBirthday(const Time& birthday);
	int GetAge(const Time& currentTime) const;
	const Time& GetMarryday() const;
	void SetMarryday(const Time& marryday);
	const std::string& GetNick() const;
	void SetNick(const std::string& nick);
	int GetDeposit() const;
	void SetDeposit(int deposit);
	int GetPhone() const;
	void SetPhone(int phone);

	// 实时模拟
	int GetSimulate() const;
	void SetSimulate(int simulate);

	// 管理亲属
	void AddRelative(RELATIVE_TYPE type, std::shared_ptr<Person> person);
	std::shared_ptr<Person> GetFather();
	std::shared_ptr<Person> GetMother();
	std::shared_ptr<Person> GetSpouse();
	std::vector<std::shared_ptr<Person>> GetChilds();

	// 管理资产
	void AddAsset(std::shared_ptr<Asset> asset);
	std::vector<std::shared_ptr<Asset>>& GetAssets();
	std::vector<std::shared_ptr<Asset>> GetAssets(std::string name);
	std::shared_ptr<Asset> GetAsset(std::string name);

	// 管理职业
	std::vector<std::shared_ptr<Job>> GetJobs();
	void AddJob(std::shared_ptr<Job> job);
	void RemoveJob(std::shared_ptr<Job> job);
	void SetWork(int job);
	std::shared_ptr<Job> GetWork();

	// 管理住址
	std::shared_ptr<Room> GetHome();
	void SetHome(std::shared_ptr<Room> room);
	void RemoveHome();

	// 管理调度
	std::shared_ptr<Scheduler> GetScheduler();
	void SetScheduler(std::shared_ptr<Scheduler> scheduler);

	// 管理经历
	void AddEducationExperience(EducationExperience exp);
	void AddEmotionExperience(EmotionExperience exp);
	void AddJobExperience(JobExperience exp);
	std::vector<EducationExperience>& GetEducationExperiences();
	std::vector<EmotionExperience>& GetEmotionExperiences();
	std::vector<JobExperience>& GetJobExperiences();
	void ExperienceComposition();

	// 管理剧本
	void AddScript(std::shared_ptr<Script> script);
	std::pair<std::vector<Dialog>, std::vector<std::shared_ptr<Change>>> MatchEvent(
		std::shared_ptr<Event> event, std::unique_ptr<Story>& story);
	bool AddOption(std::string option);
	bool RemoveOption(std::string option);
	std::unordered_set<std::string> GetOptions();

	// 实时状态
	void SetStatus(std::shared_ptr<Zone> zone);
	void SetStatus(std::shared_ptr<Building> building);
	void SetStatus(std::shared_ptr<Room> room);
	void SetStatus(std::shared_ptr<Room> target, std::vector<Connection> paths, Time time);
	std::shared_ptr<Plot> GetCurrentPlot();
	std::shared_ptr<Zone> GetCurrentZone();
	std::shared_ptr<Building> GetCurrentBuilding();
	std::shared_ptr<Room> GetCurrentRoom();
	Commute GetCurrentCommute();

private:
	int id;
	std::string name;
	GENDER_TYPE gender;
	Time birthday;
	Time marryday;
	float height, weight;
	std::string nick;
	int deposit;
	int phone;

	bool simulate = true;

	std::vector<std::pair<RELATIVE_TYPE, std::shared_ptr<Person>>> relatives;

	std::vector<std::shared_ptr<Asset>> assets;
	std::vector<std::shared_ptr<Job>> jobs;
	int working = -1;

	std::shared_ptr<Room> home;
	std::shared_ptr<Scheduler> scheduler;

	std::vector<EducationExperience> educationExperiences;
	std::vector<EmotionExperience> emotionExperiences;
	std::vector<JobExperience> jobExperiences;

	std::vector<std::shared_ptr<Script>> scripts;
	std::unordered_set<std::string> options;

	std::shared_ptr<Plot> currentPlot;
	std::shared_ptr<Zone> currentZone;
	std::shared_ptr<Building> currentBuilding;
	std::shared_ptr<Room> currentRoom;
	Commute commute;
};