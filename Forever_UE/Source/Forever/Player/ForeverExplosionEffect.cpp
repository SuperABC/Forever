#include "Player/ForeverExplosionEffect.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Player/ForeverExplosionShake.h"
#include "UObject/ConstructorHelpers.h"

// 球体网格(/Engine/BasicShapes/Sphere)直径100cm。
#define EXPLOSION_SPHERE_DIAMETER 100.f

// 火焰材质(Additive，参数Color/EmissiveStrength/Opacity)：火球、冲击波环、火花共用。
#define EXPLOSION_FIRE_MATERIAL_PATH TEXT("/Game/Asset/Materials/ExplosionFire.ExplosionFire")
// 烟材质(半透明，参数Color/Opacity)。
#define EXPLOSION_SMOKE_MATERIAL_PATH TEXT("/Game/Asset/Materials/ExplosionSmoke.ExplosionSmoke")

AForeverExplosionEffect::AForeverExplosionEffect()
{
	PrimaryActorTick.bCanEverTick = true;

	root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = root;

	fireball = MakePart(TEXT("Fireball"), false);
	ring = MakePart(TEXT("Ring"), false);
	for (int32 i = 0; i < SparkCount; i++) {
		sparks.Add(MakePart(*FString::Printf(TEXT("Spark%d"), i), false));
	}
	for (int32 i = 0; i < SmokeCount; i++) {
		smokes.Add(MakePart(*FString::Printf(TEXT("Smoke%d"), i), true));
	}

	flash = CreateDefaultSubobject<UPointLightComponent>(TEXT("Flash"));
	flash->SetupAttachment(root);
	flash->SetMobility(EComponentMobility::Movable);
	flash->SetCastShadows(false);
	flash->SetLightColor(FLinearColor(1.f, 0.55f, 0.15f));
	flash->SetIntensity(0.f);
}

FExplosionPart AForeverExplosionEffect::MakePart(const TCHAR* name, bool isSmoke)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> sphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> fireFinder(EXPLOSION_FIRE_MATERIAL_PATH);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> smokeFinder(EXPLOSION_SMOKE_MATERIAL_PATH);

	FExplosionPart part;
	part.mesh = CreateDefaultSubobject<UStaticMeshComponent>(name);
	part.mesh->SetupAttachment(root);
	// 纯视觉：不参与碰撞/阴影，否则会挡住后续射线或在场景里投出实心阴影。
	part.mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	part.mesh->SetCastShadow(false);
	part.mesh->SetCanEverAffectNavigation(false);
	part.mesh->SetVisibility(false);
	if (sphereFinder.Succeeded()) {
		part.mesh->SetStaticMesh(sphereFinder.Object);
	}
	auto& materialFinder = isSmoke ? smokeFinder : fireFinder;
	if (materialFinder.Succeeded()) {
		part.mesh->SetMaterial(0, materialFinder.Object);
	}
	return part;
}

void AForeverExplosionEffect::Spawn(UWorld* world, const FVector& location, const FVector& normal, float scale)
{
	if (!world) return;

	FTransform transform(FRotator::ZeroRotator, location);
	// 延迟生成：BeginPlay之前先把法线/缩放写进去，BeginPlay里的初始化才能用到。
	AForeverExplosionEffect* effect = world->SpawnActorDeferred<AForeverExplosionEffect>(
		AForeverExplosionEffect::StaticClass(), transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!effect) return;

	FVector safeNormal = normal.GetSafeNormal();
	effect->impactNormal = safeNormal.IsNearlyZero() ? FVector::UpVector : safeNormal;
	effect->effectScale = FMath::Max(scale, 0.01f);
	effect->FinishSpawning(transform);

	// 镜头震动：内半径内满强度，到外半径衰减为0；步枪命中是高频小爆炸，所以范围和幅度都很克制。
	UGameplayStatics::PlayWorldCameraShake(world, UForeverExplosionShake::StaticClass(), location,
		300.f * effect->effectScale, 1500.f * effect->effectScale);
}

void AForeverExplosionEffect::BeginPlay()
{
	Super::BeginPlay();

	// 每个部件独占一个动态材质，颜色才能各自变化。没有基础材质时material保持空，只靠网格默认外观。
	auto bindMaterial = [](FExplosionPart& part) {
		if (part.mesh && part.mesh->GetMaterial(0)) {
			part.material = part.mesh->CreateAndSetMaterialInstanceDynamic(0);
		}
	};
	bindMaterial(fireball);
	bindMaterial(ring);
	for (FExplosionPart& part : sparks) bindMaterial(part);
	for (FExplosionPart& part : smokes) bindMaterial(part);

	// 冲击波环：压扁的球当圆盘，盘面法线对准命中面。
	ring.mesh->SetWorldRotation(FRotationMatrix::MakeFromZ(impactNormal).Rotator());
	SetPartColor(ring, FLinearColor(1.f, 0.8f, 0.5f));

	flash->SetAttenuationRadius(ringRadius * effectScale * 4.f);
	// 光源稍微抬离命中面，避免正好陷进墙里。
	flash->SetRelativeLocation(impactNormal * 40.f * effectScale);

	for (int32 i = 0; i < SparkCount; i++) {
		// 火花在命中面法线的半球内随机射出。
		FVector direction = FMath::VRandCone(impactNormal, FMath::DegreesToRadians(70.f));
		sparkVelocity.Add(direction * FMath::FRandRange(sparkSpeedMin, sparkSpeedMax) * effectScale);
		sparkPosition.Add(FVector::ZeroVector);
		sparkLife.Add(FMath::FRandRange(sparkLifeMin, sparkLifeMax));
		SetPartColor(sparks[i], FLinearColor(1.f, 0.75f, 0.2f));
		SetPartRadius(sparks[i], 2.f * effectScale);
		sparks[i].mesh->SetVisibility(true);
	}

	for (int32 i = 0; i < SmokeCount; i++) {
		smokeOffset.Add(FMath::VRand() * 30.f * effectScale);
		smokeDelay.Add(0.05f + 0.04f * i);
		SetPartColor(smokes[i], FLinearColor(0.12f, 0.11f, 0.1f) * smokeBrightness);
	}

	fireball.mesh->SetVisibility(true);
	ring.mesh->SetVisibility(true);
}

