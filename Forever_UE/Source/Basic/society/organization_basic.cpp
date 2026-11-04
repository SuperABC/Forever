#include "organization_basic.h"

#include "common/json.h"

using namespace std;

int ShopOrganization::count = 0;

ShopOrganization::ShopOrganization() : id(count++) {
	scriptModName = "script_empty";
	milestoneNames = { "organization_shop" };

	initialFunds = 20000000;         // 20万元
	shareholders = vector<float>(10, 0.1f); // 10个股东各占10%
	wage = 10000;                    // 每个员工每日100元
}

const char* ShopOrganization::GetName() {
	name = "ShopOrganization" + to_string(id);
	return name.data();
}

const unordered_map<string, pair<int, int>>& ShopOrganization::GetRequirements() {
	static const unordered_map<string, pair<int, int>> requirements = { { "component_shop", { 1, 2 } } };
	return requirements;
}

void ShopOrganization::DesignJobsForRoom(const std::string& componentType, const std::string&,
	const std::string& roomType, const std::string&, int workspaceCapacity) {
	// 按房间容纳量的一半定员。
	if (componentType != "component_shop") return;
	if (roomType == "room_shop") {
		for (int i = 0; i < workspaceCapacity / 2; i++) vacancies.push_back("job_shop_saler");
	}
	else if (roomType == "room_warehouse") {
		for (int i = 0; i < workspaceCapacity / 2; i++) vacancies.push_back("job_warehouse_keeper");
	}
}

void ShopOrganization::CollectServices(Container* script, const vector<string>& stockTypes, PostHandle* post) {
	services.clear();
	if (!script || !post) return;

	for (const string& type : stockTypes) {
		auto [exist, value] = script->GetValue("stock." + type);
		if (!exist || ToInt(value) <= 0) continue;

		JsonValue request;
		request["post"] = "product info";
		request["type"] = type;
		post->Post(request);
		const JsonValue& info = post->GetResult();
		if (info["result"].AsString() != "success") continue;

		// 服务类型为空的货物(小麦/牛肉)暂不开放给市民直接购买。
		string serviceType = info["service type"].AsString();
		if (serviceType.empty()) continue;

		ServiceOffer offer;
		offer.name = type; // 服务名只给自己看：直接用货物类型，ApplyServiceUsage时据此扣库存
		offer.type = serviceType;
		offer.unitPrice = static_cast<int64_t>(info["retail price"].AsDouble()); // 商店单价固定为零售价，不参与市场调控
		offer.quality = static_cast<float>(info["service quality"].AsDouble());
		offer.capacity = ToInt(value); // 能卖的份数就是库存
		services.push_back(offer);
	}
}

void ShopOrganization::ApplyServiceUsage(const string& name, int count, Container* script) {
	if (!script || count <= 0) return;

	auto [exist, value] = script->GetValue("stock." + name);
	if (!exist) return;
	script->SetValue("stock." + name, ValueType(max(0, ToInt(value) - count)));
}

int FactoryOrganization::count = 0;

FactoryOrganization::FactoryOrganization() : id(count++) {
	initialFunds = 20000000;                // 20万元
	shareholders = vector<float>(10, 0.1f); // 10个股东各占10%
	wage = 10000;                           // 每个员工每日100元
}

const char* FactoryOrganization::GetName() {
	name = "FactoryOrganization" + to_string(id);
	return name.data();
}

const unordered_map<string, pair<int, int>>& FactoryOrganization::GetRequirements() {
	static const unordered_map<string, pair<int, int>> requirements = { { "component_factory", { 1, 1 } } };
	return requirements;
}

void FactoryOrganization::DesignJobsForRoom(const string& componentType, const string&,
	const string& roomType, const string&, int workspaceCapacity) {
	// 工坊房间按容纳量的一半定员。
	if (componentType != "component_factory") return;
	if (roomType == "room_farm" || roomType == "room_ranch" || roomType == "room_food_factory") {
		for (int i = 0; i < workspaceCapacity / 2; i++) vacancies.push_back("job_factory_worker");
	}
}
