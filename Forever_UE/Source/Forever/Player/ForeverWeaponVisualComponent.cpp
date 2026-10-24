#include "Player/ForeverWeaponVisualComponent.h"

#include "Player/ForeverCharacter.h"
#include "Player/ForeverWeaponComponent.h"
#include "Framework/ForeverFrameworkActor.h"

#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

#include "common/registry.h"
#include "player/asset.h"
#include "player/player.h"
#include "player/weapon.h"

UForeverWeaponVisualComponent::UForeverWeaponVisualComponent() {
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // BeginShowing()才打开，市民很多时不白跑
	PrimaryComponentTick.TickGroup = TG_PostPhysics;     // 角色这一帧移动完之后再摆
}

void UForeverWeaponVisualComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	EndShowing();
	Super::EndPlay(EndPlayReason);
}

void UForeverWeaponVisualComponent::BeginShowing() {
	framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()));
	bShowing = true;
	bTrackActivated = false;
	activatedAsset = nullptr;
	SetComponentTickEnabled(true);
}

void UForeverWeaponVisualComponent::EndShowing() {
	bShowing = false;
	bTrackActivated = false;
	activatedAsset = nullptr;
	for (int32 i = 0; i < 2; ++i) DestroySlot(i);
	SetComponentTickEnabled(false);
}

void UForeverWeaponVisualComponent::NotifyActivated(int32 slot, Asset* asset) {
	if (slot != 1 && slot != 2) return;
	bTrackActivated = true;
	activatedAsset = asset;
}

Asset* UForeverWeaponVisualComponent::GetShoulderAsset(int32 slot) const {
	// 目前只有被玩家占有的角色有肩膀装备(Player的肩膀是全局的)。以后市民有自己的装备时改这里。
	AForeverCharacter* character = Cast<AForeverCharacter>(GetOwner());
	AForeverFrameworkActor* frameworkActor = framework.Get();
	if (!character || !character->IsPlayerControlled() || !frameworkActor) return nullptr;
	Player* player = frameworkActor->GetPlayer();
	if (!player) return nullptr;
	return slot == 1 ? player->GetLeftShoulder() : player->GetRightShoulder();
}

const UForeverWeaponVisualComponent::FWeaponVisualDef* UForeverWeaponVisualComponent::FindDef(const FString& weaponType) {
	FWeaponVisualDef* cached = defCache.Find(weaponType);
	if (!cached) {
		FWeaponVisualDef def;
		// 临时构造一个Core Weapon只为读WeaponMod的字段，读完就释放(析构自己走Factory销毁mod)。
		Weapon weapon(&Registry::Get().GetWeaponFactory(), std::string(TCHAR_TO_UTF8(*weaponType)));
		if (weapon.IsValid()) {
			def.path = UTF8_TO_TCHAR(weapon.GetFirstPersonMeshPath().c_str());
			def.aimOffset = FVector(weapon.GetAttachOffsetX(), weapon.GetAttachOffsetY(), weapon.GetAttachOffsetZ());
			def.backOffset = FVector(weapon.GetBackOffsetX(), weapon.GetBackOffsetY(), weapon.GetBackOffsetZ());
			def.modelScale = weapon.GetModelScale();
			def.anchor = FVector(weapon.GetAnchorOffsetX(), weapon.GetAnchorOffsetY(), weapon.GetAnchorOffsetZ());
			def.muzzle = FVector(weapon.GetMuzzleOffsetX(), weapon.GetMuzzleOffsetY(), weapon.GetMuzzleOffsetZ());
		}
		if (def.path.IsEmpty()) {
			UE_LOG(LogTemp, Warning, TEXT("UForeverWeaponVisualComponent: weapon %s has no model path (firstPersonMeshPath), nothing to show."), *weaponType);
		}
		cached = &defCache.Add(weaponType, def);
	}
	return cached->path.IsEmpty() ? nullptr : cached;
}

