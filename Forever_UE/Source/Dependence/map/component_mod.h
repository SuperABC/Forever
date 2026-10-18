#pragma once

#include "common/mod_state.h"

#include <string>


// ComponentMod：一个"组合"类型的标签(比如"office"/"warehouse"，公司/组织以后会按这个
// 类型名撮合)——和RoomMod同理，Component永远是Building自己在Layout()里通过
// AssignRoom/ArrangeRow(component名字,id)间接创建的，不参与地块竞争，接口保持
// GetType()/GetName()两个虚方法即可，见Source/Core/map/component.h/building.md。
class ComponentMod {
public:
	ComponentMod() = default;
	virtual ~ComponentMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

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
