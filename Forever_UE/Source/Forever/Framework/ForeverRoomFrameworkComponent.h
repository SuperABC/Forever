#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverRoomFrameworkComponent.generated.h"

// 对应旧Framework Actor `Room`(C++ Base:RoomBase)。Room进入/离开检测碰撞盒一度在这里实现
// (第N轮迁移)，后来发现全地图所有Room共用这个组件所在的单例owner会被物理引擎的碰撞体焊接
// 开销拖累(和Building近处LOD组件同一个根因，见
// Source/Forever/Framework/ForeverBuildingFrameworkComponent.md"性能第2轮"一节)，已经挪到
// 每栋building专属的`ABuildingElement`里(见Source/Forever/Element/BuildingElement.h)，
// 跟着那栋楼自己的碰撞盒/近处LOD组件一起attach到同一个Actor。这个组件恢复成空骨架，留着
// `Room`这个域槽位给以后的阶段（职责说明见ForeverFrameworkComponent.md域对照表）。
UCLASS()
class FOREVER_API UForeverRoomFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()
};
