#pragma once

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
};
