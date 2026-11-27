#include "Framework/ForeverAssetFrameworkComponent.h"

#include "Element/AssetElement.h"
#include "Framework/ForeverFrameworkActor.h"
#include "Framework/CoreGuard.h"

#include "Framework/ForeverStoryFrameworkComponent.h"
#include "Player/ForeverExplosionEffect.h"
#include "Player/ForeverDamage.h"
#include "Element/CitizenElement.h"
#include "Framework/ForeverPopulaceFrameworkComponent.h"
#include "populace/populace.h"
#include "populace/citizen.h"

#include "player/player.h"
#include "player/asset.h"
#include "map/map.h"
#include "map/room.h"
#include "map/building.h"
#include "story/change.h"
#include "story/expression.h"
#include "story/script.h"
#include "common/utility.h"

#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Sound/SoundBase.h"

#include <algorithm>
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"

#define ASSET_WORLD_SCALE 1000.f
// 固定物品的生成高度比地板高这么多(cm)，和房间家具的BUILDING_HEIGHT_EPSILON一致，避免和地板共面闪烁。
#define ASSET_FIXTURE_HEIGHT_EPSILON 1.f
// 布局摆的可移动物品(restOnBottom)：底面比给定高度再高这么多(cm)，靠重力落地，不嵌进地板。
#define ASSET_REST_HEIGHT_EPSILON 2.f

namespace {
	// Core的地面摆位(地图单位、度) <-> UE世界坐标(cm、度)。
	void GroundToWorld(const AssetGround& ground, FVector& location, FRotator& rotation) {
		location = FVector(ground.x, ground.y, ground.z) * ASSET_WORLD_SCALE;
		rotation = FRotator(ground.pitch, ground.yaw, ground.roll);
	}

	AssetGround WorldToGround(const FVector& location, const FRotator& rotation) {
		AssetGround ground;
		ground.x = static_cast<float>(location.X / ASSET_WORLD_SCALE);
		ground.y = static_cast<float>(location.Y / ASSET_WORLD_SCALE);
		ground.z = static_cast<float>(location.Z / ASSET_WORLD_SCALE);
		ground.pitch = static_cast<float>(rotation.Pitch);
		ground.yaw = static_cast<float>(rotation.Yaw);
		ground.roll = static_cast<float>(rotation.Roll);
		ground.restOnBottom = false; // 这是实体原点的实际位置
		return ground;
	}
}

UForeverAssetFrameworkComponent::UForeverAssetFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = true;
}

int32 UForeverAssetFrameworkComponent::AddGroundEntry(Asset* asset, Room* room) {
	if (int32* found = entryIndexByAsset.Find(asset)) return *found;
	FGroundEntry entry;
	entry.asset = asset;
	entry.room = room;
	entry.name = UTF8_TO_TCHAR(asset->GetName().c_str());
	GroundToWorld(asset->GetGround(), entry.location, entry.rotation);
	const int32 index = groundEntries.Add(entry);
	entryIndexByAsset.Add(asset, index);
	assetByName.Add(entry.name, asset);
	return index;
}

void UForeverAssetFrameworkComponent::RemoveGroundEntry(Asset* asset) {
	const int32* found = entryIndexByAsset.Find(asset);
	if (!found) return;
	const int32 index = *found;
	assetByName.Remove(groundEntries[index].name);
	entryIndexByAsset.Remove(asset);
	groundEntries.RemoveAtSwap(index, 1, EAllowShrinking::No);
	if (groundEntries.IsValidIndex(index)) entryIndexByAsset.Add(groundEntries[index].asset, index); // 被换到这个下标的那一项
}

void UForeverAssetFrameworkComponent::RegisterGroundAssets() {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	if (!framework) return;
	if (Map* map = framework->GetMap()) {
		for (const auto& [buildingName, building] : map->GetBuildings()) {
			if (!building) continue;
			for (Room* room : building->GetRooms()) {
				if (!room) continue;
				for (const auto& [name, asset] : room->GetAssets()) if (asset) AddGroundEntry(asset, room);
			}
		}
	}
	if (Player* player = framework->GetPlayer()) {
		for (const auto& [name, entry] : player->GetWorldAssets()) if (entry.asset) AddGroundEntry(entry.asset);
	}
	bGroundRegistered = true;
	bRoomKnown = false; // 读档后按玩家当前房间重新挂固定物品的选项实体
	UE_LOG(LogTemp, Log, TEXT("UForeverAssetFrameworkComponent: %d ground assets registered for streaming."), groundEntries.Num());
}

