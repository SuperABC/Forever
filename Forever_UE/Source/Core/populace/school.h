#pragma once

#include "class.h"

#include <string>
#include <vector>


// 学历阶段——小学(6-12岁)/中学(12-18岁)/大学(18-22岁，非义务，见Populace::
// GenerateEducations())。
enum EDUCATION_LEVEL : int {
	EDUCATION_ELEMENTARY,
	EDUCATION_MIDDLE,
	EDUCATION_UNIVERSITY
};

// 虚拟班级——没有Mod/Factory背书的纯Core实体，写法照抄Citizen（citizen.md："自己没有
// factory/工厂查表机制"）：由Populace::GenerateEducations()生成、Populace::schoolClasses
// 持有、~Populace()统一delete。同一个班级里的所有学生互相是"同学"关系（Populace::
// GenerateEducations()据此生成acquaintances，不靠这个类自己表达关系）。
class SchoolClass {
public:
	SchoolClass(const std::string& schoolName, EDUCATION_LEVEL level, int startYear);

	const std::string& GetSchoolName() const;
	EDUCATION_LEVEL GetLevel() const;
	int GetStartYear() const;
	const std::vector<Citizen*>& GetStudents() const;
	void AddStudent(Citizen* citizen);

private:
	std::string schoolName;
	EDUCATION_LEVEL level;
	int startYear;
	std::vector<Citizen*> students; // 不持有所有权
};
