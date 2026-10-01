#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AssetElement.generated.h"

class UStaticMeshComponent;
class Asset;

// 物品/资产在世界里的可视化表示——这次一律用引擎/项目通用的Cube占位(和
// ABuildingElement/ForeverBuildingFrameworkComponent用的是同一份"/Game/Asset/Meshes/
// Cube.Cube"网格)，不区分具体资产类型，见Source/Core/player/asset_mod.h"不设icon/mesh
// 字段"一节。带物理模拟+重力(Movable+"PhysicsActor"碰撞预设)，不是纯摆设的Static网格，
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

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> mesh;

	Asset* asset = nullptr; // 非owning，见类注释
};