AAssetElement* UForeverAssetFrameworkComponent::SpawnElementFor(Asset* asset) {
	if (!asset || !GetWorld()) return nullptr;

	// 带脚本的资产第一次变成场景物体时补发game_start(脚本里一般在这时add_option)，要在Init烘焙选项快照之前。
	// 脚本可能把这件物品弄走(捡起/销毁)，所以发完再重新找登记项。
	if (AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner())) {
		if (framework->GetStoryFramework()) framework->GetStoryFramework()->StartAssetScript(asset);
	}
	const int32* found = entryIndexByAsset.Find(asset);
	if (!found) return nullptr;
	FGroundEntry& entry = groundEntries[*found];
	if (AAssetElement* existing = entry.element.Get()) return existing;

	// 缩放走Transform：本Actor的根就是网格组件，物理体在FinishSpawning时按最终的网格和缩放创建。
	// 没指定meshPath的资产保持Cube占位、缩放1。
	float scaleX = 1.f, scaleY = 1.f, scaleZ = 1.f;
	if (!asset->GetMeshPath().empty()) asset->GetMeshScale3(scaleX, scaleY, scaleZ);
	FVector location = entry.location;
	const bool restOnBottom = !asset->IsFixture() && asset->GetGround().restOnBottom;
	if (asset->IsFixture()) {
		location.Z += ASSET_FIXTURE_HEIGHT_EPSILON;
	}
	else if (restOnBottom) {
		// 房间mod布局时给的z是"底面该落的高度"：按网格包围盒把原点抬上去(网格原点不一定在底面)。
		UStaticMesh* staticMesh = AAssetElement::LoadAssetMesh(asset->GetMeshPath());
		if (!staticMesh) staticMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Asset/Common/Mesh/Cube.Cube"));
		if (staticMesh) {
			const FBoxSphereBounds bounds = staticMesh->GetBounds();
			location.Z += -(bounds.Origin.Z - bounds.BoxExtent.Z) * scaleZ + ASSET_REST_HEIGHT_EPSILON;
		}
	}

	FTransform transform(entry.rotation, location, FVector(scaleX, scaleY, scaleZ));
	AAssetElement* element = GetWorld()->SpawnActorDeferred<AAssetElement>(
		AAssetElement::StaticClass(), transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!element) return nullptr;
	element->Init(asset);
	element->SetOptionsAllowed(CanShowOptions(entry));
	element->FinishSpawning(transform);

	entry.element = element;
	entry.spawnedAt = GetWorld()->GetTimeSeconds();
	if (restOnBottom) {
		// 从这一刻起Core里记的就是实体原点的位置，以后重新生成不再抬高。
		entry.location = location;
		asset->SetGround(WorldToGround(location, entry.rotation));
	}
	return element;
}

void UForeverAssetFrameworkComponent::WriteBackToCore(FGroundEntry& entry) {
	AAssetElement* element = entry.element.Get();
	if (!element || !entry.asset || entry.asset->IsFixture()) return;
	entry.location = element->GetActorLocation();
	entry.rotation = element->GetActorRotation();
	entry.asset->SetGround(WorldToGround(entry.location, entry.rotation));
}

void UForeverAssetFrameworkComponent::TickComponent(float deltaTime, ELevelTick tickType, FActorComponentTickFunction* thisTickFunction) {
	Super::TickComponent(deltaTime, tickType, thisTickFunction);
	CORE_GUARD_BEGIN("UForeverAssetFrameworkComponent::TickComponent")
	if (!bGroundRegistered || groundEntries.Num() == 0 || !GetWorld()) return;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	if (!framework || !framework->IsInitialGenerationDone()) return; // 楼层地板(外壳)要等世界生成完才有碰撞
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn) return;

	// 玩家换了房间(Player::currentRoom由房间触发盒设置)：固定物品的选项实体跟着换，可移动物品重设能不能弹选项。
	if (Player* player = framework->GetPlayer()) {
		Room* current = player->GetCurrentRoom();
		if (!bRoomKnown || current != optionRoom) {
			optionRoom = current;
			bRoomKnown = true;
			OnPlayerRoomChanged(current);
		}
	}

	// 可移动物品按水平距离流式(不看高度)，每帧最多maxAssetOpsPerTick个生成/销毁，从上一帧停下的地方接着扫。
	const FVector2D player(pawn->GetActorLocation());
	const double spawnLimit = FMath::Max(0.f, assetSpawnDistance) * ASSET_WORLD_SCALE;
	const double despawnLimit = FMath::Max(assetSpawnDistance, assetDespawnDistance) * ASSET_WORLD_SCALE;
	const double spawnSquared = spawnLimit * spawnLimit, despawnSquared = despawnLimit * despawnLimit;
	const double now = GetWorld()->GetTimeSeconds();
	const int32 budget = FMath::Max(1, maxAssetOpsPerTick);

	// 生成会跑物品脚本的game_start，脚本可能增删登记项(捡起/销毁)，所以按下标走、每次重新取引用。
	int32 ops = 0;
	const int32 total = groundEntries.Num();
	if (streamCursor >= total) streamCursor = 0;
	for (int32 step = 0; step < total && groundEntries.Num() > 0; step++) {
		const int32 index = (streamCursor + step) % groundEntries.Num();
		if (ops >= budget) {
			streamCursor = index; // 预算用完：下一帧从这一项接着扫
			break;
		}
		FGroundEntry& entry = groundEntries[index];
		if (!entry.asset || entry.asset->IsFixture()) continue; // 固定物品见OnPlayerRoomChanged
		AAssetElement* element = entry.element.Get();
		if (!element) {
			if (FVector2D::DistSquared(player, FVector2D(entry.location)) < spawnSquared) {
				SpawnElementFor(entry.asset);
				ops++;
			}
			continue;
		}
		if (FVector2D::DistSquared(player, FVector2D(element->GetActorLocation())) > despawnSquared &&
			now - entry.spawnedAt >= minAssetLifetime) {
			WriteBackToCore(entry); // 走远了：把实际位置记回Core，下次回来在原处生成
			element->Destroy();
			entry.element = nullptr;
			ops++;
		}
	}
	CORE_GUARD_END
}

