#include "person.h"


using namespace std;

Person::Person() {

}

Person::~Person() {

}

int Person::GetId() const {
	return id;
}

void Person::SetId(int id) {
	this->id = id;
}

const string& Person::GetName() const {
	return name;
}

void Person::SetName(const string& name) {
	this->name = name;
}

GENDER_TYPE Person::GetGender() const {
	return gender;
}

void Person::SetGender(GENDER_TYPE gender) {
	this->gender = gender;
}

float Person::GetHeight() const {
	return height;
}

void Person::SetHeight(float height) {
	this->height = height;
}

float Person::GetWeight() const {
	return weight;
}

void Person::SetWeight(float weight) {
	this->weight = weight;
}

const Time& Person::GetBirthday() const {
	return birthday;
}

void Person::SetBirthday(const Time& birthday) {
	this->birthday = birthday;
}

int Person::GetAge(const Time& currentTime) const {
	int age = currentTime.GetYear() - birthday.GetYear();
	if (currentTime.GetMonth() < birthday.GetMonth() ||
		(currentTime.GetMonth() == birthday.GetMonth() && currentTime.GetDay() < birthday.GetDay())) {
		age--;
	}
	return age;
}

const Time& Person::GetMarryday() const {
	return marryday;
}

void Person::SetMarryday(const Time& marryday) {
	this->marryday = marryday;
}

const string& Person::GetNick() const {
	return nick;
}

void Person::SetNick(const string& nick) {
	this->nick = nick;
}

int Person::GetDeposit() const {
	return deposit;
}

void Person::SetDeposit(int deposit) {
	this->deposit = deposit;
}

int Person::GetPhone() const {
	return phone;
}

void Person::SetPhone(int phone) {
	this->phone = phone;
}

int Person::GetSimulate() const {
	return simulate;
}

void Person::SetSimulate(int simulate) {
	this->simulate = simulate;
}

void Person::AddRelative(RELATIVE_TYPE type, shared_ptr<Person> person) {
	relatives.push_back(make_pair(type, person));
}

shared_ptr<Person> Person::GetFather() {
	for (auto relative : relatives) {
		if (relative.first == RELATIVE_FATHER)return relative.second;
	}
	return nullptr;
}

shared_ptr<Person> Person::GetMother() {
	for (auto relative : relatives) {
		if (relative.first == RELATIVE_MOTHER)return relative.second;
	}
	return nullptr;
}

shared_ptr<Person> Person::GetSpouse() {
	for (auto relative : relatives) {
		if (relative.first == RELATIVE_WIFE || relative.first == RELATIVE_HUSBAND)return relative.second;
	}

	return nullptr;
}

vector<shared_ptr<Person>> Person::GetChilds() {
	vector<shared_ptr<Person>>childs;
	for (auto relative : relatives) {
		if (relative.first == RELATIVE_SON || relative.first == RELATIVE_DAUGHTER)childs.push_back(relative.second);
	}
	return childs;
}

void Person::AddAsset(std::shared_ptr<Asset> asset) {
	assets.push_back(asset);
}

std::vector<std::shared_ptr<Asset>>& Person::GetAssets() {
	return assets;
}

std::vector<std::shared_ptr<Asset>> Person::GetAssets(std::string type) {
	std::vector<std::shared_ptr<Asset>> results;

	for (auto& asset : assets) {
		if (asset->GetType() == type) {
			results.push_back(asset);
		}
	}

	return results;
}

std::shared_ptr<Asset> Person::GetAsset(std::string name) {
	for (auto& asset : assets) {
		if (asset->GetName() == name) {
			return asset;
		}
	}

	return nullptr;
}

vector<shared_ptr<Job>> Person::GetJobs() {
	return jobs;
}

void Person::AddJob(shared_ptr<Job> job) {
	jobs.push_back(job);
}

void Person::RemoveJob(std::shared_ptr<Job> job) {
	for (auto &j : jobs) {
		if (j == job) {
			j = *jobs.end();
			jobs.pop_back();
			break;
		}
	}
}

shared_ptr<Room> Person::GetHome() {
	return home;
}

void Person::SetHome(std::shared_ptr<Room> room) {
	home = room;
}

void Person::RemoveHome() {
	home = nullptr;
}

shared_ptr<Scheduler> Person::GetScheduler() {
	return scheduler;
}

void Person::SetScheduler(std::shared_ptr<Scheduler> scheduler) {
	this->scheduler = scheduler;
}

void Person::AddEducationExperience(EducationExperience exp) {
	educationExperiences.push_back(exp);
}

void Person::AddEmotionExperience(EmotionExperience exp) {
	emotionExperiences.push_back(exp);
}

void Person::AddJobExperience(JobExperience exp) {
	jobExperiences.push_back(exp);
}

vector<EducationExperience>& Person::GetEducationExperiences() {
	return educationExperiences;
}

vector<EmotionExperience>& Person::GetEmotionExperiences() {
	return emotionExperiences;
}

vector<JobExperience>& Person::GetJobExperiences() {
	return jobExperiences;
}

void Person::ExperienceComposition() {
	if (educationExperiences.size() == 0)return;

	int idx = 0;
	EducationExperience education = educationExperiences[0];
	for (int i = 1; i < educationExperiences.size(); i++) {
		if (education.GetSchool() == educationExperiences[i].GetSchool() &&
			education.GetEndTime().GetYear() == educationExperiences[i].GetBeginTime().GetYear()) {
			education.SetTime(education.GetBeginTime(), educationExperiences[i].GetEndTime());
			education.SetGraduate(educationExperiences[i].GetGraduate());
		}
		else {
			educationExperiences[idx++] = education;
			education = educationExperiences[i];
		}
	}
	educationExperiences[idx++] = education;
	educationExperiences.resize(idx);
}

void Person::AddScript(string path, unique_ptr<Story>& story) {
	scripts.push_back(make_shared<Script>());
	scripts.back()->ReadScript(path,
		story->GetEventFactory(), story->GetChangeFactory());
}

pair<vector<Dialog>, vector<shared_ptr<Change>>> Person::MatchEvent(
	shared_ptr<Event> event, unique_ptr<Story>& story) {
	for (auto& script : scripts) {
		auto result = script->MatchEvent(event, story.get());
		if (!result.first.empty() || !result.second.empty()) {
			return result;
		}
	}
	return make_pair(vector<Dialog>(), vector<shared_ptr<Change>>());
}
