#include "Player/ForeverThrowableComponent.h"

#include "Element/AssetElement.h"
#include "Framework/ForeverAssetFrameworkComponent.h"
#include "Framework/ForeverFrameworkActor.h"
#include "Player/ForeverCharacter.h"
#include "Player/ForeverWeaponComponent.h"

#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"

#include "player/asset.h"
#include "player/player.h"

#include <algorithm>

UForeverThrowableComponent::UForeverThrowableComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UForeverThrowableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (previewMesh) {
		previewMesh->DestroyComponent();
		previewMesh = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

AForeverCharacter* UForeverThrowableComponent::GetCharacter() const
{
	return Cast<AForeverCharacter>(GetOwner());
}

Player* UForeverThrowableComponent::GetPlayer() const
{
	if (AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()))) {
		return framework->GetPlayer();
	}
	return nullptr;
}

int32 UForeverThrowableComponent::IndexOfType(const std::string& type) const
{
	for (size_t i = 0; i < entries.size(); i++) {
		if (entries[i].type == type) return static_cast<int32>(i);
	}
	return -1;
}

void UForeverThrowableComponent::Refresh()
{
	Player* player = GetPlayer();
	if (!player) return;

	// 激活的那个用完时，原来排在它后面的会顶到同一个下标——用旧下标取"下一个"。
	int32 oldIndex = IsActive() ? IndexOfType(activeType) : -1;

	std::vector<Player::ThrowableEntry> fresh = player->CollectThrowables();
	// 新出现的type按扫描顺序分配新序号；已有的保持原序号，所以排序结果稳定。
	for (const Player::ThrowableEntry& entry : fresh) {
		if (!rankByType.count(entry.type)) rankByType[entry.type] = nextRank++;
	}
	std::sort(fresh.begin(), fresh.end(), [this](const Player::ThrowableEntry& a, const Player::ThrowableEntry& b) {
		return rankByType[a.type] < rankByType[b.type];
	});
	entries.clear();
	for (const Player::ThrowableEntry& entry : fresh) {
		entries.push_back({ entry.type, entry.count, entry.meshPath, entry.meshScale });
	}

	if (!IsActive() || IndexOfType(activeType) >= 0) return;

	// 激活的投掷物用完了：自动切到下一个，一个都没有了才退出投掷状态。
	if (entries.empty()) {
		Deactivate();
		if (AForeverCharacter* character = GetCharacter()) {
			if (character->IsAiming()) character->StopAim();
		}
		ShowNoThrowableMessage();
		return;
	}
	int32 next = oldIndex >= 0 ? oldIndex % static_cast<int32>(entries.size()) : 0;
	ActivateType(entries[next].type);
}

TArray<FThrowListItem> UForeverThrowableComponent::GetThrowableList()
{
	Refresh();
	TArray<FThrowListItem> list;
	for (const FThrowableEntry& entry : entries) {
		FThrowListItem item;
		item.type = UTF8_TO_TCHAR(entry.type.c_str());
		item.count = entry.count;
		list.Add(item);
	}
	return list;
}

FString UForeverThrowableComponent::GetActiveType() const
{
	return UTF8_TO_TCHAR(activeType.c_str());
}

bool UForeverThrowableComponent::ActivateByType(const FString& type)
{
	Refresh();
	std::string typeUtf8 = TCHAR_TO_UTF8(*type);
	if (IndexOfType(typeUtf8) < 0) return false;
	ActivateType(typeUtf8);
	return true;
}

void UForeverThrowableComponent::CycleThrowable()
{
	Refresh();
	if (entries.empty()) {
		Deactivate();
		ShowNoThrowableMessage();
		return;
	}

	int32 current = IsActive() ? IndexOfType(activeType) : -1;
	int32 next = (current + 1) % static_cast<int32>(entries.size()); // current=-1 时取第0个
	ActivateType(entries[next].type);
}

void UForeverThrowableComponent::ActivateType(const std::string& type)
{
	AForeverCharacter* character = GetCharacter();
	if (!character) return;

	activeType = type;
	// 同一时刻手上要么是肩上的武器要么是投掷物，激活投掷物就让武器空下来。
	if (UForeverWeaponComponent* weapon = character->GetWeaponComponent()) weapon->ClearWeapon();

	// 正在瞄准(原来拿着武器瞄准)但是手不够投掷：取消瞄准，仍然保持激活。
	if (character->IsAiming() && !CanThrow()) character->StopAim();

	ShowActiveMessage();
}

void UForeverThrowableComponent::Deactivate()
{
	activeType.clear();
	HidePreview();
}

bool UForeverThrowableComponent::CanThrow() const
{
	Player* player = GetPlayer();
	if (!player || !IsActive()) return false;
	return player->HasThrowableInHand(activeType) || !player->GetLeftHand() || !player->GetRightHand();
}

void UForeverThrowableComponent::ShowActiveMessage() const
{
	if (!GEngine) return;
	int32 index = IndexOfType(activeType);
	int count = index >= 0 ? entries[index].count : 0;
	// 和武器名共用同一个Key(201)，切换武器/投掷物时同一行互相覆盖。
	GEngine->AddOnScreenDebugMessage(201, 3.f, FColor::Cyan,
		FString::Printf(TEXT("当前投掷物: %s x%d"), UTF8_TO_TCHAR(activeType.c_str()), count));
}