void UForeverAssetFrameworkComponent::OnPlayerRoomChanged(Room* current) {
	// 固定物品：离开的房间撤掉选项实体，进入的房间给有脚本的生成(第一次生成时补发game_start)。
	// 没有脚本的固定物品没有选项，不需要实体——显示和阻挡在房间家具的ISM里。
	TArray<Asset*> toSpawn;
	for (FGroundEntry& entry : groundEntries) {
		if (!entry.asset || !entry.asset->IsFixture()) continue;
		AAssetElement* element = entry.element.Get();
		if (element && entry.room != current) {
			element->Destroy();
			entry.element = nullptr;
		}
		else if (!element && current && entry.room == current && entry.asset->GetScript()) {
			toSpawn.Add(entry.asset);
		}
	}
	// 生成会跑脚本、可能改动登记表，先收集再逐个生成。
	for (Asset* asset : toSpawn) SpawnElementFor(asset);
	// 可移动物品：房间里的只在玩家就在那个房间里时弹选项(触发盒会伸过墙，站在隔壁房间不该弹)。
	for (FGroundEntry& entry : groundEntries) {
		if (entry.asset && entry.asset->IsFixture()) continue;
		if (AAssetElement* element = entry.element.Get()) element->SetOptionsAllowed(CanShowOptions(entry));
	}
}

AAssetElement* UForeverAssetFrameworkComponent::SpawnWorldAsset(Asset* asset, FVector location, FRotator rotation) {
	if (!asset || !GetWorld()) return nullptr;

	// 归属：调用方已经把它放进了玩家所在房间的Room::assets(在房间里丢/拖出来)，或者Player::worldAssets。
	Room* owner = nullptr;
	if (AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner())) {
		if (Player* player = framework->GetPlayer()) {
			if (Room* room = player->GetCurrentRoom()) {
				auto it = room->GetAssets().find(asset->GetName());
				if (it != room->GetAssets().end() && it->second == asset) owner = room;
			}
		}
	}
	const int32 index = AddGroundEntry(asset, owner);
	FGroundEntry& entry = groundEntries[index];
	entry.room = owner; // 已经登记过的(重新放下)也按这次的归属
	entry.location = location;
	entry.rotation = rotation;
	asset->SetGround(WorldToGround(location, rotation)); // 室内室外的物品都把落点记进Core
	if (AAssetElement* stale = entry.element.Get()) {
		stale->Destroy();
		entry.element = nullptr;
	}
	return SpawnElementFor(asset);
}

AAssetElement* UForeverAssetFrameworkComponent::SpawnThrownAsset(Asset* asset, FVector location,
	FVector velocity, AActor* thrower) {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Player* player = framework ? framework->GetPlayer() : nullptr;
	if (!asset || !player) return nullptr;

	// 先登记进Player::worldAssets(所有权转给Player)，再生成Actor——同名已存在就整个失败，
	// 不留下一个没有归属的Actor。
	if (!player->AddWorldAsset(asset, WorldToGround(location, FRotator::ZeroRotator))) return nullptr;

	AAssetElement* element = SpawnWorldAsset(asset, location);
	if (!element) {
		player->RemoveWorldAsset(asset->GetName()); // 所有权还给调用方
		return nullptr;
	}
	element->Launch(velocity, thrower);
	return element;
}

