#include "Player/ForeverBulletSubsystem.h"

#include "Player/ForeverExplosionEffect.h"
#include "Element/CitizenElement.h"
#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverStoryFrameworkComponent.h"
#include "Framework/ForeverPopulaceFrameworkComponent.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Stats/Stats.h"

#include "story/event.h"
#include "populace/citizen.h"

using namespace std;

// 曳光弹用爆炸特效的火焰材质(Additive自发光，已抵消自动曝光，参数Color/EmissiveStrength/Opacity)。
#define BULLET_TRACER_MESH_PATH TEXT("/Engine/BasicShapes/Sphere.Sphere")
#define BULLET_TRACER_MATERIAL_PATH TEXT("/Game/Asset/Materials/ExplosionFire.ExplosionFire")

namespace {
	// 仿真固定子步(秒)。子弹最快约700m/s，一个子步走约6m，线段扫掠不会穿透。
	constexpr float kSubStep = 1.f / 120.f;
	// 单次Tick最多推进的时间，卡顿帧不会让子弹一次飞出一大截(也避免一帧里做太多子步)。
	constexpr float kMaxTickTime = 0.1f;

	// 曳光弹外观：沿飞行方向拉长的球，尾巴长度不超过已飞距离(刚出膛不拖到玩家脸上)。
	// 尾巴长度取"最短长度"和"本帧位移"中较大者：子弹一帧飞好几米，尾巴不够长就只会在画面上闪成一个点。
	constexpr float kTracerLength = 300.f;   // cm，最短长度
	constexpr float kTracerThickness = 1.5f; // cm
	constexpr float kTracerMinLength = 10.f;
	constexpr float kTracerEmissive = 4.f;   // 画面显示亮度，见ForeverExplosionEffect.md；火焰材质有Fresnel衰减，细长体边缘会变暗，所以取大
}

float FBullet::SampleDamage(float distance) const {
	if (maxRange <= 0.f) return damageCurve[0];
	float u = FMath::Clamp(distance / maxRange, 0.f, 1.f) * (DamageSamples - 1);
	int32 index = FMath::Min(FMath::FloorToInt(u), DamageSamples - 2);
	return FMath::Lerp(damageCurve[index], damageCurve[index + 1], u - index);
}

bool UForeverBulletSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const {
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UForeverBulletSubsystem::Deinitialize() {
	bullets.Empty();
	tracerHost = nullptr;
	tracerMesh = nullptr;
	Super::Deinitialize();
}

TStatId UForeverBulletSubsystem::GetStatId() const {
	RETURN_QUICK_DECLARE_CYCLE_STAT(UForeverBulletSubsystem, STATGROUP_Tickables);
}

void UForeverBulletSubsystem::AddBullet(FBullet&& bullet) {
	bullets.Add(MoveTemp(bullet));
}

void UForeverBulletSubsystem::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	UWorld* world = GetWorld();
	if (!world) return;

	if (bullets.IsEmpty()) {
		if (tracerHasInstances) UpdateTracer();
		return;
	}

	// DeltaTime已经带全局时间膨胀：商店/地图暂停时子弹跟着冻结，投掷列表的慢动作里子弹也跟着变慢。
	float time = FMath::Min(DeltaTime, kMaxTickTime);
	int32 steps = FMath::Max(1, FMath::CeilToInt(time / kSubStep));
	float h = time / steps;
	float gravityZ = world->GetGravityZ();

	for (int32 i = bullets.Num() - 1; i >= 0; --i) {
		FBullet& bullet = bullets[i];

		FCollisionQueryParams params;
		for (const TWeakObjectPtr<AActor>& actor : bullet.ignored) {
			if (AActor* ignoredActor = actor.Get()) params.AddIgnoredActor(ignoredActor);
		}

		bool finished = false;
		bullet.lastMove = 0.f;
		for (int32 step = 0; step < steps && !finished; ++step) {
			bullet.velocity.Z += gravityZ * bullet.gravityScale * h;
			FVector from = bullet.position;
			FVector to = from + bullet.velocity * h;

			float segment = FVector::Dist(from, to);
			float remaining = bullet.maxRange - bullet.traveled;
			if (segment >= remaining) {
				// 本子步飞出射程：只扫到射程终点，扫完这一段就结束。
				to = from + (to - from).GetSafeNormal() * remaining;
				segment = remaining;
				finished = true;
			}

			// 必须用ECC_Pawn：本项目Pawn碰撞预设忽略ECC_Visibility，射线会穿过市民胶囊体(见CONVENTIONS第11节)。
			FHitResult hit;
			if (world->LineTraceSingleByChannel(hit, from, to, ECC_Pawn, params)) {
				// 伤害按命中点的实际飞行距离衰减。打到墙/地面不产生任何效果，子弹直接消失。
				float distance = bullet.traveled + hit.Distance;
				ApplyCitizenHit(world, bullet.shooter.Get(), bullet.weaponType, bullet.SampleDamage(distance), hit);
				finished = true;
				break;
			}

			bullet.traveled += segment;
			bullet.lastMove += segment;
			bullet.position = to;
		}

		if (finished) bullets.RemoveAtSwap(i);
	}

	UpdateTracer();
}

