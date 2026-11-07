#include "Player/ForeverWeaponComponent.h"

#include "Player/ForeverCharacter.h"
#include "Player/ForeverExplosionEffect.h"
#include "Player/ForeverWeaponVisualComponent.h"
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
#include "player/weapon.h"
#include "story/event.h"
#include "player/player.h"
#include "populace/citizen.h"

using namespace std;

UForeverWeaponComponent::UForeverWeaponComponent() {
	PrimaryComponentTick.bCanEverTick = true;
}

void UForeverWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	delete currentWeapon;
	currentWeapon = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UForeverWeaponComponent::EquipWeapon(const FString& weaponId) {
	string idUtf8 = TCHAR_TO_UTF8(*weaponId);
	Weapon* newWeapon = new Weapon(&Registry::Get().GetWeaponFactory(), idUtf8);
	if (!newWeapon->IsValid()) { // id未注册/未在config.json"weapon_mods"数组里启用，静默保留原武器
		delete newWeapon;
		return;
	}

	delete currentWeapon;

	currentWeapon = newWeapon;
	currentAmmo = currentWeapon->GetMagazineCapacity();
	bReloading = false;
	bWantsToFire = false;
}

void UForeverWeaponComponent::ClearWeapon() {
	if (!currentWeapon) return;

	delete currentWeapon;
	currentWeapon = nullptr;
	currentAmmo = 0;
	bReloading = false;
	bWantsToFire = false;
}

int32 UForeverWeaponComponent::GetMagazineCapacity() const {
	return currentWeapon ? currentWeapon->GetMagazineCapacity() : 0;
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
	if (currentAmmo >= currentWeapon->GetMagazineCapacity()) return;

	bReloading = true;
	bWantsToFire = false;
	reloadEndTime = GetWorld()->GetTimeSeconds() + currentWeapon->GetReloadDuration();
}

void UForeverWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bReloading && GetWorld()->GetTimeSeconds() >= reloadEndTime) {
		bReloading = false;
		if (currentWeapon) {
			// 一次换弹扣整个弹匣容量的备弹(不是补齐差额)；备弹不够就不扣、不填满。
			if (AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
				UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()))) {
				if (Player* player = framework->GetPlayer()) {
					if (player->ConsumeByType(currentWeapon->GetAmmoType(), currentWeapon->GetMagazineCapacity())) {
						currentAmmo = currentWeapon->GetMagazineCapacity();
					}
				}
			}
		}
	}

	if (bWantsToFire && currentWeapon && currentWeapon->IsFullAuto()) {
		Fire();
	}

	// 固定Key每帧刷新同一行、显示时长很短，武器被清掉或换人后自己消失；只显示玩家正在操控的Pawn。
	APawn* ownerPawn = Cast<APawn>(GetOwner());
	if (GEngine && currentWeapon && ownerPawn && ownerPawn->IsPlayerControlled()) {
		GEngine->AddOnScreenDebugMessage(200, 0.1f, FColor::White,
			FString::Printf(TEXT("弹药: %d/%d"), currentAmmo, currentWeapon->GetMagazineCapacity()));
	}
}

