#pragma once

#include "society/organization_mod.h"

#include <string>
#include <vector>


// ShopOrganization/FactoryOrganization共用的岗位计划调整和股本增发规则，见organization_shop.md。
int64_t DistressIssue(const OrganizationMonthlyState& state);
int PlannedCount(const std::vector<Vacancy>& vacancies, const std::string& type);
void AdjustPlan(std::vector<Vacancy>& vacancies, const OrganizationMonthlyState& state, const std::string& type,
	int steps, int minPercent, int maxPercent);
bool Distress(const OrganizationMonthlyState& state);
int HeadcountDelta(const OrganizationMonthlyState& state, int planned, int64_t wage);
