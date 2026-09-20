#include "storage_basic.h"

using namespace std;

int StorageBasic::count = 0;

StorageBasic::StorageBasic() : id(count++) {
}

const char* StorageBasic::GetName() {
	name = "StorageBasic" + to_string(id);
	return name.data();
}
