#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverTerrainFrameworkComponent.generated.h"

// 阶段2骨架:对应旧Framework Actor `Terrain`(C++ Base:TerrainBase)。
// 当前为空实现,阶段4迁移对应系统时把逻辑收进来。职责说明见ForeverFrameworkComponent.md域对照表。
UCLASS()
class FOREVER_API UForeverTerrainFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()
};
