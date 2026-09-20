#pragma once

#include "society/organization_mod.h"


// ShopOrganization：商店，这次用来测试"一个组织随机占1~2个component_shop"——
// ComponentRequirements要求1~2个"component_shop"类型的Component，DesignJobsForRoom
// 给每个"room_shop"类型的workspace room恰好配一个店员(不管这个房间WorkspaceCapacity()
// 具体是多少，workspaceCapacity==0就不配)。
class ShopOrganization : public OrganizationMod {
public:
	static const char* GetId() { return "organization_shop"; }
	virtual const char* GetType() const override { return "organization_shop"; }
	virtual const char* GetName() override { return "ShopOrganization"; }

	static float GetPower() { return 1.f; }

	virtual void ComponentRequirements() override;
	virtual void DesignJobsForRoom(const std::string& componentType, const std::string& componentName,
		const std::string& roomType, const std::string& roomName, int workspaceCapacity) override;
};
