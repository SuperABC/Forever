#include "Element/BoatElement.h"

#include "Framework/CoreGuard.h"

#include "Framework/ForeverFrameworkActor.h"
#include "Input/ForeverKeyBindingSubsystem.h"
#include "Water/WaterQuery.h"

#include "Camera/CameraComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

#include "map/map.h"

namespace {
	// AddOnScreenDebugMessage的key：同一个key新消息会替换旧消息。9000/200/201/202/210已被占用(见PlaneElement.cpp)。
	constexpr int32 kBoatHudKey = 211;
	constexpr float kBoatCmPerSecToKmh = 0.036f; // cm/s -> m/s(0.01) -> km/h(3.6)
	// 球体网格(/Engine/BasicShapes/Sphere)直径100cm；烟雾材质(参数Color/Opacity)和爆炸特效共用。
	constexpr float kSphereDiameter = 100.f;
}

ABoatElement::ABoatElement()
{
	// 船没有轮子：基类建好的4个轮子清掉(Chaos轮式载具组件留着，基类的输入/驻车调用都是普通成员调用，没轮子无害)。
	vehicleMovement->WheelSetups.Reset();
	// 纯C++驱动轮子的动画实例对船没用。
	carMesh->SetAnimInstanceClass(nullptr);

	// 重心下移50cm(基类AVehicleElement的蓝图变量centerOfMassOffset，BeginPlay时叠加到物理资产算出的重心上，Z取负=下移)：
	// 物理资产是包住船体的一个盒子，重心在盒子中央，比真实船高，高速转弯/被托起时容易侧翻。蓝图Details的Mass分类里可以改。
	centerOfMassOffset = FVector(0.f, 0.f, -50.f);

	// 追尾视角：弹簧臂跟鼠标环视(同车)，船大，臂长拉远。
	cameraBoom->TargetArmLength = 1800.f;
	cameraBoom->SetRelativeLocation(FVector(0.f, 0.f, 300.f));

	// 浪花粒子池：球体网格+烟雾材质，世界空间(不跟船)，没有碰撞和阴影，默认隐藏。
	static ConstructorHelpers::FObjectFinder<UStaticMesh> sphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> smokeFinder(TEXT("/Game/Asset/Effects/Explosion/ExplosionSmoke.ExplosionSmoke"));
	for (int32 i = 0; i < kWakeParticleCount; i++) {
		UStaticMeshComponent* mesh = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Wake%d"), i));
		mesh->SetupAttachment(RootComponent);
		mesh->SetAbsolute(true, true, true);
		mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		mesh->SetGenerateOverlapEvents(false);
		mesh->SetCastShadow(false);
		mesh->SetVisibility(false);
		if (sphereFinder.Succeeded()) mesh->SetStaticMesh(sphereFinder.Object);
		if (smokeFinder.Succeeded()) mesh->SetMaterial(0, smokeFinder.Object);
		wakeMeshes.Add(mesh);
	}
}

void ABoatElement::BeginPlay()
{
	Super::BeginPlay();

	BuildAutoPoints();
	if (carMesh) UE_LOG(LogTemp, Log, TEXT("Boat mass: %s = %.0f kg (displacementMassKg=%.0f, 0=取当前质量)"), *GetName(), carMesh->GetMass(), displacementMassKg);

	// 每个粒子一个材质实例：各自的颜色/不透明度。
	wakeMaterials.Reset();
	wakeParticles.SetNum(kWakeParticleCount);
	for (UStaticMeshComponent* mesh : wakeMeshes) {
		wakeMaterials.Add(mesh && mesh->GetMaterial(0) ? mesh->CreateAndSetMaterialInstanceDynamic(0) : nullptr);
	}

	// Chaos轮式载具对船没用(没有轮子)，关掉它的Tick省掉每帧开销。
	if (vehicleMovement) vehicleMovement->SetComponentTickEnabled(false);
}

void ABoatElement::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	throttleCmd = 0.f;
	steerCmd = 0.f;
}

void ABoatElement::UnPossessed()
{
	// 没人开了就松油门、回舵；浮力和阻尼照常(Tick里无论有没有人开都算)，船停在水上不会飘走也不会沉。
	throttleCmd = 0.f;
	steerCmd = 0.f;
	Super::UnPossessed();
}

