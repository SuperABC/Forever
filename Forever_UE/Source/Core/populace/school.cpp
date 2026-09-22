#include "populace/school.h"


using namespace std;

SchoolClass::SchoolClass(const string& schoolName, EDUCATION_LEVEL level, int startYear) :
	schoolName(schoolName),
	level(level),
	startYear(startYear) {
}

const string& SchoolClass::GetSchoolName() const { return schoolName; }
EDUCATION_LEVEL SchoolClass::GetLevel() const { return level; }
int SchoolClass::GetStartYear() const { return startYear; }
const vector<Citizen*>& SchoolClass::GetStudents() const { return students; }
void SchoolClass::AddStudent(Citizen* citizen) { if (citizen) students.push_back(citizen); }