AActor* UForeverWeaponVisualComponent::SpawnVisualActor(const FString& path, float scale) {
	UWorld* world = GetWorld();
	AActor* owner = GetOwner();
	if (!world || !owner) return nullptr;

	FActorSpawnParameters params;
	params.Owner = owner;
	params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FTransform transform(FRotator::ZeroRotator, owner->GetActorLocation(), FVector(scale));

	AActor* actor = nullptr;
	if (path.EndsWith(TEXT("_C"))) {
		// Actor Blueprint类(整件武器：枪体+弹匣的相对位置都在蓝图里)。
		UClass* actorClass = LoadClass<AActor>(nullptr, *path);
		if (!actorClass) {
			UE_LOG(LogTemp, Warning, TEXT("UForeverWeaponVisualComponent: failed to load weapon blueprint class %s."), *path);
			return nullptr;
		}
		actor = world->SpawnActor<AActor>(actorClass, transform, params);
	} else {
		// 单个StaticMesh。
		UStaticMesh* mesh = LoadObject<UStaticMesh>(nullptr, *path);
		if (!mesh) {
			UE_LOG(LogTemp, Warning, TEXT("UForeverWeaponVisualComponent: failed to load weapon mesh %s."), *path);
			return nullptr;
		}
		AStaticMeshActor* meshActor = world->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), transform, params);
		if (meshActor) {
			meshActor->SetMobility(EComponentMobility::Movable);
			meshActor->GetStaticMeshComponent()->SetStaticMesh(mesh);
		}
		actor = meshActor;
	}
	if (!actor) return nullptr;

	actor->SetActorEnableCollision(false);
	actor->SetActorTickEnabled(false);
	return actor;
}

void UForeverWeaponVisualComponent::DestroySlot(int32 index) {
	FSlotVisual& visual = slots[index];
	if (AActor* actor = visual.actor.Get()) actor->Destroy();
	visual = FSlotVisual();
}

void UForeverWeaponVisualComponent::RebuildSlot(int32 index, Asset* asset) {
	DestroySlot(index);
	slots[index].asset = asset;
	if (!asset) return;

	const FWeaponVisualDef* def = FindDef(UTF8_TO_TCHAR(asset->GetWeaponType().c_str()));
	if (!def) return;

	AActor* actor = SpawnVisualActor(def->path, def->modelScale);
	if (!actor) return;

	// 两个锚点都是枪托=模型原点，不需要算包围盒；只打一行缩放后的实测尺寸，调缩放时对照。
	FBox box = actor->GetComponentsBoundingBox(/*bNonColliding=*/true).ShiftBy(-actor->GetActorLocation());
	UE_LOG(LogTemp, Log, TEXT("UForeverWeaponVisualComponent: %s scaled model size %.1f x %.1f x %.1f cm, x from %.1f to %.1f."),
		*def->path, box.GetSize().X, box.GetSize().Y, box.GetSize().Z, box.Min.X, box.Max.X);

	FSlotVisual& visual = slots[index];
	visual.actor = actor;
	visual.muzzleLocal = def->muzzle; // 枪口就是WeaponMod::muzzleOffset，不从模型推算
	visual.modelScale = def->modelScale;
	visual.anchor = def->anchor;
	visual.aimOffset = def->aimOffset;
	// 背枪锚点写的是右肩的位置，左肩(槽位0)上的枪左右对称，Y取反。
	visual.backOffset = def->backOffset;
	if (index == 0) visual.backOffset.Y = -visual.backOffset.Y;
	visual.pose = EPose::None;
}

bool UForeverWeaponVisualComponent::GetActiveMuzzleWorldLocation(FVector& OutLocation) const {
	AForeverCharacter* character = Cast<AForeverCharacter>(GetOwner());
	int32 activeSlot = character ? character->GetActiveShoulderSlot() : 0;
	if (activeSlot != 1 && activeSlot != 2) return false;

	const FSlotVisual& visual = slots[activeSlot - 1];
	const AActor* actor = visual.actor.Get();
	if (!actor) return false;
	// muzzleLocal是缩放后的局部坐标，所以只需要旋转、不再乘缩放。
	OutLocation = actor->GetActorLocation() + actor->GetActorRotation().RotateVector(visual.muzzleLocal);
	return true;
}

void UForeverWeaponVisualComponent::SetHiddenForOwner(FSlotVisual& visual, bool bHide) {
	AActor* actor = visual.actor.Get();
	if (!actor || visual.bHiddenForOwner == bHide) return;
	visual.bHiddenForOwner = bHide;
	actor->ForEachComponent<UPrimitiveComponent>(/*bIncludeFromChildActors=*/false, [bHide](UPrimitiveComponent* component) {
		component->SetOwnerNoSee(bHide);
	});
}

