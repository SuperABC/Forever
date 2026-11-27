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

// 物品/资产在世界里的可视化表示，不持有Asset的所有权，见AssetElement.md。两种形态：
//   普通物品(有AssetMod)：物理体(Cube占位或mod指定的网格)，能被推动、捡走。
//   固定物品(Asset::IsFixture()，没有AssetMod)：显示和阻挡跟房间家具一起由ABuildingElement建(ISM)，这个Actor只剩
//   弹选项的触发盒(网格隐藏、不碰撞)，玩家进了这件物品所在的房间才生成。
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

	// 加载界面预热：用这个网格临时生成一个物品实体(不Init，没有Asset)再立刻销毁，把组件注册/物理体创建的一次性开销
	// 摊到加载界面(同UForeverTrafficFrameworkComponent::WarmVehicleClasses)。mesh为空用默认Cube。
	static void SpawnWarmup(UWorld* world, UStaticMesh* mesh, const FVector& location);

	// 投掷：给物理体一个初速度。thrower不为空时忽略与它的碰撞(起点在投掷者胶囊体附近，
	// 不忽略会被顶开)；小而快的球体开连续碰撞检测，避免穿过薄墙/地面。
	void Launch(const FVector& velocity, AActor* thrower);

	// 交互选项在运行中变了(add_option/remove_option，由AForeverFrameworkActor::RefreshInteractOptions
	// 调用)：重新烘焙快照；玩家站在触发盒里时同步换掉屏幕上的选项。没有触发盒(资产没有脚本)时空操作。
	void RefreshOptions();

	// 允不允许弹选项：房间里的物品只在玩家就在这个房间里时才弹(隔着墙站在隔壁房间不弹)，由物品组件在玩家换房间时设置。
	// 玩家正站在触发盒里时立刻显示/撤掉屏幕上的选项。默认允许。
	void SetOptionsAllowed(bool bAllowed);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 固定物品：关掉物理和网格碰撞、隐藏网格(显示和阻挡在房间家具的ISM里)，只留触发盒。
	void SetupFixture();

	// 资产带脚本(asset->GetScript()非空)才建：只overlap的触发盒(普通物品是原点周围120x120x80cm；固定物品是网格包围盒
	// 向外扩一圈)，玩家的AForeverCharacter进入时
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
	bool bOptionsAllowed = true;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> mesh;

	Asset* asset = nullptr; // 非owning，见类注释
};
