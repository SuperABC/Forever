#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverRoomFrameworkComponent.generated.h"

// 对应旧Framework Actor `Room`(C++ Base:RoomBase)。Room进入/离开检测碰撞盒实际生成在每栋
// building专属的`ABuildingElement`里(见Source/Forever/Element/BuildingElement.h)，跟着
// 那栋楼自己的碰撞盒/近处LOD组件一起attach到同一个Actor，不在这个组件里——全地图所有Room
// 共用一个owner会被物理引擎的碰撞体焊接开销拖累，见
// Source/Forever/Framework/ForeverBuildingFrameworkComponent.md"性能排查"一节。这个组件是
// 空骨架，留着`Room`这个域槽位给以后的阶段（职责说明见ForeverFrameworkComponent.md域
// 对照表）。
UCLASS()
class FOREVER_API UForeverRoomFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()
};
