#include "asset_basic.h"

using namespace std;

int AssetBasic::count = 0;

AssetBasic::AssetBasic() : id(count++) {
}

const char* AssetBasic::GetName() {
	name = "AssetBasic" + to_string(id);
	return name.data();
}
