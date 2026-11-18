#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include <string>
#include <vector>
#include "ForeverAssetFrameworkComponent.generated.h"

class AAssetElement;
class Asset;
class Script;
class PickAssetChange;
class GrantAssetChange;
class DeductAssetChange;
class DropAssetChange;
class SpawnEffectChange;
class DestroyAssetChange;
class SingleDamageChange;
class AreaDamageChange;
class AddSatietyChange;
struct ScriptContext;

// 世界掉落物的生成/销毁登记。背包UI的Drop/Pick动作分别调用SpawnWorldAsset/DestroyWorldAsset，见
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

	// 按资产名字找场景里的Actor(只有房间里/地上的资产有)，找不到返回nullptr。
	AAssetElement* FindElementByName(const FString& name) const;

	// pick_asset/grant_asset/deduct_asset/drop_asset四个Change的执行，由AForeverFrameworkActor::ApplyChange转发。
	// 事件(get_asset/lose_asset)发给执行Change的脚本(context.self)，见UForeverStoryFrameworkComponent::NotifyAssetEvent。
	void ApplyPickAsset(const PickAssetChange* change, const ScriptContext& context);
	void ApplyGrantAsset(const GrantAssetChange* change, const ScriptContext& context);
	void ApplyDeductAsset(const DeductAssetChange* change, const ScriptContext& context);
	void ApplyDropAsset(const DropAssetChange* change, const ScriptContext& context);

	// spawn_effect：生成一次特效。位置取position字段("(x, y, z)"表达式，如$$local.hit)；留空则取指定资产
	// (缺省=执行Change的脚本所属资产)的场景位置；两者都得不到(比如武器脚本没填position)就Warning并跳过。
	void ApplySpawnEffect(const SpawnEffectChange* change, const ScriptContext& context);
	// single_damage/area_damage：伤害Change的执行(扣血、致死处理见FForeverDamage)。area_damage的position规则同spawn_effect。
	void ApplySingleDamage(const SingleDamageChange* change, const ScriptContext& context);
	void ApplyAreaDamage(const AreaDamageChange* change, const ScriptContext& context);
	// add_satiety：给市民加饱食度(target空=玩家当前操控的市民)。
	void ApplyAddSatiety(const AddSatietyChange* change, const ScriptContext& context);
	// destroy_asset：销毁地面资产。这里只隐藏Actor并登记名字——同一个里程碑里后面的Change还要用这件资产的脚本
	// (context.self)，所以真正的销毁(Actor+Asset)推迟到FlushPendingDestroy，由AForeverFrameworkActor::Tick调用。
	void ApplyDestroyAsset(const DestroyAssetChange* change, const ScriptContext& context);
	void FlushPendingDestroy();
	// 已经从世界/身上摘下来、数量并进了别的堆叠的资产(StoreResult::Merged)：脚本里的pick/grant可能正是这件资产
	// 自己的脚本在执行，不能立刻delete，登记到这里，下一次FlushPendingDestroy时才delete。
	void QueueDelete(Asset* asset);

private:
	// 不能标UPROPERTY——Asset*是Core层的普通C++类型，不是UObject/USTRUCT，UHT反射不认。
	// 生命周期不受影响：SpawnActor出来的Actor由World自己的Actor列表持有，不依赖这张表的
	// 引用来防止GC，和ForeverWeaponComponent::currentWeapon(WeaponMod*)同一个道理。
	TMap<Asset*, AAssetElement*> worldAssetActors;

	// 位置表达式/资产解析成世界坐标：positionExpression非空按"(x, y, z)"解析，否则取asset(缺省=脚本所属资产)的Actor位置。
	bool ResolvePosition(const std::string& positionExpression, const std::string& assetExpression,
		const ScriptContext& context, FVector& out) const;

	// 等待销毁的资产名(不存指针：销毁前它可能已经被捡走/换了容器，Flush时按名字重新找)。
	std::vector<std::string> pendingDestroy;
	std::vector<Asset*> pendingDelete;
};