void ABoatElement::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// 故意绕过AVehicleElement::SetupPlayerInputComponent(它把W/S绑成车的油门/刹车，并绑手刹)，直接用APawn的。
	APawn::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* enhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!enhancedInput) return;

	enhancedInput->BindAction(moveAction, ETriggerEvent::Triggered, this, &ABoatElement::MoveBoat);
	enhancedInput->BindAction(moveAction, ETriggerEvent::Completed, this, &ABoatElement::StopMoveBoat);
	enhancedInput->BindAction(moveAction, ETriggerEvent::Canceled, this, &ABoatElement::StopMoveBoat);
	enhancedInput->BindAction(lookAction, ETriggerEvent::Triggered, this, &ABoatElement::LookBoat);
	enhancedInput->BindAction(mouseLookAction, ETriggerEvent::Triggered, this, &ABoatElement::LookBoat);

	if (UGameInstance* gameInstance = GetGameInstance()) {
		if (UForeverKeyBindingSubsystem* keyBindings = gameInstance->GetSubsystem<UForeverKeyBindingSubsystem>()) {
			enhancedInput->BindAction(keyBindings->GetAction(TEXT("ExitVehicle")), ETriggerEvent::Started, this, &ABoatElement::BoatExit);
		}
	}
}

void ABoatElement::MoveBoat(const FInputActionValue& value)
{
	const FVector2D movement = value.Get<FVector2D>();
	throttleCmd = movement.Y; // W(Y>0)=前进，S(Y<0)=后退
	steerCmd = movement.X;    // D(X>0)=右转
}

void ABoatElement::StopMoveBoat(const FInputActionValue& value)
{
	throttleCmd = 0.f;
	steerCmd = 0.f;
}

void ABoatElement::LookBoat(const FInputActionValue& value)
{
	Look(value); // 基类：只转相机(cameraBoom跟控制器旋转)，不转船
}

void ABoatElement::BoatExit()
{
	ExitVehicle();
}

Map* ABoatElement::GetCoreMap()
{
	AForeverFrameworkActor* framework = cachedFramework.Get();
	if (!framework) {
		framework = Cast<AForeverFrameworkActor>(UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()));
		cachedFramework = framework;
	}
	return framework ? framework->GetMap() : nullptr;
}

void ABoatElement::BuildAutoPoints()
{
	activeFloatPoints = floatPoints;
	activeThrustPoint = thrustPoint;
	if (!activeFloatPoints.IsEmpty() && bCustomThrustPoint) return;

	// 自动点：骨骼网格包围盒(网格局部空间)的船底。船头+X。
	FVector origin = FVector::ZeroVector;
	FVector extent = FVector(1200.f, 350.f, 200.f); // 取不到网格时的兜底(约25m x 7m)
	if (carMesh && carMesh->GetSkeletalMeshAsset()) {
		const FBoxSphereBounds bounds = carMesh->GetSkeletalMeshAsset()->GetBounds();
		origin = bounds.Origin;
		extent = bounds.BoxExtent;
	}
	const float keelZ = origin.Z - extent.Z;
	if (activeFloatPoints.IsEmpty()) {
		activeFloatPoints = {
			FVector(origin.X + extent.X * 0.8f, origin.Y, keelZ),
			FVector(origin.X - extent.X * 0.8f, origin.Y, keelZ),
			FVector(origin.X + extent.X * 0.4f, origin.Y + extent.Y * 0.7f, keelZ),
			FVector(origin.X + extent.X * 0.4f, origin.Y - extent.Y * 0.7f, keelZ),
			FVector(origin.X - extent.X * 0.4f, origin.Y + extent.Y * 0.7f, keelZ),
			FVector(origin.X - extent.X * 0.4f, origin.Y - extent.Y * 0.7f, keelZ),
		};
	}
	if (!bCustomThrustPoint) activeThrustPoint = FVector(origin.X - extent.X * 0.8f, origin.Y, keelZ);
}

