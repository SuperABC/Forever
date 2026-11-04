#pragma once

#include "industry/product_mod.h"
#include "industry/product_factory.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>


// Product：一种产品的类型目录条目——只是配方/分类定义，不是"某个仓库里的一份库存"，
// 库存数量全部放进Storage自己的存量表，见storage.h。Industry按产品type各创建一份，
// 存进自己的目录，供Storage/Manufacture查配方/分类用，见product.md。
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

	int64_t GetRetailPrice() const;  // 分
	int64_t GetImportPrice() const;
	int64_t GetExportPrice() const;
	int64_t GetWholesalePrice() const; // 城市内批发价，初始=(进口价+出口价)/2，后续由市场调控
	void SetWholesalePrice(int64_t price);
	const std::string& GetServiceType() const; // 空=暂不开放市民直接购买
	float GetServiceQuality() const;

private:
	ProductFactory* factory;
	ProductMod* mod;
	std::string type;

	std::vector<std::string> categories;
	float batchSize;
	std::unordered_map<std::string, float> ingredients;
	std::unordered_map<std::string, float> byproducts;

	int64_t retailPrice = 0;
	int64_t importPrice = 0;
	int64_t exportPrice = 0;
	int64_t wholesalePrice = 0;
	std::string serviceType;
	float serviceQuality = 0.f;
};
