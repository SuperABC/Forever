#include "organization_basic.h"

using namespace std;

int ShopOrganization::count = 0;

ShopOrganization::ShopOrganization() : id(count++) {
	scriptModName = "empty";
	milestoneNames = { "organization_shop" };
}

const char* ShopOrganization::GetName() {
	name = "ShopOrganization" + to_string(id);
	return name.data();
}

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
