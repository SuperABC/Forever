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
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

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
		if (currentWeapon) currentAmmo = currentWeapon->magazineCapacity;
	}

	if (bWantsToFire && currentWeapon && currentWeapon->fullAuto) {
		Fire();
	}
}

void UForeverWeaponComponent::Fire() {
	if (!currentWeapon || bReloading) return;

	float now = GetWorld()->GetTimeSeconds();
	if (now - lastFireTime < currentWeapon->fireRate) return;

	if (currentAmmo <= 0) {
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1.f, FColor::Silver, TEXT("弹匣已空，按R换弹"));
		return;
	}

	lastFireTime = now;
	currentAmmo--;

	AForeverCharacter* character = Cast<AForeverCharacter>(GetOwner());
	if (!character) return;

	UCameraComponent* camera = character->IsFirstPerson() ? character->GetFirstPersonCamera() : character->GetFollowCamera();
	if (!camera) return;

	FVector start = camera->GetComponentLocation();
	float spreadRad = FMath::DegreesToRadians(currentWeapon->baseSpread);
	FVector direction = FMath::VRandCone(camera->GetForwardVector(), spreadRad);
	FVector end = start + direction * currentWeapon->maxRange;

	// ECC_Visibility：默认碰撞预设里静态几何(墙体slab，见BuildingElement.cpp的SpawnCube)
	// 和Pawn(ACharacter默认Capsule预设)都会Block这个通道——子弹应该先打到墙就停下，不会
	// 穿墙命中，这次不需要额外配置新的碰撞通道/预设。
	FHitResult hit;
	FCollisionQueryParams params;
	params.AddIgnoredActor(GetOwner());
	bool bHit = GetWorld()->LineTraceSingleByChannel(hit, start, end, ECC_Visibility, params);

	if (!bHit || !hit.GetActor()) return;

	float distance = FVector::Dist(start, hit.ImpactPoint);
	float damage = currentWeapon->ComputeDamage(distance);

	if (ACitizenElement* hitCitizen = Cast<ACitizenElement>(hit.GetActor())) {
		Citizen* target = hitCitizen->GetCitizen();
		if (target && !target->IsDead()) {
			target->TakeDamage(damage);
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
	}

	if (GEngine) {
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Red,
			FString::Printf(TEXT("命中 %s，伤害 %.1f"), *hit.GetActor()->GetName(), damage));
	}
}
