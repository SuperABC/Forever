#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ForeverModSubsystem.generated.h"

// 阶段3验证用途:启动时一次性扫描config.json配置的mod目录和每个concept的
// "<concept>_mods"参数列表,对19个concept分别构造一个临时Factory、设置参数表、注册发现的
// mod、创建+校验+销毁每个已注册id,验证完就地卸载,不长期持有任何Core/Dependence对象或dll
// 句柄。真正的Factory归属会在阶段4随各领域系统落地转移给对应Core系统类,详见
// ForeverModSubsystem.md。
//
// 游戏启动配置界面(Story/Mod/Resource三screen)落地后新增拆分:Initialize()只保留纯发现
// (ReadConfig+缺省回退扫描)，写进Config静态状态供菜单UI立刻有数据展示，不做任何真正的
// Plugin/Pak挂载或Factory注册/校验——那部分挪进新增的EnsureModsRegistered()，由
// UForeverConfigBridgeSubsystem::ValidateAndStartGame在Start Game校验通过后、OpenLevel
// 之前显式调用，bModsRegistered门闩保证每个GameInstance生命周期只真正跑一次，见
// ForeverModSubsystem.md。
UCLASS()
class FOREVER_API UForeverModSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 真正的Plugin/Pak挂载+Factory注册/校验——由配置界面点击"开始游戏"校验通过后显式调用，
	// bModsRegistered门闩保证只跑一次(不支持中途回菜单重新注册，见ForeverModSubsystem.md
	// "待办"一节)。
	void EnsureModsRegistered();

private:
	bool bModsRegistered = false;
};