bool ABoatElement::SamplePoint(const FVector& world, double time, float& outDepth, float& outSurfaceZ)
{
	float surfaceZ = 0.f, bedZ = 0.f;
	if (!ForeverWater::QuerySurface(GetCoreMap(), world, time, surfaceZ, bedZ)) return false;
	outSurfaceZ = surfaceZ;
	outDepth = surfaceZ - world.Z;
	return true;
}

void ABoatElement::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime); // 把位置写回Core，见AVehicleElement::Tick
	CORE_GUARD_BEGIN("ABoatElement::Tick")

	if (!carMesh) return;

	// IsSimulatingPhysics()对骨骼网格按根骨查body，物理资产配置不同时可能误判，用IsAnySimulatingPhysics()。
	if (!carMesh->IsAnySimulatingPhysics()) {
		if (IsPlayerControlled() && !bWarnedNotSimulating) {
			bWarnedNotSimulating = true;
			UE_LOG(LogTemp, Warning, TEXT("ABoatElement: carMesh没有在模拟物理，浮力/驾驶不生效。检查物理资产的Root body是否Simulated。"));
		}
		return;
	}
	const float mass = carMesh->GetMass();
	if (mass <= 0.f || activeFloatPoints.IsEmpty()) return;

	const double time = GetWorld()->GetTimeSeconds();
	const FTransform transform = carMesh->GetComponentTransform();
	const FVector forward = transform.GetUnitAxis(EAxis::X);
	const FVector right = transform.GetUnitAxis(EAxis::Y);
	const FVector up = transform.GetUnitAxis(EAxis::Z);
	const FVector velocity = carMesh->GetPhysicsLinearVelocity();
	const float gravity = FMath::Abs(GetWorld()->GetGravityZ());
	const float pointCount = static_cast<float>(activeFloatPoints.Num());
	const float safeDraft = FMath::Max(draft, 1.f);
	const float displacement = displacementMassKg > 0.f ? displacementMassKg : mass;

	// ---- 浮力：每个浮点各算一份向上的力，点之间的差产生俯仰/横滚恢复力矩 ----
	// 加速度 = g*沉入比/N - 阻尼*垂直速度/N：N个点都沉入draft时合力刚好等于重力(平衡吃水)，
	// 阻尼项让船落水后不会无限弹；水不会往下拉船，所以每点加速度不为负。
	float wetness = 0.f; // 平均入水程度[0,1]，给阻力/转向缩放(船整个出水就没有水的作用)
	for (const FVector& localPoint : activeFloatPoints) {
		const FVector world = transform.TransformPosition(localPoint);
		float depth = 0.f, surfaceZ = 0.f;
		if (!SamplePoint(world, time, depth, surfaceZ) || depth <= 0.f) continue;

		wetness += FMath::Clamp(depth / safeDraft, 0.f, 1.f);
		const float ratio = FMath::Clamp(depth / safeDraft, 0.f, maxSubmergeRatio);
		const float verticalSpeed = carMesh->GetPhysicsLinearVelocityAtPoint(world).Z;
		// 浮力(弹簧)按排水量算：只跟沉入深度有关，和船的质量无关；阻尼项是加速度制(乘船的质量)。
		const float force = FMath::Max(0.f, (displacement * gravity * ratio - mass * floatDamping * verticalSpeed) / pointCount);
		carMesh->AddForceAtLocation(FVector(0.f, 0.f, force), world);
	}
	wetness /= pointCount;

	// ---- 推进：只有螺旋桨(推力点)在水里才有推力 ----
	const FVector thrustWorld = transform.TransformPosition(activeThrustPoint);
	float propellerWet = 0.f;
	{
		float depth = 0.f, surfaceZ = 0.f;
		if (SamplePoint(thrustWorld, time, depth, surfaceZ)) propellerWet = FMath::Clamp(depth / safeDraft, 0.f, 1.f);
	}

	const FVector worldUp(0.f, 0.f, 1.f);
	// 船身倾斜角(船体"上"和世界上的夹角)：下面推力的上抬分量、稳定力矩都要用。
	const float tiltRadians = FMath::Acos(FMath::Clamp(FVector::DotProduct(up, worldUp), -1.f, 1.f));

	if (throttleCmd != 0.f && propellerWet > 0.f) {
		const float thrust = mass * maxThrustAccel * propellerWet * (throttleCmd > 0.f ? throttleCmd : throttleCmd * reverseThrustScale);
		// 水平分量：作用点沿船体上下方向的力臂只保留thrustLeverRatio(0=作用在重心高度，完全不产生俯仰；1=真的在船底，
		// 力臂约等于重心离船底的高度，大船会被推得"竖起来")。船尾位置(前后/左右)保持不变。
		const FVector centerOfMass = carMesh->GetCenterOfMass();
		const FVector leverArm = thrustWorld - centerOfMass;
		const FVector thrustApplyPoint = thrustWorld - up * (FVector::DotProduct(leverArm, up) * (1.f - thrustLeverRatio));
		carMesh->AddForceAtLocation(forward * thrust, thrustApplyPoint);
		// 向上分量：沿世界上方向(不跟船体倾斜走，否则船头一抬它就变成往后推，越抬越翻)，作用在重心(不产生俯仰)，
		// 船体倾斜越大越弱，超过maxLiftTilt就没有。托起后浮点出水、螺旋桨出水推力变小，落回水里再托起——快艇一跳一跳。
		if (throttleCmd > 0.f) {
			const float tiltFade = FMath::Clamp(1.f - FMath::RadiansToDegrees(tiltRadians) / FMath::Max(maxLiftTilt, 1.f), 0.f, 1.f);
			carMesh->AddForce(worldUp * (thrust * thrustUpRatio * tiltFade));
		}
	}

	// ---- 阻力：纵向小、横向大，按入水程度缩放 ----
	const float forwardSpeed = FVector::DotProduct(velocity, forward);
	const float lateralSpeed = FVector::DotProduct(velocity, right);
	if (wetness > 0.f) {
		const float forwardDecel = forwardDrag * forwardSpeed + forwardDragQuad * FMath::Abs(forwardSpeed) * forwardSpeed;
		const float lateralDecel = lateralDrag * lateralSpeed + lateralDragQuad * FMath::Abs(lateralSpeed) * lateralSpeed;
		carMesh->AddForce((-forward * forwardDecel - right * lateralDecel) * (mass * wetness));
	}

	// ---- 转向：目标转向角速度正比于沿船头方向的速度(停着=0；倒车方向反过来) ----
	const FVector angularVelocity = carMesh->GetPhysicsAngularVelocityInRadians();
	FVector angularAccel = FVector::ZeroVector;
	if (wetness > 0.f) {
		// UE里绕+Z的角速度=向右转(偏航角增大)。
		const float speedFactor = FMath::Clamp(forwardSpeed / FMath::Max(steerFullSpeed, 1.f), -1.f, 1.f);
		const float targetYawRate = FMath::DegreesToRadians(maxYawRate) * steerCmd * speedFactor;
		angularAccel.Z = (targetYawRate - angularVelocity.Z) * yawResponse * wetness;
	}
	{
		// 防翻船：船体"上"向世界上方向回正的角加速度(叉积幅值=sin倾斜角)，加上俯仰/横滚阻尼(船体局部X/Y轴)。
		// 浮力靠船底几个浮点出水就失效，船头一抬、一跳起来就没有恢复力了，所以这里不完全随入水程度缩放(空中也保留一部分)。
		const float stability = FMath::Max(wetness, 0.3f);
		angularAccel += FVector::CrossProduct(up, worldUp) * (uprightAccel * stability);
		const FVector localAngular = transform.InverseTransformVectorNoScale(angularVelocity);
		const FVector localDamping(-localAngular.X * rollPitchDamping, -localAngular.Y * rollPitchDamping, 0.f);
		angularAccel += transform.TransformVectorNoScale(localDamping) * stability;
	}
	carMesh->AddTorqueInRadians(angularAccel, NAME_None, true);

	UpdateWake(DeltaTime, transform, velocity, time, propellerWet);
	UpdateHud(velocity.Size());
	CORE_GUARD_END
}

