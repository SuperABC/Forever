#pragma once

#include <string>
#include <unordered_map>
#include <vector>


// Product：一种产品的类型定义(不是某个仓库里的一份库存，库存数据在Storage自己的
// 存量表里，见storage_mod.h)——分类标签(categories)决定哪些仓库能存它，配方
// (ingredients/byproducts)是"批次配方"：batchSize(x)是"一批最少产多少份"，
// ingredients/byproducts的value含义是"生产一批(x份)需要/产生多少"，不是"每1份"。
// x不要求是整数，但工坊(Manufacture)每天配置的产量必须是batchesPerDay(正整数，
// 见manufacture_mod.h)，即批次数量整，每批的份数(batchSize)不要求是整数。
class ProductMod {
public:
	ProductMod() = default;
	virtual ~ProductMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 具体子类构造函数/SetProperty()里填这几个字段——和VehicleMod::blueprintPath
	// 同一个"mod自己的字段，Core只读转发"模式。SetProperty()是纯虚方法而不是构造函数
	// 直接赋值，只是为了和Storage/ManufactureMod保持同一套"构造+SetProperty两段式"
	// 的约定(照抄老工程的写法)，这次没有依赖它去做"运行时才能确定的初始化"，具体子类
	// 可以直接在构造函数里赋好这几个字段、SetProperty()留空也可以。
	virtual void SetProperty() = 0;

	std::vector<std::string> categories;                        // 分类标签，任一交集即可入库
	float batchSize = 1.f;                                       // 一批最少产多少份(x)
	std::unordered_map<std::string, float> ingredients;          // 每批需要的原料：type->数量
	std::unordered_map<std::string, float> byproducts;           // 每批产生的副产品：type->数量
};
