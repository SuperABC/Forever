#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ForeverModSubsystem.generated.h"

// Initialize()只做纯发现(ReadConfig+缺省回退扫描)，把结果写进Config静态状态供配置界面
// (Story/Mod/Resource三screen)立刻有数据展示，不做任何真正的Plugin/Pak挂载或Factory
// 注册/校验——那部分在EnsureModsRegistered()里，由UForeverConfigBridgeSubsystem::
// ValidateAndStartGame在Start Game依赖校验通过后、OpenLevel之前显式调用，对16个concept
// 分别构造一个临时Factory、设置参数表、注册发现的mod、创建+校验+销毁每个已注册id，验证完
// 就地卸载，不长期持有任何Core/Dependence对象或dll句柄。可以重复调用(退出游戏回菜单后再
// 开始游戏)：已经挂载过的Plugin/Pak按挂载点跳过，校验每次重新跑。真正需要长期存在的Factory归属转移给对应Core领域系统类
// (如Terrain已经转移给Map持有)，详见ForeverModSubsystem.md。
UCLASS()
class FOREVER_API UForeverModSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 真正的Plugin/Pak挂载+Factory注册/校验——由配置界面点击"开始游戏"校验通过后显式调用，
	// 每次开局都会调用(退出游戏回菜单再开始也一样)：新出现的Plugin/Pak才挂载(mountedMountPoints
	// 记录已挂载的)，Factory校验每次用当前config重新跑。真正长期持有的mod dll由
	// Registry::RefreshMods()在AForeverFrameworkActor::BeginPlay里增量加载。
	void EnsureModsRegistered();

	// 用任意路径的配置文件替换内存里的Config状态(菜单"读取配置")。文件打不开/无效(解析后
	// dll_paths、resource_paths、story_scripts全空)时恢复成config.json的内容并返回false。
	// 只更新内存发现状态，不挂载不注册。
	bool ReloadConfigFrom(const FString& Path);

private:
	// ReadConfig + dll_paths/resource_paths缺省时回退扫描默认目录，Initialize和ReloadConfigFrom共用。
	void ReadConfigWithFallbacks(const FString& configPath);

	// 已经挂载过的Plugin/Pak挂载点("/PluginName/")，重复调用EnsureModsRegistered时跳过。
	TSet<FString> mountedMountPoints;
};