void ABoatElement::UpdateWake(float deltaTime, const FTransform& transform, const FVector& velocity, double time, float propellerWet)
{
	if (wakeParticles.Num() != wakeMeshes.Num()) return;

	const FVector forward = transform.GetUnitAxis(EAxis::X);
	const FVector right = transform.GetUnitAxis(EAxis::Y);
	const float speed = velocity.Size();
	const float gravity = FMath::Abs(GetWorld()->GetGravityZ());

	// 发射：速度越快越密，螺旋桨出水(跳起来)就停。起点在船尾水面高度。
	const float speedAlpha = FMath::Clamp((speed - wakeMinSpeed) / FMath::Max(wakeFullSpeed - wakeMinSpeed, 1.f), 0.f, 1.f);
	if (speedAlpha > 0.f && propellerWet > 0.f) {
		wakeEmitAccumulator += wakeRate * speedAlpha * deltaTime;
		const FVector sourceWorld = transform.TransformPosition(activeThrustPoint + wakeOffset);
		float depth = 0.f, surfaceZ = 0.f;
		if (SamplePoint(sourceWorld, time, depth, surfaceZ)) {
			while (wakeEmitAccumulator >= 1.f) {
				wakeEmitAccumulator -= 1.f;
				FWakeParticle* slot = nullptr;
				for (FWakeParticle& particle : wakeParticles) {
					if (!particle.bActive) { slot = &particle; break; }
				}
				if (!slot) break; // 池满了就丢掉这一批
				slot->bActive = true;
				slot->age = 0.f;
				slot->life = wakeLife * FMath::FRandRange(0.7f, 1.2f);
				slot->size = wakeSize * FMath::FRandRange(0.6f, 1.f);
				slot->position = FVector(sourceWorld.X, sourceWorld.Y, surfaceZ) + right * FMath::FRandRange(-120.f, 120.f);
				// 向船尾后方抛出：船尾后方+左右散开+上抬。
				slot->velocity = velocity * 0.3f - forward * (80.f + speed * 0.15f)
					+ right * FMath::FRandRange(-250.f, 250.f) + FVector(0.f, 0.f, FMath::FRandRange(200.f, 420.f));
			}
		}
	}
	else {
		wakeEmitAccumulator = 0.f;
	}

	// 更新粒子：重力、落到水面或寿命到就回收；半径越来越大，不透明度先升后降。
	for (int32 i = 0; i < wakeParticles.Num(); i++) {
		FWakeParticle& particle = wakeParticles[i];
		UStaticMeshComponent* mesh = wakeMeshes[i];
		if (!mesh) continue;
		if (!particle.bActive) {
			if (mesh->IsVisible()) mesh->SetVisibility(false);
			continue;
		}

		particle.age += deltaTime;
		particle.velocity.Z -= gravity * deltaTime;
		particle.position += particle.velocity * deltaTime;

		float depth = 0.f, surfaceZ = 0.f;
		const bool bHasWater = SamplePoint(particle.position, time, depth, surfaceZ);
		if (particle.age >= particle.life || (bHasWater && depth > 20.f && particle.velocity.Z < 0.f)) {
			particle.bActive = false;
			mesh->SetVisibility(false);
			continue;
		}

		const float u = particle.age / particle.life;
		const float diameter = particle.size * FMath::Lerp(0.4f, 1.f, u);
		mesh->SetWorldLocation(particle.position);
		mesh->SetWorldScale3D(FVector(diameter / kSphereDiameter));
		if (!mesh->IsVisible()) mesh->SetVisibility(true);
		if (UMaterialInstanceDynamic* material = wakeMaterials.IsValidIndex(i) ? wakeMaterials[i].Get() : nullptr) {
			material->SetVectorParameterValue(TEXT("Color"), wakeColor);
			material->SetScalarParameterValue(TEXT("Opacity"), wakeOpacity * FMath::SmoothStep(0.f, 0.15f, u) * (1.f - FMath::SmoothStep(0.4f, 1.f, u)));
		}
	}
}

void ABoatElement::UpdateHud(float speedCmPerSec)
{
	if (!GEngine || !IsPlayerControlled()) return;

	GEngine->AddOnScreenDebugMessage(kBoatHudKey, 0.1f, FColor::White,
		FString::Printf(TEXT("速度 %.0f km/h"), speedCmPerSec * kBoatCmPerSecToKmh));
}