void UForeverAssetFrameworkComponent::SyncWorldAssetsToCore() {
	// 有实体的物品(室内室外都算)把实际位置朝向写回Core；没有实体的保留上一次写回的位置。
	for (FGroundEntry& entry : groundEntries) WriteBackToCore(entry);
}

void UForeverAssetFrameworkComponent::CollectNearby(FVector center, float radiusCm, TArray<Asset*>& out) const {
	out.Reset();
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Player* player = framework ? framework->GetPlayer() : nullptr;
	if (!player || !GetWorld()) return;

	// AAssetElement的网格用PhysicsActor预设，对象类型是PhysicsBody。
	TArray<FOverlapResult> overlaps;
	GetWorld()->OverlapMultiByObjectType(overlaps, center, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_PhysicsBody), FCollisionShape::MakeSphere(radiusCm));

	const auto& worldAssets = player->GetWorldAssets();
	TArray<TPair<float, Asset*>> found;
	TSet<Asset*> seen;
	for (const FOverlapResult& overlap : overlaps) {
		AAssetElement* element = Cast<AAssetElement>(overlap.GetActor());
		Asset* asset = element ? element->GetAsset() : nullptr;
		if (!asset || seen.Contains(asset)) continue;
		// 只要归Player::worldAssets持有的(房间里丢的物品归Room，不在"附近"列表里)。
		auto it = worldAssets.find(asset->GetName());
		if (it == worldAssets.end() || it->second.asset != asset) continue;
		seen.Add(asset);
		found.Add({ FVector::DistSquared(element->GetActorLocation(), center), asset });
	}

	found.Sort([](const TPair<float, Asset*>& a, const TPair<float, Asset*>& b) { return a.Key < b.Key; });
	for (const auto& entry : found) out.Add(entry.Value);
}

bool UForeverAssetFrameworkComponent::GetCurrentLocation(Asset* asset, FVector& out) const {
	if (!asset) return false;
	if (const int32* found = entryIndexByAsset.Find(asset)) {
		if (const AAssetElement* element = groundEntries[*found].element.Get()) {
			out = element->GetActorLocation();
			return true;
		}
	}
	const AssetGround& ground = asset->GetGround();
	if (ground.restOnBottom) return false; // 布局后还没生成过：z是底面高度不是原点，而且它没动过
	FRotator rotation;
	GroundToWorld(ground, out, rotation);
	return true;
}

void UForeverAssetFrameworkComponent::DetachFromRoom(Asset* asset, Room* room) {
	if (!asset || !room || asset->IsFixture()) return;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Player* player = framework ? framework->GetPlayer() : nullptr;
	if (!player) return;
	auto inRoom = room->GetAssets().find(asset->GetName());
	if (inRoom == room->GetAssets().end() || inRoom->second != asset) return;
	if (player->GetWorldAssets().count(asset->GetName())) return; // 同名已经在室外了：留在房间里
	// 有实体就先把这一刻的实际位置写回Core，Player::AddWorldAsset按它记摆位。
	const int32* found = entryIndexByAsset.Find(asset);
	if (found) WriteBackToCore(groundEntries[*found]);
	room->RemoveAsset(asset->GetName());
	if (!player->AddWorldAsset(asset, asset->GetGround())) {
		room->AddAsset(asset);
		return;
	}
	if (found) {
		FGroundEntry& entry = groundEntries[*found];
		entry.room = nullptr;
		if (AAssetElement* element = entry.element.Get()) element->SetOptionsAllowed(true);
	}
	UE_LOG(LogTemp, Log, TEXT("物品 %s 离开了房间 %s，改为直接放在场景里"), UTF8_TO_TCHAR(asset->GetName().c_str()), UTF8_TO_TCHAR(room->GetName().c_str()));
}

void UForeverAssetFrameworkComponent::DestroyWorldAsset(Asset* asset) {
	if (!asset) return;
	if (const int32* found = entryIndexByAsset.Find(asset)) {
		if (AAssetElement* element = groundEntries[*found].element.Get()) element->Destroy();
		RemoveGroundEntry(asset);
	}
}

AAssetElement* UForeverAssetFrameworkComponent::FindElementByName(const FString& name) const {
	Asset* const* asset = assetByName.Find(name);
	const int32* index = asset ? entryIndexByAsset.Find(*asset) : nullptr;
	return index ? groundEntries[*index].element.Get() : nullptr;
}

namespace {
	// 执行Change的脚本(事件要发回给它)。context.self就是这个Change所在的Script。
	Script* SenderOf(const ScriptContext& context) {
		return dynamic_cast<Script*>(context.self);
	}

	int ParseAmount(const std::string& expression, const ScriptContext& context) {
		if (expression.empty()) return 1;
		return ToInt(EvaluateExpression(expression, context));
	}

