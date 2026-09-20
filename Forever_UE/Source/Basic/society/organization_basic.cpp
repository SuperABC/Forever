#include "organization_basic.h"

void ShopOrganization::ComponentRequirements() {
	requirements.clear();
	requirements["component_shop"] = { 1, 2 };
}

void ShopOrganization::DesignJobsForRoom(const std::string& componentType, const std::string&,
	const std::string& roomType, const std::string&, int workspaceCapacity) {
	if (componentType == "component_shop" && roomType == "room_shop" && workspaceCapacity > 0) {
		vacancies.push_back("job_shop_saler");
	}
}
