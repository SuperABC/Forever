#pragma once

#include "society/organization_mod.h"


// ShopOrganization：商店，这次用来测试"一个组织随机占1~2个component_shop"——
// ComponentRequirements要求1~2个"component_shop"类型的Component，DesignJobsForRoom
// 给每个"room_shop"类型的workspace room恰好配一个店员(不管这个房间WorkspaceCapacity()
// 具体是多少，workspaceCapacity==0就不配)。构造函数里设scriptModName="empty"+
// milestoneNames={"organization_shop"}，让Organization::Organization()据此创建一个挂了
// Resource/Story/organization_shop.script的Script，见organization.md"Script配置"一节。
class ShopOrganization : public OrganizationMod {
public:
	ShopOrganization();

	static const char* GetId() { return "organization_shop"; }
	virtual const char* GetType() const override { return "organization_shop"; }
	// GetName()必须全局唯一，见CONVENTIONS.md——static计数器+id，和
	// Source/Basic/map/terrain_basic.h/.cpp的OceanTerrain同一个模式。
	virtual const char* GetName() override;

	static float GetPower() { return 1.f; }

	virtual void ComponentRequirements() override;
	virtual void DesignJobsForRoom(const std::string& componentType, const std::string& componentName,
		const std::string& roomType, const std::string& roomName, int workspaceCapacity) override;

private:
	static int count;
	int id;
	std::string name;
};