	// 地上的资产(房间根或Player::worldAssets)被摘下来之前的位置，失败时原样放回。
	struct FGroundOrigin {
		Room* room = nullptr;
		bool bWorld = false;
	};

	// 在房间根或Player::worldAssets里按名字找，找到就摘下来(不delete)。容器里的、身上的不算“地上”。
	Asset* DetachGroundAsset(AForeverFrameworkActor* framework, const std::string& name, FGroundOrigin& origin) {
		Player* player = framework->GetPlayer();
		if (player) {
			auto it = player->GetWorldAssets().find(name);
			if (it != player->GetWorldAssets().end()) {
				origin.bWorld = true;
				return player->RemoveWorldAsset(name);
			}
		}
		if (Map* map = framework->GetMap()) {
			for (const auto& [buildingName, building] : map->GetBuildings()) {
				if (!building) continue;
				for (Room* room : building->GetRooms()) {
					if (!room) continue;
					auto held = room->GetAssets().find(name);
					if (held != room->GetAssets().end()) {
						if (held->second && held->second->IsFixture()) return nullptr; // 固定物品拿不走、也不能用脚本销毁
						origin.room = room;
						return room->RemoveAsset(name);
					}
				}
			}
		}
		return nullptr;
	}

	void ReattachGroundAsset(AForeverFrameworkActor* framework, Asset* asset, const FGroundOrigin& origin) {
		// 它刚从原处被拿起来，正常一定放得回去；放不回去(原处又出现了同名物品)只能记一笔，物品不再属于任何地方。
		bool restored = true;
		if (origin.bWorld) {
			if (Player* player = framework->GetPlayer()) restored = player->AddWorldAsset(asset, asset->GetGround()); // 摆位一直记在物品自己身上
		}
		else if (origin.room) {
			restored = origin.room->AddAsset(asset);
		}
		if (!restored && asset) {
			UE_LOG(LogTemp, Error, TEXT("物品 %s 放不回原处(那里已经有同名物品)。"), UTF8_TO_TCHAR(asset->GetName().c_str()));
		}
	}
}

namespace {
	// 目标资产名：asset字段非空按表达式求值，否则取执行这个Change的脚本所属资产(脚本变量name)。
	std::string ResolveTargetAssetName(const std::string& assetExpression, const ScriptContext& context) {
		if (!assetExpression.empty()) return ToString(EvaluateExpression(assetExpression, context));
		if (context.self) {
			if (const ValueType* value = context.self->FindValue("name")) return ToString(*value);
		}
		return std::string();
	}
}

void UForeverAssetFrameworkComponent::ApplyPickAsset(const PickAssetChange* change, const ScriptContext& context) {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Player* player = framework ? framework->GetPlayer() : nullptr;
	UForeverStoryFrameworkComponent* story = framework ? framework->GetStoryFramework() : nullptr;
	if (!player || !story) return;

	Script* sender = SenderOf(context);
	std::string name;
	if (!change->GetAsset().empty()) {
		name = ToString(EvaluateExpression(change->GetAsset(), context));
	}
	else if (context.self) {
		// 缺省=执行这个Change的脚本所属的资产
		if (const ValueType* value = context.self->FindValue("name")) name = ToString(*value);
	}

	FGroundOrigin origin;
	Asset* asset = name.empty() ? nullptr : DetachGroundAsset(framework, name, origin);
	if (!asset) {
		UE_LOG(LogTemp, Warning, TEXT("pick_asset: asset \"%s\" is not lying on the ground, skipped."), UTF8_TO_TCHAR(name.c_str()));
		return;
	}

	const Player::StoreResult stored = player->StoreAsset(asset);
	if (stored != Player::StoreResult::Failed) {
		DestroyWorldAsset(asset); // 场景里的Actor
		story->NotifyAssetEvent(asset, /*bGet=*/true, "success", sender);
		// 并进了背包里同类的堆叠：这件资产已经没用了，但执行脚本可能就是它自己的，延迟销毁。
		if (stored == Player::StoreResult::Merged) QueueDelete(asset);
	}
	else {
		ReattachGroundAsset(framework, asset, origin);
		story->NotifyAssetEvent(asset, /*bGet=*/true, "fail", sender);
	}
}

