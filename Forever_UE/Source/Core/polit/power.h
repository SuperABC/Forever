#pragma once

#include <string>
#include <vector>


// 权力词汇里的一项。policy非空=这项权力关联某个可调政策(如"征收企业税"关联corporate_tax)。
struct PowerDef {
	std::string id;
	std::string name;
	std::string policy;
};

// Core动态提供的权力词汇表：党派mod通过AssignPowers把这里的权力分给党员。
// 以后想加新权力(如"发放游戏版号")只要Register一条。权力没有任何内置效果：城市运转不依赖职位。
class PowerCatalog {
public:
	// 注册9个内置权力(每个可调政策一个)。重复调用幂等。
	void RegisterDefaults();

	// 同id已存在则覆盖。
	void Register(const PowerDef& def);

	const std::vector<PowerDef>& All() const { return defs; }
	const PowerDef* Find(const std::string& id) const;

private:
	std::vector<PowerDef> defs;
};