void UForeverWeaponVisualComponent::ApplyHolsterPose(int32 index) {
	FSlotVisual& visual = slots[index];
	AActor* actor = visual.actor.Get();
	ACharacter* character = Cast<ACharacter>(GetOwner());
	if (!actor || !character) return;

	FVector offset = visual.backOffset;
	FRotator rotation = index == 0 ? holsterRotationLeft : holsterRotationRight;
	FQuat desired = rotation.Quaternion();

	// 背在骨骼上：offset/rotation是"角色朝向下的直观坐标"，要按骨骼参考姿势的朝向转成骨骼局部
	// (和AForeverCharacter::PlaceAvatar同一个办法)。找不到骨骼就退回挂在角色根上。
	USkeletalMeshComponent* mesh = character->GetMesh();
	USkeletalMesh* skeletalMesh = mesh ? mesh->GetSkeletalMeshAsset() : nullptr;
	int32 boneIndex = skeletalMesh ? skeletalMesh->GetRefSkeleton().FindBoneIndex(holsterBone) : INDEX_NONE;
	if (boneIndex != INDEX_NONE) {
		FQuat boneInComponent = FAnimationRuntime::GetComponentSpaceTransformRefPose(skeletalMesh->GetRefSkeleton(), boneIndex).GetRotation();
		FQuat boneInActor = mesh->GetRelativeRotation().Quaternion() * boneInComponent;
		actor->AttachToComponent(mesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, holsterBone);
		FQuat relativeRotation = boneInActor.Inverse() * desired;
		actor->SetActorRelativeRotation(relativeRotation);
		// 锚点(模型自己坐标系里的visual.anchor)放到offset：actor原点 = 目标位置 - 旋转后的锚点偏移。
		actor->SetActorRelativeLocation(boneInActor.Inverse().RotateVector(offset) - relativeRotation.RotateVector(visual.anchor));
	} else {
		UE_LOG(LogTemp, Warning, TEXT("UForeverWeaponVisualComponent: holster bone %s not found, attaching to the capsule."), *holsterBone.ToString());
		actor->AttachToComponent(character->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		actor->SetActorRelativeRotation(desired);
		actor->SetActorRelativeLocation(offset - desired.RotateVector(visual.anchor));
	}
	actor->SetActorRelativeScale3D(FVector(visual.modelScale));
	visual.pose = EPose::Holster;
}

void UForeverWeaponVisualComponent::ApplyAimPose(int32 index) {
	FSlotVisual& visual = slots[index];
	AActor* actor = visual.actor.Get();
	AForeverCharacter* character = Cast<AForeverCharacter>(GetOwner());
	if (!actor || !character) return;

	if (visual.pose != EPose::Aim) {
		actor->AttachToComponent(character->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		actor->SetActorRelativeScale3D(FVector(visual.modelScale));
		visual.pose = EPose::Aim;
	}

	// 瞄准时角色偏航已经跟着摄像机(见UpdateRotationMode)，只需要补俯仰。枪托锚点(模型原点加
	// anchorOffset)固定在角色本地的aimOffset(右手靠前)，枪体从枪托向前延伸，绕枪托俯仰——枪不会向后穿进身体。
	float pitch = FRotator::NormalizeAxis(character->GetControlRotation().Pitch);
	FQuat rotation = FRotator(pitch, 0.f, 0.f).Quaternion();
	actor->SetActorRelativeRotation(rotation);
	actor->SetActorRelativeLocation(visual.aimOffset - rotation.RotateVector(visual.anchor));
}

void UForeverWeaponVisualComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AForeverCharacter* character = Cast<AForeverCharacter>(GetOwner());
	if (!bShowing || !character) return;

	// 1) 模型跟肩膀槽位走：槽位里的Asset变了(装备/拖走/换)就重建，空了就删。背包没有变化事件，
	//    两个指针比较足够便宜，不需要事件。
	for (int32 i = 0; i < 2; ++i) {
		Asset* asset = GetShoulderAsset(i + 1);
		if (asset != slots[i].asset || (asset && !slots[i].actor.IsValid())) RebuildSlot(i, asset);
	}

	// 2) 手上正在用的武器(能开火的那份)跟激活肩膀走：激活槽位里的Asset变了就重新激活
	//    (空=卸下，有枪=装备)。投掷物激活时槽位没变，不会误触发。
	int32 activeSlot = character->GetActiveShoulderSlot();
	if (bTrackActivated && (activeSlot == 1 || activeSlot == 2) && GetShoulderAsset(activeSlot) != activatedAsset) {
		character->ActivateShoulderWeapon(activeSlot); // 内部会NotifyActivated更新activatedAsset
	}

	// 3) 摆放：激活的那把在瞄准时举到枪口位置，其余的(和不瞄准时全部)背在背后。
	UForeverWeaponComponent* weaponComponent = character->GetWeaponComponent();
	bool bAiming = character->IsAiming() && weaponComponent && weaponComponent->HasWeapon();
	for (int32 i = 0; i < 2; ++i) {
		FSlotVisual& visual = slots[i];
		if (!visual.actor.IsValid()) continue;

		bool bAimThis = bAiming && activeSlot == i + 1;
		if (bAimThis) {
			ApplyAimPose(i);
			SetHiddenForOwner(visual, false);
		} else {
			if (visual.pose != EPose::Holster) ApplyHolsterPose(i);
			SetHiddenForOwner(visual, character->IsFirstPerson()); // 第一人称看不到自己背上的枪
		}
	}
}