void UForeverAssetFrameworkComponent::ApplyGrantAsset(const GrantAssetChange* change, const ScriptContext& context) {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Player* player = framework ? framework->GetPlayer() : nullptr;
	UForeverStoryFrameworkComponent* story = framework ? framework->GetStoryFramework() : nullptr;
	if (!player || !story) return;

	Script* sender = SenderOf(context);
	std::string type = ToString(EvaluateExpression(change->GetAssetType(), context));
	int amount = ParseAmount(change->GetCount(), context);
	if (amount <= 0) amount = 1;

	Asset* asset = player->CreateAsset(type);
	if (!asset) {
		UE_LOG(LogTemp, Warning, TEXT("grant_asset: asset type \"%s\" is not available."), UTF8_TO_TCHAR(type.c_str()));
		story->NotifyAssetResult(/*bGet=*/true, type, "fail", sender);
		return;
	}
	// 多个作为一个堆叠整体：放不下就整体失败，不会只给一部分。
	asset->SetCount(amount);

	// 指定了slot：只放那个位置(肩膀/背/手/容器路径)，放不了就失败，不退回自动放置。
	if (!change->GetSlot().empty()) {
		const std::string slot = ToString(EvaluateExpression(change->GetSlot(), context));
		if (player->AddByPath(slot, asset)) {
			story->StartAssetScript(asset);
			story->NotifyAssetEvent(asset, /*bGet=*/true, "success", sender);
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("grant_asset: cannot put \"%s\" into slot \"%s\"."), UTF8_TO_TCHAR(type.c_str()), UTF8_TO_TCHAR(slot.c_str()));
			story->NotifyAssetEvent(asset, /*bGet=*/true, "fail", sender);
			player->DestroyAsset(asset);
		}
		return;
	}

	const Player::StoreResult stored = player->StoreAsset(asset);
	if (stored == Player::StoreResult::Merged) {
		story->NotifyAssetEvent(asset, /*bGet=*/true, "success", sender);
		QueueDelete(asset);
	}
	else if (stored != Player::StoreResult::Failed) {
		story->StartAssetScript(asset);
		story->NotifyAssetEvent(asset, /*bGet=*/true, "success", sender);
	}
	else {
		story->NotifyAssetEvent(asset, /*bGet=*/true, "fail", sender);
		player->DestroyAsset(asset);
	}
}

void UForeverAssetFrameworkComponent::ApplyDeductAsset(const DeductAssetChange* change, const ScriptContext& context) {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Player* player = framework ? framework->GetPlayer() : nullptr;
	UForeverStoryFrameworkComponent* story = framework ? framework->GetStoryFramework() : nullptr;
	if (!player || !story) return;

	Script* sender = SenderOf(context);
	std::string type = ToString(EvaluateExpression(change->GetAssetType(), context));
	int amount = ParseAmount(change->GetCount(), context);
	if (amount <= 0) amount = 1;

	std::vector<Asset*> taken;
	if (!player->TakeOwned(type, amount, taken)) {
		story->NotifyAssetResult(/*bGet=*/false, type, "fail", sender);
		return;
	}
	// 先发事件(资产还活着)再销毁。
	for (Asset* asset : taken) story->NotifyAssetEvent(asset, /*bGet=*/false, "success", sender);
	for (Asset* asset : taken) player->DestroyAsset(asset);
}

void UForeverAssetFrameworkComponent::ApplyDropAsset(const DropAssetChange* change, const ScriptContext& context) {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Player* player = framework ? framework->GetPlayer() : nullptr;
	UForeverStoryFrameworkComponent* story = framework ? framework->GetStoryFramework() : nullptr;
	if (!player || !story || !GetWorld()) return;

	Script* sender = SenderOf(context);
	std::string type = ToString(EvaluateExpression(change->GetAssetType(), context));
	int amount = ParseAmount(change->GetCount(), context);
	if (amount <= 0) amount = 1;

	std::vector<Asset*> taken;
	if (!player->TakeOwned(type, amount, taken)) {
		story->NotifyAssetResult(/*bGet=*/false, type, "fail", sender);
		return;
	}

	// 放到玩家附近的地面：在房间里放进房间，否则放进Player::worldAssets，再生成场景物体；多件在身前错开。
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	FVector origin = pawn ? pawn->GetActorLocation() : FVector::ZeroVector;
	FVector forward = pawn ? pawn->GetActorForwardVector() : FVector::ForwardVector;
	FVector right = pawn ? pawn->GetActorRightVector() : FVector::RightVector;
	int index = 0;
	for (Asset* asset : taken) {
		FVector location = origin + forward * 100.f + right * (static_cast<float>(index) - (taken.size() - 1) * 0.5f) * 50.f;
		index++;
		if (Room* room = player->GetCurrentRoom()) {
			room->AddAsset(asset);
		}
		else {
			player->AddWorldAsset(asset, WorldToGround(location, FRotator::ZeroRotator));
		}
		SpawnWorldAsset(asset, location);
		story->NotifyAssetEvent(asset, /*bGet=*/false, "success", sender);
	}
}

