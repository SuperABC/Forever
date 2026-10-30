#include "Framework/ForeverAssetFrameworkComponent.h"

#include "Element/AssetElement.h"
#include "Framework/ForeverFrameworkActor.h"

#include "Framework/ForeverStoryFrameworkComponent.h"

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

#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"

AAssetElement* UForeverAssetFrameworkComponent::SpawnWorldAsset(Asset* asset, FVector location, FRotator rotation) {
	if (!asset || !GetWorld()) return nullptr;

	// 延迟生成：先Init换好网格、并把meshScale写进Transform，再FinishSpawning——物理体在
	// 那时才按最终的网格和缩放创建，不会先按Cube创建再换。没指定meshPath的资产保持原来的
	// Cube、缩放1。
	float scale = asset->GetMeshPath().empty() ? 1.f : asset->GetMeshScale();
	FTransform transform(rotation, location, FVector(scale));
	AAssetElement* element = GetWorld()->SpawnActorDeferred<AAssetElement>(
		AAssetElement::StaticClass(), transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!element) return nullptr;

	// 带脚本的资产第一次变成场景物体时补发game_start(脚本里一般在这时add_option)，要在Init烘焙选项快照之前。
	if (AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner())) {
		if (framework->GetStoryFramework()) framework->GetStoryFramework()->StartAssetScript(asset);
	}
	element->Init(asset);
	element->FinishSpawning(transform);
	worldAssetActors.Add(asset, element);
	return element;
}

AAssetElement* UForeverAssetFrameworkComponent::SpawnThrownAsset(Asset* asset, FVector location,
	FVector velocity, AActor* thrower) {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Player* player = framework ? framework->GetPlayer() : nullptr;
	if (!asset || !player) return nullptr;

	// 先登记进Player::worldAssets(所有权转给Player)，再生成Actor——同名已存在就整个失败，
	// 不留下一个没有归属的Actor。
	Player::WorldAssetTransform transform;
	transform.x = location.X;
	transform.y = location.Y;
	transform.z = location.Z;
	if (!player->AddWorldAsset(asset, transform)) return nullptr;

	AAssetElement* element = SpawnWorldAsset(asset, location);
	if (!element) {
		player->RemoveWorldAsset(asset->GetName()); // 所有权还给调用方
		return nullptr;
	}
	element->Launch(velocity, thrower);
	return element;
}

void UForeverAssetFrameworkComponent::SyncWorldAssetsToCore() {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Player* player = framework ? framework->GetPlayer() : nullptr;
	if (!player) return;

	for (const auto& [name, entry] : player->GetWorldAssets()) {
		AAssetElement** found = worldAssetActors.Find(entry.asset);
		if (!found || !IsValid(*found)) continue; // 没有对应的Actor就保留上一次记的位置

		FVector location = (*found)->GetActorLocation();
		FRotator rotation = (*found)->GetActorRotation();
		Player::WorldAssetTransform transform;
		transform.x = location.X;
		transform.y = location.Y;
		transform.z = location.Z;
		transform.pitch = rotation.Pitch;
		transform.yaw = rotation.Yaw;
		transform.roll = rotation.Roll;
		player->SetWorldAssetTransform(name, transform); // 只改value，不增删，遍历中安全
	}
}

void UForeverAssetFrameworkComponent::RestoreWorldAssets() {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Player* player = framework ? framework->GetPlayer() : nullptr;
	if (!player) return;

	for (const auto& [name, entry] : player->GetWorldAssets()) {
		if (worldAssetActors.Contains(entry.asset)) continue;
		const Player::WorldAssetTransform& t = entry.transform;
		SpawnWorldAsset(entry.asset, FVector(t.x, t.y, t.z), FRotator(t.pitch, t.yaw, t.roll));
	}
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

void UForeverAssetFrameworkComponent::DestroyWorldAsset(Asset* asset) {
	if (!asset) return;

	if (AAssetElement** found = worldAssetActors.Find(asset)) {
		if (*found) (*found)->Destroy();
		worldAssetActors.Remove(asset);
	}
}

AAssetElement* UForeverAssetFrameworkComponent::FindElementByName(const FString& name) const {
	std::string nameUtf8 = TCHAR_TO_UTF8(*name);
	for (const TPair<Asset*, AAssetElement*>& pair : worldAssetActors) {
		if (pair.Key && pair.Value && pair.Key->GetName() == nameUtf8) return pair.Value;
	}
	return nullptr;
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
		Player::WorldAsset world;
		bool bWorld = false;
	};

	// 在房间根或Player::worldAssets里按名字找，找到就摘下来(不delete)。容器里的、身上的不算“地上”。
	Asset* DetachGroundAsset(AForeverFrameworkActor* framework, const std::string& name, FGroundOrigin& origin) {
		Player* player = framework->GetPlayer();
		if (player) {
			auto it = player->GetWorldAssets().find(name);
			if (it != player->GetWorldAssets().end()) {
				origin.bWorld = true;
				origin.world = it->second;
				return player->RemoveWorldAsset(name);
			}
		}
		if (Map* map = framework->GetMap()) {
			for (const auto& [buildingName, building] : map->GetBuildings()) {
				if (!building) continue;
				for (Room* room : building->GetRooms()) {
					if (!room) continue;
					if (room->GetAssets().count(name)) {
						origin.room = room;
						return room->RemoveAsset(name);
					}
				}
			}
		}
		return nullptr;
	}

	void ReattachGroundAsset(AForeverFrameworkActor* framework, Asset* asset, const FGroundOrigin& origin) {
		if (origin.bWorld) {
			if (Player* player = framework->GetPlayer()) player->AddWorldAsset(asset, origin.world.transform);
		}
		else if (origin.room) {
			origin.room->AddAsset(asset);
		}
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
		auto [found, value] = context.self->GetValue("name"); // 缺省=执行这个Change的脚本所属的资产
		if (found) name = ToString(value);
	}

	FGroundOrigin origin;
	Asset* asset = name.empty() ? nullptr : DetachGroundAsset(framework, name, origin);
	if (!asset) {
		UE_LOG(LogTemp, Warning, TEXT("pick_asset: asset \"%s\" is not lying on the ground, skipped."), UTF8_TO_TCHAR(name.c_str()));
		return;
	}

	if (player->StoreAsset(asset) != Player::StoreResult::Failed) {
		DestroyWorldAsset(asset); // 场景里的Actor
		story->NotifyAssetEvent(asset, /*bGet=*/true, "success", sender);
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

	if (player->StoreAsset(asset) != Player::StoreResult::Failed) {
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
			Player::WorldAssetTransform transform;
			transform.x = location.X;
			transform.y = location.Y;
			transform.z = location.Z;
			player->AddWorldAsset(asset, transform);
		}
		SpawnWorldAsset(asset, location);
		story->NotifyAssetEvent(asset, /*bGet=*/false, "success", sender);
	}
}
