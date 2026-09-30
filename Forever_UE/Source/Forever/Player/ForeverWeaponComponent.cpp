#include "Player/ForeverWeaponComponent.h"

#include "Player/ForeverCharacter.h"
#include "Element/CitizenElement.h"
#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverStoryFrameworkComponent.h"
#include "Framework/ForeverPopulaceFrameworkComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"

#include "common/registry.h"
#include "player/weapon_mod.h"
#include "populace/citizen.h"

using namespace std;

UForeverWeaponComponent::UForeverWeaponComponent() {
	PrimaryComponentTick.bCanEverTick = true;
}

void UForeverWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	DestroyWeaponMesh();
	if (currentWeapon) {
		Registry::Get().GetWeaponFactory().DestroyWeapon(currentWeapon);
		currentWeapon = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UForeverWeaponComponent::EquipWeapon(const FString& weaponId) {
	string idUtf8 = TCHAR_TO_UTF8(*weaponId);
	WeaponMod* newWeapon = Registry::Get().GetWeaponFactory().CreateWeapon(idUtf8);
	if (!newWeapon) return; // id未注册/未在config.json"weapon_mods"数组里启用，静默保留原武器

	DestroyWeaponMesh();
	if (currentWeapon) {
		Registry::Get().GetWeaponFactory().DestroyWeapon(currentWeapon);
	}

	currentWeapon = newWeapon;
	currentAmmo = currentWeapon->magazineCapacity;
	bReloading = false;
	bWantsToFire = false;
	SpawnWeaponMesh();
}

int32 UForeverWeaponComponent::GetMagazineCapacity() const {
	return currentWeapon ? currentWeapon->magazineCapacity : 0;
}

void UForeverWeaponComponent::SpawnWeaponMesh() {
	if (!currentWeapon || currentWeapon->firstPersonMeshPath.empty()) return; // 见weapon_mod.h：MVP阶段mesh路径留空是正常状态

	AActor* owner = GetOwner();
	if (!owner || !owner->GetRootComponent()) return;

	UStaticMesh* mesh = LoadObject<UStaticMesh>(nullptr, UTF8_TO_TCHAR(currentWeapon->firstPersonMeshPath.c_str()));
	if (!mesh) return;

	weaponMesh = NewObject<UStaticMeshComponent>(owner, NAME_None, RF_Transient);
	weaponMesh->SetStaticMesh(mesh);
	weaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	weaponMesh->SetGenerateOverlapEvents(false);

	FName socket = currentWeapon->gripSocketName.empty() ? NAME_None : FName(UTF8_TO_TCHAR(currentWeapon->gripSocketName.c_str()));
	ACharacter* character = Cast<ACharacter>(owner);
	if (character && !socket.IsNone() && character->GetMesh() && character->GetMesh()->DoesSocketExist(socket)) {
		weaponMesh->SetupAttachment(character->GetMesh(), socket);
	} else {
		// gripSocketName留空="直接挂在角色Root/胶囊体上"，见weapon_mod.h。
		weaponMesh->SetupAttachment(owner->GetRootComponent());
	}

	// 挂载点在WeaponMod里定义的本地偏移——没有真实持枪socket时直接挂Root会让枪出现在
	// 身体正中心，用这个偏移挪到看起来合理的位置(比如右肩膀靠前一点)，见weapon_mod.h。
	weaponMesh->SetRelativeLocation(FVector(currentWeapon->attachOffsetX, currentWeapon->attachOffsetY, currentWeapon->attachOffsetZ));

	weaponMesh->RegisterComponent();
}

void UForeverWeaponComponent::DestroyWeaponMesh() {
	if (weaponMesh) {
		weaponMesh->DestroyComponent();
		weaponMesh = nullptr;
	}
}

void UForeverWeaponComponent::StartFire() {
	bWantsToFire = true;
	Fire(); // 单发/全自动按下的第一发都立刻打（仍然受fireRate冷却限制，不是无条件立即开火）
}

void UForeverWeaponComponent::StopFire() {
	bWantsToFire = false;
}

void UForeverWeaponComponent::Reload() {
	if (!currentWeapon || bReloading) return;
	if (currentAmmo >= currentWeapon->magazineCapacity) return;

	bReloading = true;
	bWantsToFire = false;
	reloadEndTime = GetWorld()->GetTimeSeconds() + currentWeapon->reloadDuration;
	// MVP：备弹视为无限，这里不检查/不消耗任何备弹对象，见weapon_mod.h/weapon_basic.h
	// 顶部注释——等以后背包/Asset域真正做出来，把下面TickComponent里"弹匣填满"那一步
	// 换成"检查+扣减备弹，不够就不填满"即可，WeaponMod的其余字段和这个类的其它逻辑
	// 完全不用动。
}

void UForeverWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bReloading && GetWorld()->GetTimeSeconds() >= reloadEndTime) {
		bReloading = false;
		if (currentWeapon) {
			currentAmmo = currentWeapon->magazineCapacity;
			if (GEngine) {
				GEngine->AddOnScreenDebugMessage(200, 3.f, FColor::White,
					FString::Printf(TEXT("弹药: %d/%d"), currentAmmo, currentWeapon->magazineCapacity));
			}
		}
	}

	if (bWantsToFire && currentWeapon && currentWeapon->fullAuto) {
		Fire();
	}
}

