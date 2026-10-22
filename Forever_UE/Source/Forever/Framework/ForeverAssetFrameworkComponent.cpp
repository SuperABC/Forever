#include "Framework/ForeverAssetFrameworkComponent.h"

#include "Element/AssetElement.h"
#include "Framework/ForeverFrameworkActor.h"

#include "player/player.h"
#include "player/asset.h"

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
