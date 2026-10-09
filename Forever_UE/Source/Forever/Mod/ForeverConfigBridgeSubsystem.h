#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ForeverConfigBridgeSubsystem.generated.h"

// 一行mod enable记录，对应Config::ModEntry——Blueprint侧用来在Mod screen按行生成
// ModCheckRow，见.dump/ModPanel.txt还原出的"每个concept一个ScrollBox，每个mod id一行"结构。
USTRUCT(BlueprintType)
struct FForeverModRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString DllPath;

	UPROPERTY(BlueprintReadOnly)
	FString ConceptKey;

	UPROPERTY(BlueprintReadOnly)
	FString ModId;

	UPROPERTY(BlueprintReadOnly)
	bool bEnabled = true;

	// 这个(ConceptKey,ModId)当前是否因为"被其它已启用mod/已添加主线剧情脚本依赖"而锁定——
	// 锁定时Blueprint侧对应行要勾选框强制勾中+整行变灰，见ModCheckRowWidget。
	UPROPERTY(BlueprintReadOnly)
	bool bLocked = false;
};

// 游戏启动配置界面(Story/Mod/Resource三screen)的Blueprint桥接层，替代老工程的AStartBase——
// 做成UGameInstanceSubsystem而不是AActor，不需要在Menu关卡里放置一个Actor，Menu->World的
// OpenLevel不会把它销毁(ValidateAndStartGame需要在触发EnsureModsRegistered()和OpenLevel
// 前后都存活)，和ForeverModSubsystem走同一套既有模式。全部方法都是Config静态方法的
// FString<->std::string薄包装，业务逻辑只有ValidateAndStartGame的聚合校验和IsModLocked的
// 闭包计算两处，见ForeverConfigBridgeSubsystem.md。
UCLASS()
class FOREVER_API UForeverConfigBridgeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ---- Mod screen ----

	// 当前已注册的dll根目录路径列表(Config::GetDllPaths())，即Mod screen左列要展示的
	// 内容——删除某一行=RemoveDllPath(这个根目录)，和AddDllPathButton添加的是同一个
	// 粒度。
	UFUNCTION(BlueprintCallable, Category = "Config")
	TArray<FString> GetDllPaths() const;

	UFUNCTION(BlueprintCallable, Category = "Config")
	TArray<FForeverModRow> GetModRows() const;

	UFUNCTION(BlueprintCallable, Category = "Config")
	void AddDllPath(const FString& Path);

	UFUNCTION(BlueprintCallable, Category = "Config")
	void RemoveDllPath(const FString& Path);

	UFUNCTION(BlueprintCallable, Category = "Config")
	void SetModEnabled(const FString& ConceptKey, const FString& ModId, bool bEnabled);

	// 这个(ConceptKey,ModId)当前是否被锁定(被其它已启用mod的DLL级依赖，或任一Story screen
	// 脚本的mod_dependences，声明为必需)——锁定判定不看concept，只认mod id字符串，和
	// Registry::CheckModRegistered同一套匹配规则。
	UFUNCTION(BlueprintCallable, Category = "Config")
	bool IsModLocked(const FString& ConceptKey, const FString& ModId) const;

	// ---- Resource screen ----

	UFUNCTION(BlueprintCallable, Category = "Config")
	TArray<FString> GetResourceRootPaths() const;

	UFUNCTION(BlueprintCallable, Category = "Config")
	void AddResourcePath(const FString& Path);

	UFUNCTION(BlueprintCallable, Category = "Config")
	void RemoveResourcePath(const FString& Path);

	// ---- Story screen ----

	UFUNCTION(BlueprintCallable, Category = "Config")
	TArray<FString> GetStoryScripts() const;

	UFUNCTION(BlueprintCallable, Category = "Config")
	void AddStoryScript(const FString& Path);

	UFUNCTION(BlueprintCallable, Category = "Config")
	void RemoveStoryScript(const FString& Path);

	// ---- 共用 ----

	// 校验Story screen全部脚本的mod_dependences + 每个已加载(至少一个id启用)DLL自己的
	// GetModDllDependencies声明，是否都能在当前启用的mod id集合里找到。全部通过才会写回
	// config.json、触发UForeverModSubsystem::EnsureModsRegistered()、OpenLevel进入
	// World.World，返回true；否则OutFailureMessage里拼好失败原因，返回false，不做任何
	// 状态改变(不写config.json，不触发注册，不切关卡)。
	UFUNCTION(BlueprintCallable, Category = "Config")
	bool ValidateAndStartGame(FString& OutFailureMessage);

	// 原生文件夹选择器(IDesktopPlatform)，照抄老工程AStartBase::SelectFolder()。返回false
	// 表示用户取消了选择，OutPath不变。
	UFUNCTION(BlueprintCallable, Category = "Config")
	bool SelectFolder(FString& OutPath) const;

	// 原生文件选择器，FileFilter形如"Script Files (*.script)|*.script"。
	UFUNCTION(BlueprintCallable, Category = "Config")
	bool SelectFile(const FString& FileFilter, FString& OutPath) const;

private:
	// ValidateAndStartGame/IsModLocked共用的闭包计算：当前"必需启用"的mod id集合=全部Story
	// screen脚本的mod_dependences(无条件计入)∪每个"至少一个id已启用"的DLL自己声明的
	// GetModDllDependencies(条件计入)。
	TSet<FString> ComputeRequiredModIds() const;
};