void UForeverThrowableComponent::ShowNoThrowableMessage() const
{
	if (GEngine) {
		GEngine->AddOnScreenDebugMessage(201, 3.f, FColor::Cyan, TEXT("当前没有投掷物"));
	}
}

void UForeverThrowableComponent::ComputeLaunch(FVector& outStart, FVector& outVelocity) const
{
	AForeverCharacter* character = GetCharacter();
	FRotator control = character->GetControlRotation();

	// 起点只按偏航角转(角色本地偏移)，初速方向用完整的视线朝向再抬一个仰角。
	outStart = character->GetActorLocation() + FRotator(0.f, control.Yaw, 0.f).RotateVector(throwOffset);
	outVelocity = FRotator(control.Pitch + throwElevation, control.Yaw, 0.f).Vector() * throwSpeed;
}

void UForeverThrowableComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AForeverCharacter* character = GetCharacter();
	if (!character || !IsActive() || !character->IsAiming()) {
		if (previewMesh) HidePreview();
		return;
	}
	UpdatePreview();
}

void UForeverThrowableComponent::UpdatePreview()
{
	FVector start, velocity;
	ComputeLaunch(start, velocity);

	// 预览曲线只用来显示，真实投掷由物理模拟决定。投掷物是半径5cm左右的小球。
	FPredictProjectilePathParams params(5.f, start, velocity, previewDuration);
	params.bTraceWithCollision = true;
	params.bTraceWithChannel = true;
	params.TraceChannel = ECC_Pawn; // 项目的Pawn预设忽略Visibility，见CONVENTIONS.md第11节
	params.SimFrequency = 15.f;
	params.ActorsToIgnore.Add(GetOwner());
	FPredictProjectilePathResult result;
	UGameplayStatics::PredictProjectilePath(GetWorld(), params, result);

	// 曲线画成世界里的调试线段：和场景一起做深度测试，被墙/地形挡住的部分自然看不见(屏幕
	// 空间的HUD线条没有遮挡关系)。每帧重画一次(LifeTime<0=只显示这一帧)。注意DrawDebug
	// 在Shipping/Test构建里会被编译掉，和武器子弹轨迹的DrawDebugLine同样的限制。
	TArray<FVector> points;
	for (const FPredictProjectilePathPointData& point : result.PathData) points.Add(point.Location);
	if (result.HitResult.bBlockingHit) points.Add(result.HitResult.Location);
	for (int32 i = 1; i < points.Num(); i++) {
		DrawDebugLine(GetWorld(), points[i - 1], points[i], FColor::White, false, -1.f, SDPG_World, 1.5f);
	}
	if (result.HitResult.bBlockingHit) {
		DrawDebugSphere(GetWorld(), result.HitResult.Location, 8.f, 8, FColor::White, false, -1.f, SDPG_World, 1.f);
	}

	// 起点模型：和掉落物同一套网格/缩放。
	int32 index = IndexOfType(activeType);
	const std::string meshPath = index >= 0 ? entries[index].meshPath : std::string();
	float meshScale = index >= 0 ? entries[index].meshScale : 1.f;

	bool bNewMesh = false;
	if (!previewMesh) {
		previewMesh = NewObject<UStaticMeshComponent>(GetOwner(), NAME_None, RF_Transient);
		previewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		previewMesh->SetCastShadow(false);
		previewMesh->SetupAttachment(GetOwner()->GetRootComponent());
		previewMesh->RegisterComponent();
		bNewMesh = true;
	}
	if (bNewMesh || previewMeshPath != meshPath) {
		UStaticMesh* mesh = AAssetElement::LoadAssetMesh(meshPath);
		if (!mesh) mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Asset/Meshes/Cube.Cube"));
		previewMesh->SetStaticMesh(mesh);
		// 没指定meshPath时落地的是100cm的Cube，预览不跟着放那么大，缩到手雷大小。
		previewMesh->SetWorldScale3D(FVector(meshPath.empty() ? 0.1f : meshScale));
		previewMeshPath = meshPath;
	}
	previewMesh->SetWorldLocation(start);
	previewMesh->SetVisibility(true);
}

void UForeverThrowableComponent::HidePreview()
{
	if (previewMesh) previewMesh->SetVisibility(false);
}

void UForeverThrowableComponent::Throw()
{
	AForeverCharacter* character = GetCharacter();
	if (!character || !IsActive() || !character->IsAiming()) return;

	// 背包没有变化事件，投掷前重新核对一次——可能已经被背包界面拿走/用完了。
	Refresh();
	if (!IsActive()) return;
	if (!CanThrow()) {
		character->StopAim();
		return;
	}

	Player* player = GetPlayer();
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()));
	UForeverAssetFrameworkComponent* assetFramework = framework ? framework->GetAssetFramework() : nullptr;
	if (!player || !assetFramework) return;

	Asset* thrown = player->TakeOneThrowable(activeType);
	if (!thrown) {
		Refresh();
		return;
	}

	FVector start, velocity;
	ComputeLaunch(start, velocity);
	if (!assetFramework->SpawnThrownAsset(thrown, start, velocity, GetOwner())) {
		player->DestroyAsset(thrown); // 没登记成功，Asset没人接手，自己释放
	}

	Refresh();
	if (IsActive()) ShowActiveMessage();
}
