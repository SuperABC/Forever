#pragma once

#include "common/mod_state.h"

#include <string>
#include <vector>


// Storage：一个仓库的类型定义——只声明"能装什么(categories)"+"能装多少(capacity)"，
// 不知道自己的上下游/位置，这些由Industry的全城统一调配逻辑在运行时读写，见
// Core/industry/storage.h/storage.md。categories和ProductMod::categories同一套
// 字符串标签，任一交集即可存放。capacity是共享容量池(不同产品类型共用总容量)，不是
// 每种产品类型各自独立配额。
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

	// ---- 存档(见Source/Dependence/common/mod_state.md) ----
	// mod私有数据的存档/读档钩子，默认不存任何东西。基类里的公共声明字段由Core直接快照，
	// 这里只放mod自己的私有数据。
	virtual void SaveState(StateWriter&) const {}
	virtual void LoadState(StateReader&) {}
	// SaveState写出数据的格式版本：改了自己SaveState的格式就加一，读档时从
	// StateReader::GetSavedVersion()拿到当时的版本自己迁移。默认0。
	virtual int GetStateVersion() const { return 0; }
	// 这个mod类用来生成唯一实例名的静态计数器——存档保存、读档后恢复，避免读档后新建的实例和
	// 恢复出来的名字撞车。没有计数器的类保持默认(返回-1、Set不做事)。
	virtual int GetInstanceCounter() const { return -1; }
	virtual void SetInstanceCounter(int) {}
};