void AForeverExplosionEffect::Tick(float deltaSeconds)
{
	Super::Tick(deltaSeconds);

	age += deltaSeconds;
	if (age >= totalDuration) {
		Destroy();
		return;
	}

	UpdateFireball();
	UpdateRing();
	UpdateFlash();
	UpdateSparks(deltaSeconds);
	UpdateSmoke();
}

void AForeverExplosionEffect::SetPartColor(FExplosionPart& part, const FLinearColor& color) const
{
	if (part.material) {
		part.material->SetVectorParameterValue(TEXT("Color"), color);
	}
}

void AForeverExplosionEffect::SetPartOpacity(FExplosionPart& part, float opacity) const
{
	if (part.material) {
		part.material->SetScalarParameterValue(TEXT("Opacity"), opacity);
	}
}

void AForeverExplosionEffect::SetPartEmissive(FExplosionPart& part, float strength) const
{
	if (part.material) {
		part.material->SetScalarParameterValue(TEXT("EmissiveStrength"), strength * emissiveScale);
	}
}

void AForeverExplosionEffect::SetPartRadius(FExplosionPart& part, float radius) const
{
	float diameterScale = radius * 2.f / EXPLOSION_SPHERE_DIAMETER;
	part.mesh->SetWorldScale3D(FVector(diameterScale));
}

void AForeverExplosionEffect::UpdateFireball()
{
	float u = age / fireballDuration;
	if (u >= 1.f) {
		fireball.mesh->SetVisibility(false);
		return;
	}

	// 前35%快速膨胀(ease-out)，之后继续缓慢胀大并淡出(Opacity)，不再缩小。
	float grow = 1.f - FMath::Pow(1.f - FMath::Min(u / 0.35f, 1.f), 3.f);
	float drift = 1.f + 0.25f * FMath::SmoothStep(0.35f, 1.f, u);
	SetPartRadius(fireball, fireballRadius * effectScale * grow * drift);

	// 橙黄 -> 暗红，亮度从很亮(触发Bloom辉光)衰减到余烬。
	SetPartColor(fireball, FMath::Lerp(FLinearColor(1.f, 0.6f, 0.1f), FLinearColor(0.6f, 0.08f, 0.f), u));
	SetPartEmissive(fireball, FMath::Lerp(12.f, 1.5f, u));
	SetPartOpacity(fireball, 1.f - FMath::SmoothStep(0.45f, 1.f, u));
}

void AForeverExplosionEffect::UpdateRing()
{
	float u = age / ringDuration;
	if (u >= 1.f) {
		ring.mesh->SetVisibility(false);
		return;
	}

	float radius = ringRadius * effectScale * (1.f - FMath::Pow(1.f - u, 3.f));
	float diameterScale = radius * 2.f / EXPLOSION_SPHERE_DIAMETER;
	ring.mesh->SetWorldScale3D(FVector(diameterScale, diameterScale, 0.02f));
	SetPartEmissive(ring, 6.f);
	SetPartOpacity(ring, FMath::Square(1.f - u));
}

void AForeverExplosionEffect::UpdateFlash()
{
	float u = age / flashDuration;
	if (u >= 1.f) {
		flash->SetIntensity(0.f);
		return;
	}
	flash->SetIntensity(flashIntensity * effectScale * FMath::Square(1.f - u));
}

void AForeverExplosionEffect::UpdateSparks(float deltaSeconds)
{
	for (int32 i = 0; i < sparks.Num(); i++) {
		if (age >= sparkLife[i]) {
			sparks[i].mesh->SetVisibility(false);
			continue;
		}
		float life = 1.f - age / sparkLife[i];
		SetPartEmissive(sparks[i], 15.f);
		SetPartOpacity(sparks[i], FMath::Clamp(life * 2.f, 0.f, 1.f));
		sparkVelocity[i].Z -= 980.f * deltaSeconds;
		sparkPosition[i] += sparkVelocity[i] * deltaSeconds;
		sparks[i].mesh->SetRelativeLocation(sparkPosition[i]);
	}
}

void AForeverExplosionEffect::UpdateSmoke()
{
	for (int32 i = 0; i < smokes.Num(); i++) {
		float smokeAge = age - smokeDelay[i];
		if (smokeAge < 0.f) continue;

		float u = smokeAge / (totalDuration - smokeDelay[i]);
		smokes[i].mesh->SetVisibility(true);
		smokes[i].mesh->SetRelativeLocation(smokeOffset[i] + FVector(0.f, 0.f, smokeRiseSpeed * effectScale * smokeAge));
		SetPartRadius(smokes[i], smokeRadius * effectScale * FMath::Lerp(0.4f, 1.f, u));
		// 开头淡入、后半段淡出。
		SetPartOpacity(smokes[i], smokeOpacity * FMath::SmoothStep(0.f, 0.15f, u) * (1.f - FMath::SmoothStep(0.5f, 1.f, u)));
		SetPartColor(smokes[i], FMath::Lerp(FLinearColor(0.12f, 0.11f, 0.1f), FLinearColor(0.05f, 0.05f, 0.05f), u) * smokeBrightness);
	}
}
