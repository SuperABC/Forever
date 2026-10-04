#pragma once

#include <string>
#include <vector>


// Storage：一个仓库的类型定义——只声明"能装什么(categories)"+"能装多少(capacity)"，
// 不知道自己的上下游/位置，这些由Industry的全城统一调配逻辑在运行时读写，见
// Core/industry/storage.h/storage.md。categories和ProductMod::categories同一套
// 字符串标签，任一交集即可存放。capacity是共享容量池(不同产品类型共用总容量)，不是
// 每种产品类型各自独立配额，照抄老工程的设计。
class StorageMod {
public:
	StorageMod() = default;
	virtual ~StorageMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// 具体子类必须在SetProperty()里填这两个字段，不要在构造函数里赋值——构造函数只负责
	// id/count这类登记，和ManufactureMod::SetTargets()/TerrainMod::SetupTexture()同一套
	// "两段式"约定，Core创建完mod实例后会立刻调一次这个方法，再读这两个字段。
	virtual void SetProperty() = 0;

	std::vector<std::string> categories;
	float capacity = 0.f;
};
