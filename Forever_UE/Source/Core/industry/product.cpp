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
}

Product::~Product() {
	if (mod) factory->DestroyProduct(mod);
}

bool Product::IsValid() const { return mod != nullptr; }

const string& Product::GetType() const { return type; }
const vector<string>& Product::GetCategories() const { return categories; }
float Product::GetBatchSize() const { return batchSize; }
const unordered_map<string, float>& Product::GetIngredients() const { return ingredients; }
const unordered_map<string, float>& Product::GetByproducts() const { return byproducts; }
