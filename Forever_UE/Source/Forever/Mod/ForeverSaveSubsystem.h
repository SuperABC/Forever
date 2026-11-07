#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "common/save.h"

#include "ForeverSaveSubsystem.generated.h"

// 一次已经通过校验、等待World关卡开局时消费的读档请求。纯C++结构，不进反射。
struct FForeverPendingLoad {
	FString path;
	SaveArchive archive;
	SaveManifest manifest;
};

// 存档/读档的UE侧入口，详见ForeverSaveSubsystem.md。
UCLASS()
class FOREVER_API UForeverSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 把当前游戏写进Path(.fsav)。失败返回false并给出原因。
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool SaveToFile(const FString& Path, FString& OutError);

	// 先弹保存对话框再SaveToFile。用户取消时返回false、OutError为空。
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool SaveWithDialog(FString& OutError);

	// 读档：读文件、校验(版本/mod/依赖/资源)、把存档的mod集合应用到当前配置、确认mod都已加载，
	// 全部通过才登记pendingLoad并OpenLevel(World)；任何一步失败都恢复原配置、返回false并给出原因。
	// 菜单和游戏内都调用这一个函数。
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool RequestLoad(const FString& Path, FString& OutError);

	// 先弹打开对话框再RequestLoad。用户取消时返回false、OutError为空。
	UFUNCTION(BlueprintCallable, Category = "Save")
	bool LoadWithDialog(FString& OutError);

	bool HasPendingLoad() const { return pendingLoad.IsValid(); }

	// World关卡开局时取走读档请求(取走之后就清空)。
	TSharedPtr<FForeverPendingLoad> ConsumePendingLoad();

private:
	// 读文件+检查外层头+解压+解析分段容器。
	bool ReadArchive(const FString& Path, SaveArchive& OutArchive, FString& OutError) const;

	TSharedPtr<FForeverPendingLoad> pendingLoad;
};
