#include "Player/ForeverRocketProjectile.h"

#include "Element/CitizenElement.h"
#include "Player/ForeverExplosionEffect.h"
#include "Player/ForeverProjectileBridgeComponent.h"

#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

#include "populace/citizen.h"

// 球体网格(/Engine/BasicShapes/Sphere)直径100cm；火焰/烟材质同AForeverExplosionEffect(参数Color/EmissiveStrength/Opacity)。
#define ROCKET_SPHERE_DIAMETER 100.f
#define ROCKET_FIRE_MATERIAL_PATH TEXT("/Game/Asset/Effects/Explosion/ExplosionFire.ExplosionFire")
#define ROCKET_SMOKE_MATERIAL_PATH TEXT("/Game/Asset/Effects/Explosion/ExplosionSmoke.ExplosionSmoke")

AForeverRocketProjectile::AForeverRocketProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> sphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> fireFinder(ROCKET_FIRE_MATERIAL_PATH);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> smokeFinder(ROCKET_SMOKE_MATERIAL_PATH);

	// 碰撞盒：对所有通道Block(撞墙/地面/市民/载具/水都算)，只做查询，不参与物理；Actor原点=盒子中心=火箭弹中心，
	// 网格/尾焰按"尾端在-X半个盒长处"摆(见TailLocation)。
	box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	RootComponent = box;
	box->SetBoxExtent(boxHalfExtent);
	box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	box->SetCollisionObjectType(ECC_WorldDynamic);
	box->SetCollisionResponseToAllChannels(ECR_Block);
	box->SetGenerateOverlapEvents(false);
	box->SetCanEverAffectNavigation(false);

	meshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RocketMesh"));
	meshComponent->SetupAttachment(box);
	meshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	meshComponent->SetCastShadow(false);
	meshComponent->SetCanEverAffectNavigation(false);

	// 移动组件：初速在BeginPlay里按朝向写，Tick里加速；不跟随速度转向、无重力、不反弹，撞到就停(OnProjectileStop)。
	movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	movement->UpdatedComponent = box;
	movement->InitialSpeed = 0.f;
	movement->ProjectileGravityScale = 0.f;
	movement->bRotationFollowsVelocity = false;
	movement->bShouldBounce = false;

	bridge = CreateDefaultSubobject<UForeverProjectileBridgeComponent>(TEXT("Bridge"));

	// 尾焰：拉长的球(材质Additive)+橙色点光源，都挂在尾端原点向-X。
	flame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flame"));
	flame->SetupAttachment(box);
	flame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	flame->SetCastShadow(false);
	flame->SetCanEverAffectNavigation(false);
	if (sphereFinder.Succeeded()) flame->SetStaticMesh(sphereFinder.Object);
	if (fireFinder.Succeeded()) flame->SetMaterial(0, fireFinder.Object);

	flameLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FlameLight"));
	flameLight->SetupAttachment(box);
	flameLight->SetMobility(EComponentMobility::Movable);
	flameLight->SetCastShadows(false);
	flameLight->SetLightColor(FLinearColor(1.f, 0.5f, 0.12f));
	flameLight->SetAttenuationRadius(700.f);
	flameLight->SetIntensity(0.f);

	// 飞行音效：自己的音频组件，不走脚本系统；声音资源在BeginPlay里按路径加载，Tick里保证连续播放。
	flightAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("FlightAudio"));
	flightAudio->SetupAttachment(box);
	flightAudio->bAutoActivate = false;

	// 烟团：世界坐标，不跟着火箭弹移动；隐藏着环形复用。
	for (int32 i = 0; i < SmokeCount; i++) {
		FRocketSmokePuff puff;
		puff.mesh = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Smoke%d"), i));
		puff.mesh->SetupAttachment(box);
		puff.mesh->SetAbsolute(true, true, true);
		puff.mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		puff.mesh->SetCastShadow(false);
		puff.mesh->SetCanEverAffectNavigation(false);
		puff.mesh->SetVisibility(false);
		if (sphereFinder.Succeeded()) puff.mesh->SetStaticMesh(sphereFinder.Object);
		if (smokeFinder.Succeeded()) puff.mesh->SetMaterial(0, smokeFinder.Object);
		smokes.Add(puff);
	}
}

