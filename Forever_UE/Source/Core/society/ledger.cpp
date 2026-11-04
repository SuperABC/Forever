#include "society/ledger.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>


using namespace std;

int64_t& Ledger::Slot(const Party& party) {
	switch (party.kind) {
	case Party::GOVERNMENT: return government;
	case Party::CITIZEN: return citizens[static_cast<const Citizen*>(party.ref)];
	case Party::ORGANIZATION: return organizations[static_cast<const Organization*>(party.ref)];
	default: return outside;
	}
}

int64_t Ledger::Balance(const Party& party) const {
	switch (party.kind) {
	case Party::GOVERNMENT: return government;
	case Party::CITIZEN: {
		auto it = citizens.find(static_cast<const Citizen*>(party.ref));
		return it != citizens.end() ? it->second : 0;
	}
	case Party::ORGANIZATION: {
		auto it = organizations.find(static_cast<const Organization*>(party.ref));
		return it != organizations.end() ? it->second : 0;
	}
	default: return outside;
	}
}

double Ledger::InjectionRatio() const {
	if (initialInjection <= 0) return 1.0;
	return max(0.0, static_cast<double>(-outside) / static_cast<double>(initialInjection));
}

double Ledger::GovernmentRatio() const {
	if (initialInjection <= 0) return 0.0;
	return static_cast<double>(government) / static_cast<double>(initialInjection);
}

const char* FlowName(Flow flow) {
	switch (flow) {
	case Flow::INJECTION: return "城市外注资";
	case Flow::GOODS_INTERNAL: return "城内货款";
	case Flow::GOODS_IMPORT: return "进口货款";
	case Flow::GOODS_EXPORT: return "出口货款";
	case Flow::TARIFF: return "关税";
	case Flow::SERVICE_PURCHASE: return "买饭(服务)";
	case Flow::CONSUMPTION_TAX: return "消费税";
	case Flow::RENT: return "房租";
	case Flow::RENT_TAX: return "房租税";
	case Flow::WAGE: return "工资";
	case Flow::INCOME_TAX: return "所得税";
	case Flow::PENSION: return "养老金";
	case Flow::UNEMPLOYMENT: return "失业补助";
	case Flow::MINOR_ALLOWANCE: return "未成年补助";
	case Flow::CORPORATE_TAX: return "企业税";
	case Flow::DIVIDEND: return "分红";
	case Flow::RESTART_FUNDS: return "重启集资";
	case Flow::DEBT_WRITE_OFF: return "核销负债";
	default: return "?";
	}
}

int64_t Ledger::FlowTotal(Flow kind, Party::Kind from, Party::Kind to) const {
	return flowTotals[static_cast<size_t>(kind) * 16 + static_cast<size_t>(from) * 4 + static_cast<size_t>(to)];
}

void Ledger::Transfer(const Party& from, const Party& to, int64_t amount, Flow kind) {
	if (amount <= 0) return;
	if (from.kind == to.kind && from.ref == to.ref) return;
	flowTotals[static_cast<size_t>(kind) * 16 + static_cast<size_t>(from.kind) * 4 + static_cast<size_t>(to.kind)] += amount;
	Slot(from) -= amount;
	Slot(to) += amount;
}

int64_t Ledger::CitizensTotal() const {
	int64_t total = 0;
	for (const auto& [citizen, balance] : citizens) total += balance;
	return total;
}

int64_t Ledger::OrganizationsTotal() const {
	int64_t total = 0;
	for (const auto& [organization, balance] : organizations) total += balance;
	return total;
}

int64_t Ledger::Total() const {
	return outside + government + CitizensTotal() + OrganizationsTotal();
}

string FormatMoney(int64_t cents) {
	char buffer[64];
	int64_t absolute = cents < 0 ? -cents : cents;
	snprintf(buffer, sizeof(buffer), "%s%lld.%02lld元", cents < 0 ? "-" : "",
		static_cast<long long>(absolute / 100), static_cast<long long>(absolute % 100));
	return buffer;
}
