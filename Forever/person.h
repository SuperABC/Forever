#pragma once

#include "job.h"
#include "utility.h"

#include <string>
#include <memory>
#include <vector>


enum GENDER_TYPE {
	GENDER_FEMALE, // Ů��
	GENDER_MALE // ����
};

enum RELATIVE_TYPE {
	RELATIVE_FATHER, // ����
	RELATIVE_MOTHER, // ĸ��
	RELATIVE_WIFE, // ����
	RELATIVE_HUSBAND, // �ɷ�
	RELATIVE_SON, // ����
	RELATIVE_DAUGHTER // Ů��
};

class Person {
public:
	Person();
	~Person();

	// ��ȡ/���û�����Ϣ
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
	const Time& GetMarryday() const;
	void SetMarryday(const Time& marryday);
	const std::shared_ptr<Job> GetJob() const;
	void SetJob(const std::shared_ptr<Job>& job);
	const std::string& GetNick() const;
	void SetNick(const std::string& nick);
	int GetDeposit() const;
	void SetDeposit(int deposit);

	// ʵʱģ��
	int GetSimulate() const;
	void SetSimulate(int simulate);

	// ��������
	void AddRelative(RELATIVE_TYPE type, std::shared_ptr<Person> person);
	std::shared_ptr<Person> GetFather();
	std::shared_ptr<Person> GetMother();
	std::shared_ptr<Person> GetSpouse();
	std::vector<std::shared_ptr<Person>> GetChilds();

private:
	int id;
	std::string name;
	GENDER_TYPE gender;
	Time birthday;
	Time marryday;
	int height, weight;
	std::shared_ptr<Job> job;
	std::string nick;
	int deposit;

	bool simulate = true;

	std::vector<std::pair<RELATIVE_TYPE, std::shared_ptr<Person>>> relatives;

};