bool UForeverAssetFrameworkComponent::ResolvePosition(const std::string& positionExpression, const std::string& assetExpression,
	const ScriptContext& context, FVector& out) const {
	if (!positionExpression.empty()) {
		return FForeverDamage::ParseVec3(ToString(EvaluateExpression(positionExpression, context)), out);
	}
	const std::string name = ResolveTargetAssetName(assetExpression, context);
	if (name.empty() || !GetWorld()) return false;
	const FString key = UTF8_TO_TCHAR(name.c_str());
	if (AAssetElement* element = FindElementByName(key)) {
		out = element->GetActorLocation();
		return true;
	}
	// 物品在地上但实体已经被流式销毁(玩家走远了，比如手雷到点时)：用最后已知位置。
	Asset* const* asset = assetByName.Find(key);
	const int32* index = asset ? entryIndexByAsset.Find(*asset) : nullptr;
	if (!index) return false;
	out = groundEntries[*index].location;
	return true;
}

void UForeverAssetFrameworkComponent::ApplySingleDamage(const SingleDamageChange* change, const ScriptContext& context) {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	if (!framework || !GetWorld() || !framework->GetPopulace() || !framework->GetPopulaceFramework()) return;

	const std::string targetName = ToString(EvaluateExpression(change->GetTarget(), context));
	if (targetName.empty()) return;
	Citizen* citizen = framework->GetPopulace()->FindCitizenByName(targetName);
	if (!citizen) {
		UE_LOG(LogTemp, Warning, TEXT("single_damage: unknown target \"%s\", skipped."), UTF8_TO_TCHAR(targetName.c_str()));
		return;
	}
	// 目标没实例化(没有Actor)就跳过：伤害只对场景里的目标生效。
	ACitizenElement* element = framework->GetPopulaceFramework()->FindActiveCitizen(citizen);
	if (!element) return;

	const float damage = static_cast<float>(ToDouble(EvaluateExpression(change->GetDamage(), context)));
	if (damage <= 0.f) return;
	auto text = [&](const std::string& expression) {
		return expression.empty() ? std::string() : ToString(EvaluateExpression(expression, context));
	};
	FForeverDamage::DamageCitizen(GetWorld(), element, damage, text(change->GetSource()), text(change->GetWeapon()), text(change->GetPart()));
}

void UForeverAssetFrameworkComponent::ApplyAreaDamage(const AreaDamageChange* change, const ScriptContext& context) {
	FVector center;
	if (!ResolvePosition(change->GetPosition(), std::string(), context, center)) {
		UE_LOG(LogTemp, Warning, TEXT("area_damage: no position (position missing/invalid and the script has no world actor), skipped."));
		return;
	}

	auto number = [&](const std::string& expression, double fallback) {
		return expression.empty() ? fallback : ToDouble(EvaluateExpression(expression, context));
	};
	auto text = [&](const std::string& expression) {
		return expression.empty() ? std::string() : ToString(EvaluateExpression(expression, context));
	};
	const bool occlusion = !change->GetOcclusion().empty() && ToBool(EvaluateExpression(change->GetOcclusion(), context));
	FForeverDamage::DamageArea(GetWorld(), center, static_cast<float>(number(change->GetRadius(), 0.0)),
		static_cast<float>(number(change->GetDamage(), 0.0)), occlusion, static_cast<float>(number(change->GetPower(), 1.0)),
		text(change->GetSource()), text(change->GetWeapon()));
}

void UForeverAssetFrameworkComponent::ApplySpawnEffect(const SpawnEffectChange* change, const ScriptContext& context) {
	// 没有默认特效：effect留空=什么都不生成。
	if (change->GetEffect().empty()) return;
	FVector location;
	if (!ResolvePosition(change->GetPosition(), change->GetAsset(), context, location)) {
		UE_LOG(LogTemp, Warning, TEXT("spawn_effect: no position (position invalid, or the asset has no actor in the world: picked up / destroyed?), skipped."));
		return;
	}
	FVector normal = FVector::UpVector;
	if (!change->GetNormal().empty()) {
		FVector parsed;
		if (FForeverDamage::ParseVec3(ToString(EvaluateExpression(change->GetNormal(), context)), parsed) && !parsed.IsNearlyZero()) {
			normal = parsed.GetSafeNormal();
		}
	}

	float scale = 1.f;
	if (!change->GetScale().empty()) scale = static_cast<float>(ToDouble(EvaluateExpression(change->GetScale(), context)));
	if (scale <= 0.f) scale = 1.f;
	const FString effect = UTF8_TO_TCHAR(ToString(EvaluateExpression(change->GetEffect(), context)).c_str());
	if (effect.IsEmpty()) return;

	AForeverExplosionEffect::Spawn(GetWorld(), location, normal, scale, effect);
}

