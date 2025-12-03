#pragma once

#include "asset.h"
#include "job.h"
#include "utility.h"

#include <string>
#include <memory>
#include <vector>


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
	int GetHeight() const;
	void SetHeight(int height);
	int GetWeight() const;
	void SetWeight(int weight);
	const Time& GetBirthday() const;
	void SetBirthday(const Time& birthday);
	int GetAge(const Time& currentTime) const;
	const Time& GetMarryday() const;
	void SetMarryday(const Time& marryday);
	const std::string& GetNick() const;
	void SetNick(const std::string& nick);
	int GetDeposit() const;
	void SetDeposit(int deposit);

	// 实时模拟
	int GetSimulate() const;
	void SetSimulate(int simulate);

	// 管理亲属
	void AddRelative(RELATIVE_TYPE type, std::shared_ptr<Person> person);
	std::shared_ptr<Person> GetFather();
	std::shared_ptr<Person> GetMother();
	std::shared_ptr<Person> GetSpouse();
	std::vector<std::shared_ptr<Person>> GetChilds();

	//管理资产
	void AddAsset(std::shared_ptr<Asset> asset);
	std::vector<std::shared_ptr<Asset>>& GetAssets();
	std::vector<std::shared_ptr<Asset>> GetAssets(std::string name);
	std::shared_ptr<Asset> GetAsset(std::string name);

private:
	int id;
	std::string name;
	GENDER_TYPE gender;
	Time birthday;
	Time marryday;
	int height, weight;
	std::string nick;
	int deposit;

	bool simulate = true;

	std::vector<std::pair<RELATIVE_TYPE, std::shared_ptr<Person>>> relatives;

	std::vector<std::shared_ptr<Asset>> assets;
	std::vector<std::shared_ptr<Job>> jobs;
};