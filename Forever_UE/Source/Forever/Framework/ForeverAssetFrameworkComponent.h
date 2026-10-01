#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverAssetFrameworkComponent.generated.h"

class AAssetElement;
class Asset;

// 世界掉落物(cube占位)的生成/销毁登记——对应旧Framework Actor `Asset`(C++ Base:
// AssetBase)。背包UI的Drop/Pick动作分别调用SpawnWorldAsset/DestroyWorldAsset，见
// Source/Forever/UI/InventoryWidget.h。这个组件不持有Asset的所有权(真正所有权在
// Room::assets或容器的contents里)，只登记"哪个Asset对应世界里哪个Actor"，方便Pick时
// 反查到Actor销毁。
UCLASS()
class FOREVER_API UForeverAssetFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	// SpawnActor+Init+登记进worldAssetActors。location是Drop那一刻玩家的世界位置。
	AAssetElement* SpawnWorldAsset(Asset* asset, FVector location);

	// 查表Destroy()+从表里移除——不delete Asset本身，调用方/Room负责Asset的生命周期。
	void DestroyWorldAsset(Asset* asset);

private:
	// 不能标UPROPERTY——Asset*是Core层的普通C++类型，不是UObject/USTRUCT，UHT反射不认。
	// 生命周期不受影响：SpawnActor出来的Actor由World自己的Actor列表持有，不依赖这张表的
	// 引用来防止GC，和ForeverWeaponComponent::currentWeapon(WeaponMod*)同一个道理。
	TMap<Asset*, AAssetElement*> worldAssetActors;
};
