#include "industry/product.h"

using namespace std;

Product::Product(ProductFactory* factory, const string& id) :
	factory(factory), batchSize(1.f) {
	mod = factory->CreateProduct(id);
	if (!mod) return;

	type = mod->GetType();
	mod->SetProperty();
	categories = mod->categories;
	batchSize = mod->batchSize;
	ingredients = mod->ingredients;
	byproducts = mod->byproducts;
	retailPrice = mod->retailPrice;
	importPrice = mod->importPrice;
	exportPrice = mod->exportPrice;
	serviceType = mod->serviceType;
	serviceQuality = mod->serviceQuality;
	wholesalePrice = (importPrice + exportPrice) / 2;

	// 价格必须满足 零售价 > 进口价 > 出口价 > 0，否则整个product判无效(mod置空，IsValid()为false)。
	if (!(retailPrice > importPrice && importPrice > exportPrice && exportPrice > 0)) {
		factory->DestroyProduct(mod);
		mod = nullptr;
	}
}

Product::~Product() {
	if (mod) factory->DestroyProduct(mod);
}

bool Product::IsValid() const { return mod != nullptr; }
int64_t Product::GetRetailPrice() const { return retailPrice; }
int64_t Product::GetImportPrice() const { return importPrice; }
int64_t Product::GetExportPrice() const { return exportPrice; }
int64_t Product::GetWholesalePrice() const { return wholesalePrice; }
void Product::SetWholesalePrice(int64_t price) { wholesalePrice = price; }
const string& Product::GetServiceType() const { return serviceType; }
float Product::GetServiceQuality() const { return serviceQuality; }

const string& Product::GetType() const { return type; }
const vector<string>& Product::GetCategories() const { return categories; }
float Product::GetBatchSize() const { return batchSize; }
const unordered_map<string, float>& Product::GetIngredients() const { return ingredients; }
const unordered_map<string, float>& Product::GetByproducts() const { return byproducts; }
