#pragma once

#include <string>
#include <unordered_map>


// Manufacture(工坊)：实际生产东西的单位的类型定义。"工厂"这个中文叫法在本项目里
// 专指"mod创建/销毁管理器"(XxxFactory)，容易混淆，所以这个概念中文统一叫"工坊"，
// 类名(Manufacture)不变。
//
// 配方本身写在ProductMod身上(见product_mod.h)，工坊mod只需要声明"每天要产哪些
// 产品、各产几批"——targets的value(batchesPerDay)必须是正整数，含义是"这个产品
// 的batchSize(x)的多少倍"，不是绝对数量。支持同时配置多个目标产品(比如一个工坊
// 既产主产品又产别的东西)，Core层的Manufacture::SetProperty()会把这些targets按
// "副产物覆盖抵消+配方递归展开"算法转换成"每天净需要多少外部原料"，见
// Core/industry/manufacture.h/manufacture.md。
class ManufactureMod {
public:
	ManufactureMod() = default;
	virtual ~ManufactureMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 具体子类在这里填targets，和ProductMod/StorageMod同一个"两段式"约定。
	virtual void SetTargets() = 0;

	std::unordered_map<std::string, int> targets; // 产品type -> 每天目标产几批(正整数)
};
