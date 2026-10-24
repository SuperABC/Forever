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
	// 真正的备弹检查/扣减放在换弹计时结束时(TickComponent)，这里只负责启动计时——和开火
	// 判定的时序一致，不在按下R的瞬间就扣备弹。
}

void UForeverWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bReloading && GetWorld()->GetTimeSeconds() >= reloadEndTime) {
		bReloading = false;
		if (currentWeapon) {
			// 无论弹匣里剩多少发，一次换弹都扣除整个弹匣容量的备弹——不是"补齐差额"。备弹
			// 不够就不扣减、不填满，currentAmmo保持原值(用户没要求"部分填充"这个edge case)。
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

	// 玩家手上有武器时一直在左上角显示弹匣余量：固定Key每帧刷新同一行，显示时长很短，武器被
	// ClearWeapon或者换人操控后这一行自己很快消失。只认玩家正在操控的Pawn，没被操控的市民就算
	// 手上还挂着武器也不输出。
	APawn* ownerPawn = Cast<APawn>(GetOwner());
	if (GEngine && currentWeapon && ownerPawn && ownerPawn->IsPlayerControlled()) {
		GEngine->AddOnScreenDebugMessage(200, 0.1f, FColor::White,
			FString::Printf(TEXT("弹药: %d/%d"), currentAmmo, currentWeapon->GetMagazineCapacity()));
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

#if ENABLE_DRAW_DEBUG
	// 弹道debugLine：从枪口画到命中点(没命中画到射程终点)，用来检查"弹道是不是从枪口出来"。只是视觉，
	// 真正的命中判定仍然是上面从摄像机射出的那一条射线。
	if (UForeverWeaponVisualComponent* visual = character->GetWeaponVisualComponent()) {
		FVector muzzle;
		if (visual->GetActiveMuzzleWorldLocation(muzzle)) {
			DrawDebugLine(GetWorld(), muzzle, bHit ? hit.ImpactPoint : end, FColor::Red, false, 1.0f, 0, 1.0f);
		}
	}
#endif


	// 后坐力：这一发子弹的碰撞检测/轨迹已经用踢之前的摄像机朝向算完了，这里踢的是"下一发"
	// 会用到的朝向，不会影响这一发自己的命中判定。直接改Controller的ControlRotation
	// (不是AddControllerPitchInput/YawInput那一套——那两个方法会经过PlayerController的
	// InputPitchScale/InputYawScale缩放，符号不直观，容易踢反方向)，用UE文档写明的FRotator
	// 约定(Pitch: +Up/-Down，Yaw: +Right/-Left)直接加，方向是确定的。仿PUBG：这里不做
	// 任何"过一会儿自动回正"的衰减，单发/连发都会踢(单发/连发最终都走这同一个Fire()调用)，
	// 玩家要自己拉鼠标压枪，见WeaponMod.h"后坐力"一节。
	if (AController* controller = character->GetController()) {
		float pitchKick = FMath::FRandRange(currentWeapon->GetRecoilPitchMin(), currentWeapon->GetRecoilPitchMax());
		float yawKick = FMath::FRandRange(currentWeapon->GetRecoilYawMin(), currentWeapon->GetRecoilYawMax());
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

	// 命中位置的爆炸特效(纯视觉)。世界坐标生成，不依赖citizen存活，所以放在死亡处理之前也安全。
	AForeverExplosionEffect::Spawn(GetWorld(), hit.ImpactPoint, hit.ImpactNormal);


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
