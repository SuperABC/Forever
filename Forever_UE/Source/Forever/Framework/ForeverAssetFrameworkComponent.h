#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include <string>
#include <vector>
#include "ForeverAssetFrameworkComponent.generated.h"

class AAssetElement;
class UStaticMesh;
class Asset;
class Room;
class Script;
class PickAssetChange;
class GrantAssetChange;
class DeductAssetChange;
class DropAssetChange;
class SpawnEffectChange;
class SoundEffectChange;
class DestroyAssetChange;
class SingleDamageChange;
class AreaDamageChange;
class AddSatietyChange;
struct ScriptContext;

// 地上的物品(房间里Room::assets的、室外Player::worldAssets的)在场景里的实体：登记、按玩家距离流式生成/销毁，
// 以及物品相关Change的执行。不持有Asset的所有权(真正所有权在Room/Player)，设计见ForeverAssetFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverAssetFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverAssetFrameworkComponent();
	virtual void TickComponent(float deltaTime, ELevelTick tickType, FActorComponentTickFunction* thisTickFunction) override;

	// 开局和读档各调一次(世界里的物品都建好/恢复好之后)：把所有房间里的物品和室外的物品登记成"在地上"，只登记不生成，
	// 实体由TickComponent按距离生成。
	void RegisterGroundAssets();

	// 加载界面的"预热物品"阶段(RegisterGroundAssets之后)：第一次生成某种物品要同步加载网格(编辑器里还要等网格编译)、
	// 建物理体，走近商店时会卡一下。这里先把会用到的网格全部加载好并留着引用：
	//   - 每种已启用的物品mod的网格(Asset::CollectRegisteredMeshes)、世界里现有物品的网格：加载后各临时生成一个实体再销毁；
	//   - 房间家具和固定物品的网格：走建筑组件的ResolveMesh，进它的缓存，近处LOD建家具ISM时直接命中。
	void WarmUpAssets();

	// 可移动物品的流式距离(地图单位，1=10米)：玩家到物品的水平距离小于assetSpawnDistance就生成实体，大于assetDespawnDistance
	// 就把位置写回Core再销毁实体。固定物品不走这个：显示跟房间家具一起建，弹选项的实体只在玩家进了它所在的房间时才有。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset|Streaming")
	float assetSpawnDistance = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset|Streaming")
	float assetDespawnDistance = 8.f;
	// 可移动物品每帧最多生成+销毁这么多个，剩下的下一帧接着做(从上次停下的地方往后扫)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset|Streaming")
	int32 maxAssetOpsPerTick = 4;
	// 实体生成后至少存在这么久(秒)才允许流式销毁：Chaos物理代理没建好就销毁会出问题，不是性能限制。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset|Streaming")
	float minAssetLifetime = 2.f;

	// 把物品放到地上：登记成"在地上"、把位置朝向写进Core(Asset的地面摆位)、立刻生成实体。调用方已经把Asset放进了
	// Room::assets或Player::worldAssets(丢弃/投掷/背包面板拖出来)，这些都发生在玩家身边。location是UE世界坐标。
	AAssetElement* SpawnWorldAsset(Asset* asset, FVector location, FRotator rotation = FRotator::ZeroRotator);

	// 投掷：把一个已经从背包取出的Asset(count=1)当作物理掉落物扔出去——和SpawnWorldAsset
	// 同一个Actor，额外给初速度。thrower是投掷者，忽略与它的碰撞。Asset记进
	// Player::worldAssets(房间外的物品，不进Room::assets，落点在室内也一样)，所有权归Player，
	// 会随存档保存；扔出去之后不会自动消失。失败(同名物品已存在)返回nullptr，Asset仍归调用方。
	AAssetElement* SpawnThrownAsset(Asset* asset, FVector location, FVector velocity, AActor* thrower);

	// 存档前调用：把所有有实体的物品(室内室外都算)的实际位置朝向写回Core。没有实体的保留上一次写回的位置。
	void SyncWorldAssetsToCore();

	// 收集center周围radiusCm厘米内、归Player::worldAssets持有的物品(背包"附近"列表)，按距离从近到远。
	// 用球形重叠查询走引擎物理宽相位(代价和附近的数量成正比，不随worldAssets总量增长)，
	// 位置取Actor实时位置而不是只在存档前才同步的worldAssets transform。
	void CollectNearby(FVector center, float radiusCm, TArray<Asset*>& out) const;

	// 物品离开地面(被捡走/要销毁)：取消登记并销毁实体(没有实体就只取消登记)——不delete Asset本身，调用方负责它的生命周期。
	// 任何会delete一件"在地上"的Asset的地方都必须先调它，否则登记表里留着悬空指针。
	void DestroyWorldAsset(Asset* asset);

	// 物品此刻的世界位置(UE坐标)：有实体取实体位置(Core里的地面摆位只在实体销毁/存档时才写回)，没有就取Core的地面摆位。
	// 房间mod布局后还没生成过的(restOnBottom：z是底面该落的高度，不是原点)返回false——它没动过，不用判断出没出房间。
	bool GetCurrentLocation(Asset* asset, FVector& out) const;

	// 可移动物品出了它所在的房间(ABuildingElement判断：房间盒的EndOverlap，或者房间盒建好时它已经不在盒子里)：
	// 所有权从Room::assets转给Player::worldAssets(有实体就先把实际位置写回Core)，之后不属于任何房间
	// (选项总能弹，背包"附近"列表里有它)。不在这个房间里、固定物品、室外已有同名物品时什么都不做。
	void DetachFromRoom(Asset* asset, Room* room);

	// 按资产名字找场景里的实体(在地上、并且当前在流式范围内已经生成的才有)，找不到返回nullptr。
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
	void ApplySoundEffect(const SoundEffectChange* change, const ScriptContext& context);
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
	// 一件"在地上"的物品。不能标UPROPERTY——Asset*是Core层的普通C++类型，UHT反射不认；实体用弱引用，
	// 被引擎销毁(掉出世界等)后自动失效，下次进入范围会按最后已知位置重新生成。
	struct FGroundEntry {
		Asset* asset = nullptr;
		Room* room = nullptr;                      // 归哪个房间(Room::assets)；nullptr=Player::worldAssets(室外/扔出去的)
		FString name;                              // 登记时拷一份，取消登记时不用再碰asset
		FVector location = FVector::ZeroVector;     // 最后已知位置(UE单位)，没有实体时按它算距离
		FRotator rotation = FRotator::ZeroRotator;
		TWeakObjectPtr<AAssetElement> element;
		double spawnedAt = 0.0;
	};
	TArray<FGroundEntry> groundEntries;
	// 预热加载的物品网格：留着引用，免得还没用上就被GC回收。
	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> warmedMeshes;
	TMap<Asset*, int32> entryIndexByAsset;
	TMap<FString, Asset*> assetByName;
	bool bGroundRegistered = false;
	int32 streamCursor = 0;               // 可移动物品流式扫描上一帧停在哪个下标
	// 上一次处理过的玩家所在房间(只比较，不解引用)；bRoomKnown=false时下一帧无条件按当前房间重新处理一遍。
	Room* optionRoom = nullptr;
	bool bRoomKnown = false;

	int32 AddGroundEntry(Asset* asset, Room* room = nullptr);   // 已登记的返回原下标(room以第一次登记为准)
	// 玩家换了房间：撤掉旧房间固定物品的选项实体、给新房间有脚本的固定物品生成选项实体，并重设所有可移动物品能不能弹选项。
	void OnPlayerRoomChanged(Room* current);
	// 物品实体此刻能不能弹选项：室外的物品总能；房间里的只在玩家就在那个房间里时能。
	bool CanShowOptions(const FGroundEntry& entry) const { return !entry.room || entry.room == optionRoom; }
	void RemoveGroundEntry(Asset* asset);
	// 生成实体(先补发脚本的game_start)；脚本把这件物品弄走了就返回nullptr。会改动groundEntries，调用后别再用旧的引用。
	AAssetElement* SpawnElementFor(Asset* asset);
	// 实体的当前位置朝向 -> 登记项和Core。固定物品不动，不写。
	void WriteBackToCore(FGroundEntry& entry);

	// 位置表达式/资产解析成世界坐标：positionExpression非空按"(x, y, z)"解析，否则取asset(缺省=脚本所属资产)的Actor位置。
	bool ResolvePosition(const std::string& positionExpression, const std::string& assetExpression,
		const ScriptContext& context, FVector& out) const;

	// 等待销毁的资产名(不存指针：销毁前它可能已经被捡走/换了容器，Flush时按名字重新找)。
	std::vector<std::string> pendingDestroy;
	std::vector<Asset*> pendingDelete;
};