void UForeverAssetFrameworkComponent::ApplySoundEffect(const SoundEffectChange* change, const ScriptContext& context) {
	if (change->GetSound().empty()) return;
	FVector location;
	if (!ResolvePosition(change->GetPosition(), change->GetAsset(), context, location)) {
		UE_LOG(LogTemp, Warning, TEXT("sound_effect: no position (position invalid, or the asset has no actor in the world), skipped."));
		return;
	}
	FString path = UTF8_TO_TCHAR(ToString(EvaluateExpression(change->GetSound(), context)).c_str());
	if (path.IsEmpty()) return;
	// 只写包路径时补上对象名(<名>)，同spawn_effect/spawn_actor对蓝图类路径的处理。
	if (!path.Contains(TEXT("."))) path += TEXT(".") + FPackageName::GetShortName(path);
	USoundBase* sound = LoadObject<USoundBase>(nullptr, *path);
	if (!sound) {
		UE_LOG(LogTemp, Warning, TEXT("sound_effect: failed to load sound %s, skipped."), *path);
		return;
	}
	float volume = 1.f;
	if (!change->GetVolume().empty()) volume = static_cast<float>(ToDouble(EvaluateExpression(change->GetVolume(), context)));
	float pitch = 1.f;
	if (!change->GetPitch().empty()) pitch = static_cast<float>(ToDouble(EvaluateExpression(change->GetPitch(), context)));
	UGameplayStatics::PlaySoundAtLocation(GetWorld(), sound, location, FMath::Max(volume, 0.f), pitch > 0.f ? pitch : 1.f);
}

void UForeverAssetFrameworkComponent::ApplyDestroyAsset(const DestroyAssetChange* change, const ScriptContext& context) {
	const std::string name = ResolveTargetAssetName(change->GetAsset(), context);
	if (name.empty()) {
		UE_LOG(LogTemp, Warning, TEXT("destroy_asset: no asset name, skipped."));
		return;
	}
	if (std::find(pendingDestroy.begin(), pendingDestroy.end(), name) != pendingDestroy.end()) return;
	pendingDestroy.push_back(name);

	// 先藏起来、关掉碰撞：视觉上立刻消失，真正的销毁在FlushPendingDestroy。
	if (AAssetElement* element = FindElementByName(UTF8_TO_TCHAR(name.c_str()))) {
		element->SetActorHiddenInGame(true);
		element->SetActorEnableCollision(false);
	}
}

void UForeverAssetFrameworkComponent::QueueDelete(Asset* asset) {
	if (asset) pendingDelete.push_back(asset);
}

void UForeverAssetFrameworkComponent::FlushPendingDestroy() {
	if (!pendingDelete.empty()) {
		AForeverFrameworkActor* owner = Cast<AForeverFrameworkActor>(GetOwner());
		Player* ownerPlayer = owner ? owner->GetPlayer() : nullptr;
		std::vector<Asset*> doomed;
		doomed.swap(pendingDelete);
		for (Asset* asset : doomed) {
			if (ownerPlayer) ownerPlayer->DestroyAsset(asset);
		}
	}
	if (pendingDestroy.empty()) return;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Player* player = framework ? framework->GetPlayer() : nullptr;
	std::vector<std::string> names;
	names.swap(pendingDestroy);
	if (!player) return;

	for (const std::string& name : names) {
		FGroundOrigin origin;
		Asset* asset = DetachGroundAsset(framework, name, origin);
		if (!asset) {
			UE_LOG(LogTemp, Warning, TEXT("destroy_asset: asset \"%s\" is not lying on the ground, skipped."), UTF8_TO_TCHAR(name.c_str()));
			continue;
		}
		DestroyWorldAsset(asset); // 场景里的Actor
		player->DestroyAsset(asset); // Asset本身(连带脚本)
	}
}

void UForeverAssetFrameworkComponent::ApplyAddSatiety(const AddSatietyChange* change, const ScriptContext& context) {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	if (!framework) return;

	// target留空=玩家当前操控的市民；写了姓名就按姓名找(不要求已实例化，饱食度是Core数据)。
	Citizen* citizen = nullptr;
	const std::string targetName = change->GetTarget().empty() ? std::string() : ToString(EvaluateExpression(change->GetTarget(), context));
	if (targetName.empty()) {
		citizen = framework->GetControlledCitizen();
	} else if (framework->GetPopulace()) {
		citizen = framework->GetPopulace()->FindCitizenByName(targetName);
	}
	if (!citizen) {
		UE_LOG(LogTemp, Warning, TEXT("add_satiety: no target citizen (\"%s\"), skipped."), UTF8_TO_TCHAR(targetName.c_str()));
		return;
	}
	const float amount = static_cast<float>(ToDouble(EvaluateExpression(change->GetAmount(), context)));
	citizen->ActivateStats(); // 未实例化动态属性则先实例化
	citizen->AddSatiety(amount);
}
