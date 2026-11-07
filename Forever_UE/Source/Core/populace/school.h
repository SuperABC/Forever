#pragma once

#include "class.h"

#include <string>
#include <vector>


// 学历阶段——小学(6-12岁)/中学(12-18岁)/大学(18-22岁，非义务)。
enum EDUCATION_LEVEL : int {
	EDUCATION_ELEMENTARY,
	EDUCATION_MIDDLE,
	EDUCATION_UNIVERSITY
};

// 虚拟班级，见school.md。
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
