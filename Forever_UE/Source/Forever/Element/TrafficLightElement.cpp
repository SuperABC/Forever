#include "Element/TrafficLightElement.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

#include "traffic/junction.h"

namespace {
	// 亮着的灯写进逐实例数据0的值。M_TrafficLamp 的发光 = 灯色 × lerp(OffIntensity=0, OnIntensity=100, 数据0)，
	// lerp 不截断，所以发光强度 = 100 × 这个值。工程曝光很重(名字标签也要 10000 才看得清)，白天要到这个量级。
	constexpr float kLampLitValue = 100.f;

	// 模型由 Tools/TrafficLight 从 /Game/3rdParty/Retro_traffic_light 拆出来(框架+三个灯罩)，灯罩已经配好 MI_TrafficLamp_*。
	UInstancedStaticMeshComponent* CreateLightMesh(AActor* owner, const TCHAR* name, UStaticMesh* mesh, bool bLamp) {
		UInstancedStaticMeshComponent* component = owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(name);
		component->SetupAttachment(owner->GetRootComponent());
		if (mesh) component->SetStaticMesh(mesh);
		component->SetMobility(EComponentMobility::Movable);
		if (bLamp) {
			// 灯罩是贴在灯箱里的平面圆片：不投影、不要碰撞，亮灭读逐实例数据0。
			component->NumCustomDataFloats = 1;
			component->SetCastShadow(false);
			component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		else {
			// 框架挡相机(弹簧臂不穿过灯杆)和玩家，和家具一样；车走路面碰不到它。
			component->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		}
		return component;
	}
}

ATrafficLightElement::ATrafficLightElement() {
	// 信号由UForeverTrafficLightFrameworkComponent每帧推过来，自己不Tick。
	PrimaryActorTick.bCanEverTick = false;
	USceneComponent* root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> frameFinder(TEXT("/Game/Asset/TrafficLight/SM_TrafficLight_Frame.SM_TrafficLight_Frame"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> redFinder(TEXT("/Game/Asset/TrafficLight/SM_TrafficLight_Red.SM_TrafficLight_Red"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> yellowFinder(TEXT("/Game/Asset/TrafficLight/SM_TrafficLight_Yellow.SM_TrafficLight_Yellow"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> greenFinder(TEXT("/Game/Asset/TrafficLight/SM_TrafficLight_Green.SM_TrafficLight_Green"));
	frameMesh = CreateLightMesh(this, TEXT("Frame"), frameFinder.Object, false);
	redMesh = CreateLightMesh(this, TEXT("Red"), redFinder.Object, true);
	yellowMesh = CreateLightMesh(this, TEXT("Yellow"), yellowFinder.Object, true);
	greenMesh = CreateLightMesh(this, TEXT("Green"), greenFinder.Object, true);
}

void ATrafficLightElement::Build(int32 inJunctionId, const TArray<FTransform>& transforms, const TArray<int32>& approaches) {
	junctionId = inJunctionId;
	lightApproach = approaches;
	lightSignal.Init(static_cast<uint8>(JunctionTraffic::Signal::None), transforms.Num());
	for (UInstancedStaticMeshComponent* component : { frameMesh.Get(), redMesh.Get(), yellowMesh.Get(), greenMesh.Get() }) {
		if (!component) continue;
		component->ClearInstances();
		component->AddInstances(transforms, false, true); // 世界坐标
	}
	for (int32 i = 0; i < transforms.Num(); i++) SetLamp(i, static_cast<uint8>(JunctionTraffic::Signal::None));
}

void ATrafficLightElement::RefreshSignals(const JunctionTraffic& traffic, double now) {
	for (int32 i = 0; i < lightApproach.Num(); i++) {
		const uint8 signal = static_cast<uint8>(traffic.GetSignal(junctionId, lightApproach[i], now));
		if (signal == lightSignal[i]) continue;
		lightSignal[i] = signal;
		SetLamp(i, signal);
	}
}

void ATrafficLightElement::SetLamp(int32 light, uint8 signal) {
	const JunctionTraffic::Signal value = static_cast<JunctionTraffic::Signal>(signal);
	// 只在信号变化时调(一盏灯每几秒一次)，每次都直接标脏渲染状态。
	if (redMesh) redMesh->SetCustomDataValue(light, 0, value == JunctionTraffic::Signal::Red ? kLampLitValue : 0.f, true);
	if (yellowMesh) yellowMesh->SetCustomDataValue(light, 0, value == JunctionTraffic::Signal::Yellow ? kLampLitValue : 0.f, true);
	if (greenMesh) greenMesh->SetCustomDataValue(light, 0, value == JunctionTraffic::Signal::Green ? kLampLitValue : 0.f, true);
}
