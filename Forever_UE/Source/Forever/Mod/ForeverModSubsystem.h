#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ForeverModSubsystem.generated.h"

// 阶段3验证用途:启动时一次性扫描config.json配置的mod目录和每个concept的
// "<concept>_mods"参数列表,对21个concept分别构造一个临时Factory、设置参数表、注册发现的
// mod、创建+校验+销毁每个已注册id,验证完就地卸载,不长期持有任何Core/Dependence对象或dll
// 句柄。真正的Factory归属会在阶段4随各领域系统落地转移给对应Core系统类,详见
// ForeverModSubsystem.md。
UCLASS()
class FOREVER_API UForeverModSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
};
