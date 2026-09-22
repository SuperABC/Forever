#include "populace/experience.h"


Experience::Experience(RELATIONSHIP_CATEGORY category, int beginYear, int endYear) :
	category(category),
	beginYear(beginYear),
	endYear(endYear) {
}

RELATIONSHIP_CATEGORY Experience::GetCategory() const { return category; }
int Experience::GetBeginYear() const { return beginYear; }
int Experience::GetEndYear() const { return endYear; }
bool Experience::IsOngoing() const { return endYear == -1; }

KinshipExperience::KinshipExperience(Citizen* other, RELATIVE_TYPE relativeType, int beginYear) :
	Experience(RELATIONSHIP_KINSHIP, beginYear),
	other(other),
	relativeType(relativeType) {
}

Citizen* KinshipExperience::GetOther() const { return other; }
RELATIVE_TYPE KinshipExperience::GetRelativeType() const { return relativeType; }

EmotionExperience::EmotionExperience(Citizen* other, int beginYear, int endYear) :
	Experience(RELATIONSHIP_ROMANTIC, beginYear, endYear),
	other(other) {
}

Citizen* EmotionExperience::GetOther() const { return other; }

EducationExperience::EducationExperience(SchoolClass* schoolClass, int beginYear, int endYear) :
	Experience(RELATIONSHIP_CLASSMATE, beginYear, endYear),
	schoolClass(schoolClass) {
}

SchoolClass* EducationExperience::GetSchoolClass() const { return schoolClass; }

JobExperience::JobExperience(Organization* organization, int beginYear, int endYear) :
	Experience(RELATIONSHIP_COLLEAGUE, beginYear, endYear),
	organization(organization) {
}

Organization* JobExperience::GetOrganization() const { return organization; }
