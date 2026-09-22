#pragma once

#include "class.h"

#include <string>


// 人际关系类别——亲属/同学/同事/情感，四种关系各自对应一个Experience派生类，见下。
enum RELATIONSHIP_CATEGORY : int {
	RELATIONSHIP_KINSHIP,
	RELATIONSHIP_CLASSMATE,
	RELATIONSHIP_COLLEAGUE,
	RELATIONSHIP_ROMANTIC
};

// relativeType描述"对方"相对本人的身份（照抄老工程Person::AddRelative(type, person)的
// 语义：type说的是other的身份，不是self的），不区分父/母、儿子/女儿——Citizen::GetGender()
// 已经能查性别，没必要在枚举里再拆一份。
enum RELATIVE_TYPE : int {
	RELATIVE_SPOUSE,
	RELATIVE_PARENT,
	RELATIVE_CHILD,
	RELATIVE_SIBLING
};

// 一段经历的公共部分——类别+起止年份。年份粒度和Citizen::GetAge一致，不引入精确到月/日
// 的时间比较；endYear == -1表示这段经历仍在持续（比如现在的婚姻/现在的情人/现在的工作，
// 没有终点）。不含"other"字段——是否有一个具体的"对方"由派生类自己决定：亲属
// (KinshipExperience)/情感(EmotionExperience)是"一对一"，各自持有一个具体的other；
// 同学(EducationExperience)/同事(JobExperience)是"一对多"，指向的是自己参与的一段经历
// （班级/组织），不指向具体某个人，见populace.md"四类人际关系生成"一节的"关键设计决策"。
class Experience {
public:
	Experience(RELATIONSHIP_CATEGORY category, int beginYear, int endYear = -1);
	virtual ~Experience() = default;

	RELATIONSHIP_CATEGORY GetCategory() const;
	int GetBeginYear() const;
	int GetEndYear() const;
	bool IsOngoing() const; // endYear == -1

private:
	RELATIONSHIP_CATEGORY category;
	int beginYear;
	int endYear;
};

// 一对一：other是具体某个亲属。
class KinshipExperience : public Experience {
public:
	KinshipExperience(Citizen* other, RELATIVE_TYPE relativeType, int beginYear);

	Citizen* GetOther() const;
	RELATIVE_TYPE GetRelativeType() const;

private:
	Citizen* other; // 不持有所有权
	RELATIVE_TYPE relativeType;
};

// 一对一：other是情人或（曾经/现在的）恋爱对象——单纯记录"和这个人处过一段感情，从哪年
// 到哪年"，不需要标记这段是不是婚姻：结婚与否、结婚年份去查KinshipExperience(RELATIVE_
// SPOUSE)就有，这里不重复表达。
class EmotionExperience : public Experience {
public:
	EmotionExperience(Citizen* other, int beginYear, int endYear = -1);

	Citizen* GetOther() const;

private:
	Citizen* other; // 不持有所有权
};

// 一对多：不指向具体某个人，指向"我自己"待过的班级——一个citizen在某个学历阶段只有这
// 一条。班上其他人的acquaintances由Populace::GenerateEducations()另外派生，不靠这条
// Experience一一列出，见"关键设计决策"。
class EducationExperience : public Experience {
public:
	EducationExperience(SchoolClass* schoolClass, int beginYear, int endYear);

	SchoolClass* GetSchoolClass() const;

private:
	SchoolClass* schoolClass; // 不持有所有权，Populace::schoolClasses持有
};

// 一对多：不指向具体某个同事，指向"我自己"任职的组织——当前工作只有一条。同事的
// acquaintances由Society::GenerateEmploymentHistory()另外派生，不靠这条Experience
// 一一列出，见"关键设计决策"。
class JobExperience : public Experience {
public:
	JobExperience(Organization* organization, int beginYear, int endYear = -1);

	Organization* GetOrganization() const;

private:
	Organization* organization; // 不持有所有权
};