void UForeverBulletSubsystem::EnsureTracer() {
	if (tracerMesh) return;
	UWorld* world = GetWorld();
	if (!world) return;

	FActorSpawnParameters spawnParams;
	spawnParams.ObjectFlags |= RF_Transient;
	tracerHost = world->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, spawnParams);
	if (!tracerHost) return;

	USceneComponent* root = NewObject<USceneComponent>(tracerHost, TEXT("Root"));
	tracerHost->SetRootComponent(root);
	root->RegisterComponent();

	tracerMesh = NewObject<UInstancedStaticMeshComponent>(tracerHost, TEXT("Tracer"));
	tracerMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, BULLET_TRACER_MESH_PATH));
	tracerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	tracerMesh->SetGenerateOverlapEvents(false);
	tracerMesh->SetCastShadow(false);
	tracerMesh->SetCanEverAffectNavigation(false);
	tracerMesh->SetupAttachment(root);
	tracerMesh->RegisterComponent();

	if (UMaterialInterface* material = LoadObject<UMaterialInterface>(nullptr, BULLET_TRACER_MATERIAL_PATH)) {
		tracerMesh->SetMaterial(0, material);
		if (UMaterialInstanceDynamic* instance = tracerMesh->CreateAndSetMaterialInstanceDynamic(0)) {
			instance->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.8f, 0.3f));
			instance->SetScalarParameterValue(TEXT("EmissiveStrength"), kTracerEmissive);
			instance->SetScalarParameterValue(TEXT("Opacity"), 1.f);
		}
	}
}

void UForeverBulletSubsystem::UpdateTracer() {
	if (bullets.IsEmpty() && !tracerHasInstances) return;
	EnsureTracer();
	if (!tracerMesh) return;

	tracerMesh->ClearInstances();
	tracerHasInstances = false;

	TArray<FTransform> transforms;
	transforms.Reserve(bullets.Num());
	for (const FBullet& bullet : bullets) {
		float length = FMath::Min(FMath::Max(kTracerLength, bullet.lastMove), bullet.traveled);
		if (length < kTracerMinLength) continue;
		FVector direction = bullet.velocity.GetSafeNormal();
		FQuat rotation = FRotationMatrix::MakeFromX(direction).ToQuat();
		// 球体网格直径100cm，X方向拉成尾巴长度；尾巴在子弹位置身后。
		FVector scale(length / 100.f, kTracerThickness / 100.f, kTracerThickness / 100.f);
		transforms.Add(FTransform(rotation, bullet.position - direction * (length * 0.5f), scale));
	}
	if (!transforms.IsEmpty()) {
		tracerMesh->AddInstances(transforms, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true);
		tracerHasInstances = true;
	}
}

void UForeverBulletSubsystem::ApplyCitizenHit(UWorld* world, AActor* shooter, const string& weaponType, float damage,
	const FHitResult& hit) {
	// 只有打中ACitizenElement才算命中。
	ACitizenElement* hitCitizen = Cast<ACitizenElement>(hit.GetActor());
	if (!hitCitizen || !world) return;

	Citizen* target = hitCitizen->GetCitizen();
	if (!target || target->IsDead()) return;

	target->TakeDamage(damage);

	// 命中位置的爆炸特效(纯视觉)。世界坐标生成，不依赖citizen存活，所以放在死亡处理之前也安全。
	AForeverExplosionEffect::Spawn(world, hit.ImpactPoint, hit.ImpactNormal);

	if (!target->IsDead()) return;

	FString deadName = UTF8_TO_TCHAR(target->GetName().c_str());
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(world, AForeverFrameworkActor::StaticClass()));
	if (!framework) return;

	if (framework->GetStoryFramework()) {
		// 击杀信息：凶手(开枪的市民)、武器种类、命中部位、致死伤害、凶手是否可见、死亡地址。
		// 子弹飞行期间射手可能已经失效，此时凶手为空。
		ACitizenElement* killerElement = Cast<ACitizenElement>(shooter);
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
			visible = !world->LineTraceTestByChannel(hitCitizen->GetActorLocation(),
				killerElement->GetActorLocation(), ECC_Visibility, sightParams);
		}

		CitizenDeceaseEvent event(std::string(TCHAR_TO_UTF8(*deadName)), "player",
			killer ? killer->GetName() : std::string(), weaponType, part, damage, visible,
			framework->DescribeLocation(hitCitizen->GetActorLocation()));
		framework->GetStoryFramework()->BroadcastCitizenDecease(event);
	}
	if (framework->GetPopulaceFramework()) {
		framework->GetPopulaceFramework()->HandleCitizenDeath(target);
	}
}
