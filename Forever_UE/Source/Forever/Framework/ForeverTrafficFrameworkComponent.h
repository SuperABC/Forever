#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverTrafficFrameworkComponent.generated.h"

// 阶段2骨架:对应旧Framework Actor `Traffic`(C++ Base:TrafficBase)。
// 当前为空实现,阶段4迁移对应系统时把逻辑收进来。职责说明见ForeverFrameworkComponent.md域对照表。
UCLASS()
class FOREVER_API UForeverTrafficFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()
};
