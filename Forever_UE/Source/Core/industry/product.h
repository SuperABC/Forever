#pragma once

#include "industry/product_mod.h"
#include "industry/product_factory.h"

#include <string>
#include <unordered_map>
#include <vector>


// Product：一种产品的类型目录条目——只是配方/分类定义，不是"某个仓库里的一份库存"
// (这是和老工程的关键区别：老工程的Product身兼"类型定义"和"库存数量"两个角色，这次
// 拆开，库存数量全部放进Storage自己的存量表，见storage.h)。Industry按产品type各创建
// 一份，存进自己的目录，供Storage/Manufacture查配方/分类用，见product.md。
class Product {
public:
	Product(ProductFactory* factory, const std::string& id);
	~Product(); // factory->DestroyProduct(mod)

	// mod为空(id没有被注册/没有在config.json"product_mods"里启用)说明创建失败，
	// 调用方应当整个丢弃这个Product，见Industry::CreateProduct。
	bool IsValid() const;

	const std::string& GetType() const;
	const std::vector<std::string>& GetCategories() const;
	float GetBatchSize() const;
	const std::unordered_map<std::string, float>& GetIngredients() const; // 每批需要的原料
	const std::unordered_map<std::string, float>& GetByproducts() const;  // 每批产生的副产品

private:
	ProductFactory* factory;
	ProductMod* mod;
	std::string type;

	std::vector<std::string> categories;
	float batchSize;
	std::unordered_map<std::string, float> ingredients;
	std::unordered_map<std::string, float> byproducts;
};
