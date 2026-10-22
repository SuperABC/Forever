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
	AAssetElement* SpawnWorldAsset(Asset* asset, FVector location, FRotator rotation = FRotator::ZeroRotator);

	// 投掷：把一个已经从背包取出的Asset(count=1)当作物理掉落物扔出去——和SpawnWorldAsset
	// 同一个Actor，额外给初速度。thrower是投掷者，忽略与它的碰撞。Asset记进
	// Player::worldAssets(房间外的物品，不进Room::assets，落点在室内也一样)，所有权归Player，
	// 会随存档保存；扔出去之后不会自动消失。失败(同名物品已存在)返回nullptr，Asset仍归调用方。
	AAssetElement* SpawnThrownAsset(Asset* asset, FVector location, FVector velocity, AActor* thrower);

	// 存档前调用：把场景里房间外物品Actor的实际位置朝向写回Player::worldAssets。
	void SyncWorldAssetsToCore();

	// 读档后调用：按Player::worldAssets里记的位置朝向重新生成Actor，速度为0，从静止开始模拟。
	void RestoreWorldAssets();

	// 收集center周围radiusCm厘米内、归Player::worldAssets持有的物品(背包"附近"列表)，按距离从近到远。
	// 用球形重叠查询走引擎物理宽相位(代价和附近的数量成正比，不随worldAssets总量增长)，
	// 位置取Actor实时位置而不是只在存档前才同步的worldAssets transform。
	void CollectNearby(FVector center, float radiusCm, TArray<Asset*>& out) const;

	// 查表Destroy()+从表里移除——不delete Asset本身，调用方/Room负责Asset的生命周期。
	void DestroyWorldAsset(Asset* asset);

private:
	// 不能标UPROPERTY——Asset*是Core层的普通C++类型，不是UObject/USTRUCT，UHT反射不认。
	// 生命周期不受影响：SpawnActor出来的Actor由World自己的Actor列表持有，不依赖这张表的
	// 引用来防止GC，和ForeverWeaponComponent::currentWeapon(WeaponMod*)同一个道理。
	TMap<Asset*, AAssetElement*> worldAssetActors;
};
