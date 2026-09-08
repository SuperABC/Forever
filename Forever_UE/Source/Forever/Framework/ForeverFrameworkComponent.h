#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ForeverFrameworkComponent.generated.h"

// 阶段2骨架:所有Framework域组件(见ForeverFrameworkComponent.md域对照表)的共同基类,
// 当前无任何逻辑,只作为类型标签,方便阶段4+之后统一按类型查询/遍历。
UCLASS(Abstract)
class FOREVER_API UForeverFrameworkComponent : public UActorComponent
{
	GENERATED_BODY()
};
