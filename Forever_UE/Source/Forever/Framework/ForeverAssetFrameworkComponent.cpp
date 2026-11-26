#include "Framework/ForeverAssetFrameworkComponent.h"

#include "Element/AssetElement.h"
#include "Framework/ForeverFrameworkActor.h"

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
		// 它刚从原处被拿起来，正常一定放得回去；放不回去(原处又出现了同名物品)只能记一笔，物品不再属于任何地方。
		bool restored = true;
		if (origin.bWorld) {
			if (Player* player = framework->GetPlayer()) restored = player->AddWorldAsset(asset, origin.world.transform);
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

bool UForeverAssetFrameworkComponent::ResolvePosition(const std::string& positionExpression, const std::string& assetExpression,
	const ScriptContext& context, FVector& out) const {
	if (!positionExpression.empty()) {
		return FForeverDamage::ParseVec3(ToString(EvaluateExpression(positionExpression, context)), out);
	}
	const std::string name = ResolveTargetAssetName(assetExpression, context);
	AAssetElement* element = name.empty() || !GetWorld() ? nullptr : FindElementByName(UTF8_TO_TCHAR(name.c_str()));
	if (!element) return false;
	out = element->GetActorLocation();
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
