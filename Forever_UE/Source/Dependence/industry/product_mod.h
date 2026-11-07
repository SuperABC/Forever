#pragma once

#include "common/mod_state.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>


// 设计见product_mod.md。
class ProductMod {
public:
	ProductMod() = default;
	virtual ~ProductMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 子类在这里填下面的字段(不在构造函数里),Core创建实例后立刻调一次。
	virtual void SetProperty() = 0;

	std::vector<std::string> categories;                        // 分类标签，任一交集即可入库
	float batchSize = 1.f;                                       // 一批最少产多少份(x)
	std::unordered_map<std::string, float> ingredients;          // 每批需要的原料：type->数量
	std::unordered_map<std::string, float> byproducts;           // 每批产生的副产品：type->数量

	// 价格一律int64、单位是分,要求 retailPrice > importPrice > exportPrice > 0,不满足的在Core::Product构造时判无效。
	int64_t retailPrice = 0;                                     // 零售价：商店卖给市民的单价
	int64_t importPrice = 0;                                     // 进口价：从城市外买入的单价
	int64_t exportPrice = 0;                                     // 出口价：卖给城市外的单价

	// 市民可直接消费时的服务类型和质量;serviceType为空=暂不开放直接购买。
	std::string serviceType;
	float serviceQuality = 0.f;

	// ---- 存档(见common/mod_state.md) ----
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	virtual int GetStateVersion() const { return 0; }
	// GetName用的static count,存档保存/读档恢复;没有计数器的类保持默认。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
