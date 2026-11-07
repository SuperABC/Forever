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

// relativeType描述"对方"相对本人的身份（type说的是other的身份，不是self的），不区分
// 父/母、儿子/女儿——Citizen::GetGender()已经能查性别，没必要在枚举里再拆一份。
enum RELATIVE_TYPE : int {
	RELATIVE_SPOUSE,
	RELATIVE_PARENT,
	RELATIVE_CHILD,
	RELATIVE_SIBLING
};

// 一段经历的公共部分：类别+起止年份(年份粒度)，endYear == -1表示仍在持续。
// 一对一/一对多的区别见experience.md。
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

// 一对一：other是情人或（曾经/现在的）恋爱对象，不标记是否婚姻。
class EmotionExperience : public Experience {
public:
	EmotionExperience(Citizen* other, int beginYear, int endYear = -1);

	Citizen* GetOther() const;

private:
	Citizen* other; // 不持有所有权
};

// 一对多：指向“我自己”待过的班级，一个学历阶段只有一条。
class EducationExperience : public Experience {
public:
	EducationExperience(SchoolClass* schoolClass, int beginYear, int endYear);

	SchoolClass* GetSchoolClass() const;

private:
	SchoolClass* schoolClass; // 不持有所有权，Populace::schoolClasses持有
};

// 一对多：指向“我自己”任职的组织。
class JobExperience : public Experience {
	// 存档：SocietySaveCodec直接读写私有字段(society_save.md)。
	friend class SocietySaveCodec;
public:
	JobExperience(Organization* organization, int beginYear, int endYear = -1);

	Organization* GetOrganization() const;

private:
	Organization* organization; // 不持有所有权
};