void AForeverRocketProjectile::OnConstruction(const FTransform& transform)
{
	Super::OnConstruction(transform);

	// 蓝图里改了网格/盒尺寸，编辑器里立刻能看到；网格原点在尾端，所以挪到盒子尾端。
	box->SetBoxExtent(boxHalfExtent);
	meshComponent->SetRelativeLocation(TailLocation());
	if (rocketMesh) meshComponent->SetStaticMesh(rocketMesh);
}

void AForeverRocketProjectile::BeginPlay()
{
	Super::BeginPlay();

	// 发射者(Instigator=玩家Pawn)不挡火箭弹，否则刚出膛就炸在自己脸上。
	if (APawn* shooter = GetInstigator()) {
		box->IgnoreActorWhenMoving(shooter, true);
	}

	movement->MaxSpeed = maxSpeed;
	movement->Velocity = GetActorForwardVector() * initialSpeed;
	movement->UpdateComponentVelocity();
	movement->OnProjectileStop.AddDynamic(this, &AForeverRocketProjectile::OnProjectileStop);

	// 每个烟团/火焰独占一个动态材质，颜色/透明度才能各自变化。
	if (flame->GetMaterial(0)) flameMaterial = flame->CreateAndSetMaterialInstanceDynamic(0);
	for (FRocketSmokePuff& puff : smokes) {
		if (puff.mesh->GetMaterial(0)) puff.material = puff.mesh->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (!flightSoundPath.IsEmpty()) {
		FString path = flightSoundPath;
		if (!path.Contains(TEXT("."))) path += TEXT(".") + FPackageName::GetShortName(path);
		if (USoundBase* sound = LoadObject<USoundBase>(nullptr, *path)) {
			flightAudio->SetSound(sound);
			flightAudio->SetVolumeMultiplier(flightVolume);
		}
		else {
			UE_LOG(LogTemp, Warning, TEXT("Rocket: failed to load flight sound %s, flying silently."), *path);
		}
	}

	SetLifeSpan(lifeSeconds);
}

void AForeverRocketProjectile::Tick(float deltaSeconds)
{
	Super::Tick(deltaSeconds);
	if (bExploded) return;

	age += deltaSeconds;

	// 加速：UProjectileMovementComponent没有加速度，这里直接沿当前飞行方向加速度到maxSpeed。
	const FVector velocity = movement->Velocity;
	const FVector direction = velocity.IsNearlyZero() ? GetActorForwardVector() : velocity.GetSafeNormal();
	movement->Velocity = direction * FMath::Min(velocity.Size() + acceleration * deltaSeconds, maxSpeed);

	UpdateFlame(deltaSeconds);
	UpdateSmoke(deltaSeconds);
	UpdateFlightSound();
}

void AForeverRocketProjectile::UpdateFlightSound()
{
	if (!flightAudio || !flightAudio->Sound) return;
	// 连续播放：没在播(第一次，或声音资源不是循环的、播完了)就从头再播；音调按当前速度在初速和最大速度之间插值。
	if (!flightAudio->IsPlaying()) flightAudio->Play();
	const float u = maxSpeed > initialSpeed ? FMath::Clamp((movement->Velocity.Size() - initialSpeed) / (maxSpeed - initialSpeed), 0.f, 1.f) : 1.f;
	flightAudio->SetPitchMultiplier(FMath::Lerp(flightPitchAtStart, flightPitchAtMax, u));
}

void AForeverRocketProjectile::UpdateFlame(float deltaSeconds)
{
	// 闪烁：长度/半径随时间和随机抖动，火焰球中心在尾端后面半个长度处。
	const float flicker = 1.f + 0.2f * FMath::Sin(age * 60.f) + FMath::FRandRange(-0.12f, 0.12f);
	const float length = flameLength * flicker;
	const float radius = flameRadius * (1.f + FMath::FRandRange(-0.1f, 0.1f));
	flame->SetRelativeLocation(TailLocation() + FVector(-length * 0.5f, 0.f, 0.f));
	flame->SetRelativeScale3D(FVector(length / ROCKET_SPHERE_DIAMETER, radius * 2.f / ROCKET_SPHERE_DIAMETER, radius * 2.f / ROCKET_SPHERE_DIAMETER));
	if (flameMaterial) {
		flameMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.6f + FMath::FRandRange(-0.1f, 0.1f), 0.12f));
		flameMaterial->SetScalarParameterValue(TEXT("EmissiveStrength"), 12.f * emissiveScale);
		flameMaterial->SetScalarParameterValue(TEXT("Opacity"), 1.f);
	}
	flameLight->SetRelativeLocation(TailLocation() + FVector(-length * 0.6f, 0.f, 0.f));
	flameLight->SetIntensity(flameLightIntensity * flicker);
}

