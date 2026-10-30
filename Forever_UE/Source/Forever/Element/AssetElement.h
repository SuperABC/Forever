#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include <string>

#include "AssetElement.generated.h"

class UStaticMeshComponent;
class UStaticMesh;
class UBoxComponent;
class UPrimitiveComponent;
struct FHitResult;
class Asset;

// 物品/资产在世界里的可视化表示——默认用项目通用的Cube占位(和ABuildingElement/
// ForeverBuildingFrameworkComponent用的是同一份"/Game/Asset/Meshes/Cube.Cube"网格)；资产的
// AssetMod指定了meshPath时(比如投掷物的球体)改用它指定的网格和meshScale缩放，见
// Source/Dependence/player/asset_mod.h。带物理模拟+重力(Movable+"PhysicsActor"碰撞预设)，不是纯摆设的Static网格，
// 玩家角色可以直接走过去推动它，见.cpp构造函数注释。由
// UForeverAssetFrameworkComponent::SpawnWorldAsset()在Drop时SpawnActor+Init()，
// DestroyWorldAsset()(Pick时)Destroy()——这个Actor不持有Asset的所有权，真正所有权在
// Room::assets或某个容器的contents里。
UCLASS()
class FOREVER_API AAssetElement : public AActor
{
	GENERATED_BODY()

public:
	AAssetElement();

	// 由UForeverAssetFrameworkComponent::SpawnWorldAsset()在SpawnActor之后立刻调用一次。
	void Init(Asset* inAsset);

	Asset* GetAsset() const { return asset; }

	// 按AssetMod::meshPath加载网格：路径为空返回nullptr(调用方沿用Cube占位)；路径非空但加载
	// 失败(比如项目球体还没复制进Content)时打Warning并回退引擎自带的球体。投掷预览模型也
	// 用这个函数，保证和掉落物同一份网格。
	static UStaticMesh* LoadAssetMesh(const std::string& meshPath);

	// 投掷：给物理体一个初速度。thrower不为空时忽略与它的碰撞(起点在投掷者胶囊体附近，
	// 不忽略会被顶开)；小而快的球体开连续碰撞检测，避免穿过薄墙/地面。
	void Launch(const FVector& velocity, AActor* thrower);

	// 交互选项在运行中变了(add_option/remove_option，由AForeverFrameworkActor::RefreshInteractOptions
	// 调用)：重新烘焙快照；玩家站在触发盒里时同步换掉屏幕上的选项。没有触发盒(资产没有脚本)时空操作。
	void RefreshOptions();

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 资产带脚本(asset->GetScript()非空)才建：比网格大一圈的只overlap触发盒，玩家的AForeverCharacter进入时
	// 把asset->GetOptions()推给MeetOption，照抄AVehicleElement::BuildProximityBox。
	void BuildProximityBox();

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void BakeOptions();
	void ShowOptions();
	void HideOptions();

	UPROPERTY()
	TObjectPtr<UBoxComponent> proximityBox;

	// 名字和选项的快照，Overlap回调只读这份，不解引用asset(和AVehicleElement同一套安全原则)。
	FString assetNameOnly;
	TArray<FString> cachedOptions;
	bool bPlayerInBox = false;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> mesh;

	Asset* asset = nullptr; // 非owning，见类注释
};
