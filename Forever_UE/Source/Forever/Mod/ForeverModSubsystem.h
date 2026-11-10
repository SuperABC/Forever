#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ForeverModSubsystem.generated.h"

// mod发现(Initialize)与Plugin/Pak挂载+Factory注册校验(EnsureModsRegistered)两阶段，见ForeverModSubsystem.md。
UCLASS()
class FOREVER_API UForeverModSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// Plugin/Pak挂载+Factory注册/校验，每次开局调用，可重复。
	void EnsureModsRegistered();

	// 用任意路径的配置文件替换内存里的Config状态(菜单"读取配置")。文件打不开/无效(解析后
	// dll_paths、resource_paths、story_scripts全空)时恢复成config.json的内容并返回false。
	// 只更新内存发现状态，不挂载不注册。
	bool ReloadConfigFrom(const FString& Path);

private:
	// ReadConfig + dll_paths缺省时回退扫描默认目录(resource_paths不回退)，Initialize和ReloadConfigFrom共用。
	void ReadConfigWithFallbacks(const FString& configPath);

	// 已经挂载过的Plugin/Pak挂载点("/PluginName/")，重复调用EnsureModsRegistered时跳过。
	TSet<FString> mountedMountPoints;
};