void UForeverWeaponComponent::Fire() {
	if (!currentWeapon || bReloading) return;

	// 必须瞄准中才能开火，且要放在冷却/弹药检查之前，否则没瞄准时点按会白白消耗弹药和冷却。
	AForeverCharacter* character = Cast<AForeverCharacter>(GetOwner());
	if (!character || !character->IsAiming()) return;

	float now = GetWorld()->GetTimeSeconds();
	if (now - lastFireTime < currentWeapon->GetFireRate()) return;

	if (currentAmmo <= 0) return;

	lastFireTime = now;
	currentAmmo--;

	UCameraComponent* camera = character->IsFirstPerson() ? character->GetFirstPersonCamera() : character->GetFollowCamera();
	if (!camera) return;

	// 开火起点用摄像机(眼睛)位置，不是枪口位置——保证"看哪打哪"的精确命中判定。
	FVector start = camera->GetComponentLocation();

	float spreadRad = FMath::DegreesToRadians(currentWeapon->GetBaseSpread());
	FVector direction = FMath::VRandCone(camera->GetForwardVector(), spreadRad);
	FVector end = start + direction * currentWeapon->GetMaxRange();

	// 必须用ECC_Pawn：本项目Pawn碰撞预设忽略ECC_Visibility，射线会穿过市民胶囊体(见CONVENTIONS第11节)。
	FHitResult hit;
	FCollisionQueryParams params;
	params.AddIgnoredActor(GetOwner());
	bool bHit = GetWorld()->LineTraceSingleByChannel(hit, start, end, ECC_Pawn, params);

#if ENABLE_DRAW_DEBUG
	// 弹道debugLine(枪口→命中点/射程终点)，只是视觉，命中判定仍是上面从摄像机射出的射线。
	if (UForeverWeaponVisualComponent* visual = character->GetWeaponVisualComponent()) {
		FVector muzzle;
		if (visual->GetActiveMuzzleWorldLocation(muzzle)) {
			DrawDebugLine(GetWorld(), muzzle, bHit ? hit.ImpactPoint : end, FColor::Red, false, 1.0f, 0, 1.0f);
		}
	}
#endif


	// 后坐力：踢的是下一发的朝向，不影响这一发。直接改ControlRotation而不用AddControllerPitchInput
	// (会被InputPitchScale缩放、方向不直观)；不自动回正，见md。
	if (AController* controller = character->GetController()) {
		float pitchKick = FMath::FRandRange(currentWeapon->GetRecoilPitchMin(), currentWeapon->GetRecoilPitchMax());
		float yawKick = FMath::FRandRange(currentWeapon->GetRecoilYawMin(), currentWeapon->GetRecoilYawMax());
		FRotator recoiledRotation = controller->GetControlRotation();
		recoiledRotation.Pitch += pitchKick;
		recoiledRotation.Yaw += yawKick;
		controller->SetControlRotation(recoiledRotation);
	}

	// 只有打中ACitizenElement才算命中，打墙/落空都不产生任何效果。
	ACitizenElement* hitCitizen = bHit ? Cast<ACitizenElement>(hit.GetActor()) : nullptr;
	if (!hitCitizen) return;

	Citizen* target = hitCitizen->GetCitizen();
	if (!target || target->IsDead()) return;

	float distance = FVector::Dist(start, hit.ImpactPoint);
	float damage = currentWeapon->ComputeDamage(distance);
	target->TakeDamage(damage);

	// 命中位置的爆炸特效(纯视觉)。世界坐标生成，不依赖citizen存活，所以放在死亡处理之前也安全。
	AForeverExplosionEffect::Spawn(GetWorld(), hit.ImpactPoint, hit.ImpactNormal);


	if (target->IsDead()) {
		FString deadName = UTF8_TO_TCHAR(target->GetName().c_str());
		if (AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
			UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()))) {
			if (framework->GetStoryFramework()) {
				// 击杀信息：凶手(开枪的市民)、武器种类、命中部位、致死伤害、凶手是否可见、死亡地址。
				ACitizenElement* killerElement = Cast<ACitizenElement>(GetOwner());
				Citizen* killer = killerElement ? killerElement->GetCitizen() : nullptr;

				// 命中部位：射线只打得到胶囊体，按命中点在胶囊体上的相对高度粗分——
				// 顶部20%算头，下半截(40%以下)算腿，中间躯干。
				float radius = 0.f, halfHeight = 0.f;
				hitCitizen->GetSimpleCollisionCylinder(radius, halfHeight);
				float relativeHeight = halfHeight > 0.f
					? (hit.ImpactPoint.Z - (hitCitizen->GetActorLocation().Z - halfHeight)) / (2.f * halfHeight) : 0.5f;
				const char* part = relativeHeight >= 0.8f ? "head" : (relativeHeight >= 0.4f ? "body" : "legs");

				// 凶手是否可见：连接死者和凶手中心点的线段有没有被遮挡物(墙等)挡住，两人自己不算遮挡。
				bool visible = false;
				if (killerElement) {
					FCollisionQueryParams sightParams;
					sightParams.AddIgnoredActor(hitCitizen);
					sightParams.AddIgnoredActor(killerElement);
					visible = !GetWorld()->LineTraceTestByChannel(hitCitizen->GetActorLocation(),
						killerElement->GetActorLocation(), ECC_Visibility, sightParams);
				}

				CitizenDeceaseEvent event(std::string(TCHAR_TO_UTF8(*deadName)), "player",
					killer ? killer->GetName() : std::string(), currentWeapon->GetType(), part, damage, visible,
					framework->DescribeLocation(hitCitizen->GetActorLocation()));
				framework->GetStoryFramework()->BroadcastCitizenDecease(event);
			}
			if (framework->GetPopulaceFramework()) {
				framework->GetPopulaceFramework()->HandleCitizenDeath(target);
			}
		}
	}
}