void UForeverWeaponComponent::Fire() {
	if (!currentWeapon || bReloading) return;

	// 必须瞄准中才能开火——用户明确要求，不瞄准时按开火键完全没反应(不消耗弹药、不进
	// 冷却、不打印"弹匣已空"，因为压根没有真正尝试开火)。这个检查必须放在最前面，比
	// fireRate/弹药检查还早，否则"没瞄准时疯狂点按开火键"会在真正开始瞄准的第一时间
	// 因为lastFireTime/弹药已经被消耗过而表现异常。
	AForeverCharacter* character = Cast<AForeverCharacter>(GetOwner());
	if (!character || !character->IsAiming()) return;

	float now = GetWorld()->GetTimeSeconds();
	if (now - lastFireTime < currentWeapon->fireRate) return;

	if (currentAmmo <= 0) {
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Silver, TEXT("弹匣已空，按R换弹"));
		return;
	}

	lastFireTime = now;
	currentAmmo--;

	// 左上角显示当前弹匣余量——用固定Key(不是-1)，这样每次开火都是"刷新同一行"而不是往下
	// 堆叠新的一行消息，用户要求"每次开火后"显示，不是只在换弹/切枪时显示一次。
	if (GEngine) {
		GEngine->AddOnScreenDebugMessage(200, 3.f, FColor::White,
			FString::Printf(TEXT("弹药: %d/%d"), currentAmmo, currentWeapon->magazineCapacity));
	}

	UCameraComponent* camera = character->IsFirstPerson() ? character->GetFirstPersonCamera() : character->GetFollowCamera();
	if (!camera) return;

	// 改回从摄像机(眼睛)位置发射——用户明确要求撤回"从枪口发射"那版改动，见.md"开火起点"
	// 一节的修订记录。
	FVector start = camera->GetComponentLocation();

	float spreadRad = FMath::DegreesToRadians(currentWeapon->baseSpread);
	FVector direction = FMath::VRandCone(camera->GetForwardVector(), spreadRad);
	FVector end = start + direction * currentWeapon->maxRange;

	// ECC_Pawn，不是ECC_Visibility——实测发现的bug：这个项目的DefaultEngine.ini用的是UE5
	// 原版内置的"Pawn"碰撞预设(Source/Forever自己没有改过这份配置)，这个预设对Visibility
	// 通道的CustomResponses显式写的是ECR_Ignore(`Config/DefaultEngine.ini`
	// `[/Script/Engine.CollisionProfile]`节的"Pawn"那一行)——用Visibility通道打的射线会
	// 直接穿过citizen的Capsule，只有墙体(BlockAll/BlockAllDynamic预设，对所有通道都是
	// Block)才会被挡住，表现正是"打墙正常、打市民直接穿过去、也不打印任何消息"。改用
	// ECC_Pawn通道：BlockAll/BlockAllDynamic对"没有专门覆盖的通道"一律默认Block，Pawn
	// 预设对"Pawn"通道本身也没有覆盖成Ignore，两边都会正常挡住，不需要改
	// DefaultEngine.ini/新增自定义碰撞通道。
	FHitResult hit;
	FCollisionQueryParams params;
	params.AddIgnoredActor(GetOwner());
	bool bHit = GetWorld()->LineTraceSingleByChannel(hit, start, end, ECC_Pawn, params);

	// 轨迹线段：不管打中什么(墙/citizen/什么都没打中)都画——bHit为true时画到实际碰撞点
	// (hit.ImpactPoint)，没打中任何东西时画到maxRange处的射线终点(end)，视觉上子弹应该
	// 飞到障碍物为止，不是只有"算作命中"才画。用DrawDebugLine而不是真正的粒子特效——这次
	// 没有muzzleFlashEffectPath/impactEffectPath对应的VFX资产，纯debug线段先验证弹道
	// 方向/散射手感，见ForeverWeaponComponent.md"子弹轨迹"一节。
	FVector trailEnd = bHit ? hit.ImpactPoint : end;

	// 碰撞检测(上面的LineTraceSingleByChannel)仍然用摄像机位置和方向做——这是命中判定的
	// 依据，不能改；但画出来的debugLine视觉起点改成枪的位置(weaponMesh的世界坐标)，不是
	// 摄像机位置——枪和摄像机不在同一个点，子弹轨迹从眼睛位置画出来看着不像从枪口飞出去的。
	// weaponMesh还没有真正的mesh资产(firstPersonMeshPath留空)时退化成角色位置+attachOffset
	// 本身转到世界坐标的偏移——实测踩过的坑：一开始退化写的是纯character->GetActorLocation()，
	// 完全没管attachOffsetX/Y/Z，导致没有mesh资产的情况下(目前就是这样)那三个偏移字段
	// 形同虚设，调了WeaponMod里的数值画出来的线段起点却纹丝不动——因为一直走的是这条完全
	// 忽略偏移的退化分支，不是weaponMesh分支。这里补上：用角色当前朝向把本地偏移转到
	// 世界坐标再加到角色位置上，和"挂到Root上再加SetRelativeLocation"这条路径算出来的
	// 效果一致。
	FVector localOffset(currentWeapon->attachOffsetX, currentWeapon->attachOffsetY, currentWeapon->attachOffsetZ);
	FVector gunLocation = weaponMesh ? weaponMesh->GetComponentLocation() :
		character->GetActorLocation() + character->GetActorRotation().RotateVector(localOffset);
	DrawDebugLine(GetWorld(), gunLocation, trailEnd, FColor::Yellow, false, 5.f, 0, 1.f);

	// 后坐力：这一发子弹的碰撞检测/轨迹已经用踢之前的摄像机朝向算完了，这里踢的是"下一发"
	// 会用到的朝向，不会影响这一发自己的命中判定。直接改Controller的ControlRotation
	// (不是AddControllerPitchInput/YawInput那一套——那两个方法会经过PlayerController的
	// InputPitchScale/InputYawScale缩放，符号不直观，容易踢反方向)，用UE文档写明的FRotator
	// 约定(Pitch: +Up/-Down，Yaw: +Right/-Left)直接加，方向是确定的。仿PUBG：这里不做
	// 任何"过一会儿自动回正"的衰减，单发/连发都会踢(单发/连发最终都走这同一个Fire()调用)，
	// 玩家要自己拉鼠标压枪，见WeaponMod.h"后坐力"一节。
	if (AController* controller = character->GetController()) {
		float pitchKick = FMath::FRandRange(currentWeapon->recoilPitchMin, currentWeapon->recoilPitchMax);
		float yawKick = FMath::FRandRange(currentWeapon->recoilYawMin, currentWeapon->recoilYawMax);
		FRotator recoiledRotation = controller->GetControlRotation();
		recoiledRotation.Pitch += pitchKick;
		recoiledRotation.Yaw += yawKick;
		controller->SetControlRotation(recoiledRotation);
	}

	// 只有真的打中ACitizenElement才算"命中"——打中墙/什么都没打中，在伤害判定这一层
	// 完全等价（都不产生任何效果），不像上一版那样对任意命中的Actor都打印一条"命中 X，
	// 伤害 Y"的debug消息（那条消息在打中墙的时候意义不明，墙又不会真的掉血）。
	ACitizenElement* hitCitizen = bHit ? Cast<ACitizenElement>(hit.GetActor()) : nullptr;
	if (!hitCitizen) return;

	Citizen* target = hitCitizen->GetCitizen();
	if (!target || target->IsDead()) return;

	float distance = FVector::Dist(start, hit.ImpactPoint);
	float damage = currentWeapon->ComputeDamage(distance);
	target->TakeDamage(damage);

	if (GEngine) {
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red,
			FString::Printf(TEXT("命中 %s，伤害 %.1f"), *hit.GetActor()->GetName(), damage));
	}

	if (target->IsDead()) {
		FString deadName = UTF8_TO_TCHAR(target->GetName().c_str());
		if (AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
			UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()))) {
			if (framework->GetStoryFramework()) {
				framework->GetStoryFramework()->BroadcastCitizenDecease(deadName, TEXT("gunshot"));
			}
			if (framework->GetPopulaceFramework()) {
				framework->GetPopulaceFramework()->HandleCitizenDeath(target);
			}
		}
	}
}