void AForeverRocketProjectile::UpdateSmoke(float deltaSeconds)
{
	// 到点就在尾端放一团新烟(环形复用最老的那团)，已有的烟胀大并淡出。
	smokeTimer += deltaSeconds;
	if (smokeInterval > 0.f && smokeTimer >= smokeInterval) {
		smokeTimer = FMath::Fmod(smokeTimer, smokeInterval);
		FRocketSmokePuff& puff = smokes[nextSmoke];
		nextSmoke = (nextSmoke + 1) % SmokeCount;
		puff.age = 0.f;
		puff.mesh->SetWorldLocation(GetActorTransform().TransformPosition(TailLocation()) - GetActorForwardVector() * flameLength * 0.4f);
		puff.mesh->SetVisibility(true);
		if (puff.material) puff.material->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.12f, 0.11f, 0.1f) * smokeBrightness);
	}

	for (FRocketSmokePuff& puff : smokes) {
		if (puff.age < 0.f) continue;
		puff.age += deltaSeconds;
		const float u = puff.age / smokeLife;
		if (u >= 1.f) {
			puff.age = -1.f;
			puff.mesh->SetVisibility(false);
			continue;
		}
		const float radius = FMath::Lerp(smokeRadiusStart, smokeRadiusEnd, 1.f - FMath::Pow(1.f - u, 2.f));
		puff.mesh->SetWorldScale3D(FVector(radius * 2.f / ROCKET_SPHERE_DIAMETER));
		if (puff.material) puff.material->SetScalarParameterValue(TEXT("Opacity"), 0.6f * (1.f - u));
	}
}

void AForeverRocketProjectile::OnProjectileStop(const FHitResult& hit)
{
	Explode(hit);
}

void AForeverRocketProjectile::Explode(const FHitResult& hit)
{
	if (bExploded) return;
	bExploded = true;
	if (flightAudio) flightAudio->Stop();

	FVector normal = FVector(hit.ImpactNormal);
	if (normal.IsNearlyZero()) normal = -GetActorForwardVector();
	normal = normal.GetSafeNormal();
	const FVector point = hit.bBlockingHit ? FVector(hit.ImpactPoint) : GetActorLocation();

	// 爆炸特效是独立的世界Actor(不挂在火箭弹上)，下面火箭弹自毁不会带走它。
	AForeverExplosionEffect::Spawn(GetWorld(), point, normal, explosionScale, explosionEffectClassPath);

	// 报告命中：脚本的projectile_hit里程碑里用area_damage结算范围伤害(同步执行，ReportHit返回时已结算完)。
	FString sufferer;
	if (ACitizenElement* citizenElement = Cast<ACitizenElement>(hit.GetActor())) {
		Citizen* citizen = citizenElement->GetCitizen();
		if (citizen && !citizen->IsDead()) sufferer = UTF8_TO_TCHAR(citizen->GetName().c_str());
	}
	bridge->ReportHit(point + normal * reportOffset, normal, sufferer);

	// 先关碰撞/隐藏/停下，再销毁：不在移动组件扫掠中途留下仍然活动的组件。
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	movement->StopMovementImmediately();
	Destroy();
}
