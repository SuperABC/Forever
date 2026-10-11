#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ForeverModSubsystem.generated.h"

// Initialize()只做纯发现(ReadConfig+缺省回退扫描)，把结果写进Config静态状态供配置界面
// (Story/Mod/Resource三screen)立刻有数据展示，不做任何真正的Plugin/Pak挂载或Factory
// 注册/校验——那部分在EnsureModsRegistered()里，由UForeverConfigBridgeSubsystem::
// ValidateAndStartGame在Start Game依赖校验通过后、OpenLevel之前显式调用，对16个concept
// 分别构造一个临时Factory、设置参数表、注册发现的mod、创建+校验+销毁每个已注册id，验证完
// 就地卸载，不长期持有任何Core/Dependence对象或dll句柄。bModsRegistered门闩保证每个
// GameInstance生命周期只真正跑一次。真正需要长期存在的Factory归属转移给对应Core领域系统类
// (如Terrain已经转移给Map持有)，详见ForeverModSubsystem.md。
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
