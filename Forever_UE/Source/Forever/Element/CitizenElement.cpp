#include "Element/CitizenElement.h"

#include "Framework/ForeverPopulaceFrameworkComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

#include "populace/citizen.h"
#include "map/building.h"
#include "map/room.h"
#include "common/utility.h"

// 地图单位换算——和BuildingElement.cpp的BUILDING_WORLD_SCALE同一个值，这次按本文件既有
// 约定各自维护一份，不额外抽公共头(这个约定详见BuildingElement.cpp顶部注释)。
#define CITIZEN_WORLD_SCALE 1000.f
// 楼层Z起算的"地坪"偏移(UE单位)，和BUILDING_HEIGHT_EPSILON同一个值/同一个理由。
#define CITIZEN_HEIGHT_EPSILON 10.f
// 地板slab厚度(UE单位)——照抄BuildingElement.cpp里kSlabThickness=0.02f(地图单位)*
// BUILDING_WORLD_SCALE。BuildingElement.cpp把Ground slab的中心摆在
// "floorBaseZ对应的楼层底部+半个slab厚度"，也就是说楼层真正能站人的地板表面(slab顶面)
// 比floorBaseZ换算出来的"楼层底部"还要再高一个完整slab厚度——citizen的落脚点必须按这个
// 顶面算，不能直接用floorBaseZ换算的楼层底部，否则会陷进地板slab里(实测发现的bug：陷入
// 深度正好是这个厚度量级)。
#define CITIZEN_GROUND_SLAB_THICKNESS 20.f
// 靠近检测碰撞盒尺寸(UE单位，不走地图单位换算——这个盒子纯粹是UE层的装饰性判定，和
// Core侧map数据无关)：水平半宽~2.5m，垂直半高~1m。
#define CITIZEN_PROXIMITY_HALF_XY 250.f
#define CITIZEN_PROXIMITY_HALF_Z 100.f

using namespace std;

namespace {
	// 把Floor局部坐标(原点在楼体左下角，未旋转)转成世界坐标——和BuildingElement.cpp里
	// ComputeWorldPosition同一个公式，这次按本文件既有约定各自维护一份。**函数名不能叫
	// 同一个名字**：UBT的unity build会把这个模块(Forever)一批.cpp文件拼进同一个生成的
	// Module.Forever.cpp里编译，原本"各自的匿名namespace互不可见"的隔离性在这种情况下
	// 失效(两份匿名namespace被塞进了同一个真正的翻译单元)，两个同名同签名的函数会直接
	// 报重定义错误(实测踩过：main分支代码不变、只是Git切了一次分支导致UBT自适应unity
	// 排除名单重算，BuildingElement.cpp和CitizenElement.cpp第一次被一起打进同一个unity
	// blob就编译失败了)——本文件的版本改叫`ComputeCitizenWorldPosition`避开这个问题，
	// 其余每个文件仍然可以有自己的私有helper，只要函数名在整个Forever模块内不重复即可。
	void ComputeCitizenWorldPosition(const Building& building, float localX, float localY,
		float& outWorldX, float& outWorldY) {
		float relX = localX - building.GetBodySizeX() * 0.5f + building.GetBodyOffsetX();
		float relY = localY - building.GetBodySizeY() * 0.5f + building.GetBodyOffsetY();
		float rot = building.GetRotation();
		float c = FMath::Cos(rot), s = FMath::Sin(rot);
		outWorldX = (building.GetPosX() + relX * c - relY * s) * CITIZEN_WORLD_SCALE;
		outWorldY = (building.GetPosY() + relX * s + relY * c) * CITIZEN_WORLD_SCALE;
	}
}

ACitizenElement::ACitizenElement() {
	PrimaryActorTick.bCanEverTick = false;

	// 占位:UE默认小白人，和AForeverCharacter.cpp同款软路径——以后会替换成别的资产。mesh
	// 相对Z偏移=-capsule半高，让mesh视觉上的脚底正好落在capsule底部(和AForeverCharacter.
	// cpp的-96.f是同一个道理，这里用GetScaledCapsuleHalfHeight()动态取值而不是硬编码，
	// 不依赖某个特定的capsule尺寸配置)。
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> meshFinder(
		TEXT("/Game/Asset/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (meshFinder.Succeeded()) {
		GetMesh()->SetSkeletalMesh(meshFinder.Object);
		GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	}

	// 没有AnimInstance的SkeletalMeshComponent会一直显示bind pose(T-pose)——和
	// AForeverCharacter.cpp同款动画蓝图，至少有一个正常的待机姿势，不是张开手臂的T-pose。
	static ConstructorHelpers::FClassFinder<UAnimInstance> animFinder(
		TEXT("/Game/Asset/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
	if (animFinder.Succeeded()) {
		GetMesh()->SetAnimInstanceClass(animFinder.Class);
	}

	// 这次没有真正的AI移动逻辑——显式禁用移动模式，避免CharacterMovementComponent自己的
	// 重力/地面检测把citizen从Init()摆好的位置上挪走。以后加AI移动时改回MOVE_Walking即可，
	// 组件骨架已经搭好，不需要额外改动。
	GetCharacterMovement()->SetMovementMode(MOVE_None);
}

void ACitizenElement::Init(Citizen* inCitizen, UForeverPopulaceFrameworkComponent* inFramework) {
	citizen = inCitizen;
	framework = inFramework;
	if (!citizen) return;

	float worldX = 0.f, worldY = 0.f, worldZ = 0.f;
	if (citizen->HasPosition()) {
		// 反之直接在记录的位置出现——不再随机抖动一次。
		float mapX, mapY, mapZ;
		citizen->GetPosition(mapX, mapY, mapZ);
		worldX = mapX * CITIZEN_WORLD_SCALE;
		worldY = mapY * CITIZEN_WORLD_SCALE;
		worldZ = mapZ * CITIZEN_WORLD_SCALE;
	}
	else {
		// 换了新房间之后从未在场景里实例化过：房间中心+随机偏移(老工程原公式，抖动范围
		// ±0.2地图单位)，Z用Building::GetFloorBaseZ(room->GetLayer())——比老工程
		// "layer*楼层固定高度"的粗糙算法更准，这栋楼各层高度本来就不均匀。actor location
		// (capsule中心)要比楼板高一个capsule半高，脚底才会正好落在楼板上。算出来立刻写回
		// Citizen，这样"首次随机、此后复用"的记录在这一步就完成。
		Room* room = citizen->GetCurrentRoom(); // 物理位置，不是家(GetRoom())——两者这次
		// 初始状态重合，但语义上要用当前位置，见citizen.h/room.h的三概念说明
		Building* building = citizen->GetBuilding();
		if (room && building) {
			ComputeCitizenWorldPosition(*building, room->GetPosX(), room->GetPosY(), worldX, worldY);
			worldX += (GetRandom(11) / 10.f - 0.5f) * 0.4f * CITIZEN_WORLD_SCALE;
			worldY += (GetRandom(11) / 10.f - 0.5f) * 0.4f * CITIZEN_WORLD_SCALE;
			float floorZ = CITIZEN_HEIGHT_EPSILON + building->GetFloorBaseZ(room->GetLayer()) * CITIZEN_WORLD_SCALE
				+ CITIZEN_GROUND_SLAB_THICKNESS;
			worldZ = floorZ + GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		}
		citizen->SetPosition(worldX / CITIZEN_WORLD_SCALE, worldY / CITIZEN_WORLD_SCALE,
			worldZ / CITIZEN_WORLD_SCALE);
	}

	SetActorLocation(FVector(worldX, worldY, worldZ));

	BuildProximityBox();
}

void ACitizenElement::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	citizen = nullptr;
	Super::EndPlay(EndPlayReason);
}

void ACitizenElement::BuildProximityBox() {
	if (!citizen) return;

	UBoxComponent* box = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
	box->SetBoxExtent(FVector(CITIZEN_PROXIMITY_HALF_XY, CITIZEN_PROXIMITY_HALF_XY, CITIZEN_PROXIMITY_HALF_Z));
	box->SetCollisionProfileName(TEXT("Trigger"));
	box->SetupAttachment(RootComponent); // RootComponent就是ACharacter自带的CapsuleComponent
	box->OnComponentBeginOverlap.AddDynamic(this, &ACitizenElement::OnOverlapBegin);
	box->OnComponentEndOverlap.AddDynamic(this, &ACitizenElement::OnOverlapEnd);
	box->RegisterComponent();

	// 只烘焙一份显示用的姓名字符串，Overlap回调绝不解引用citizen——和ABuildingElement同一套
	// 安全原则。
	collisionLabel = FString::Printf(TEXT("Citizen: %s"), UTF8_TO_TCHAR(citizen->GetName().c_str()));
	proximityBox = box;
}

void ACitizenElement::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn || OtherActor != pawn || !GEngine) return;
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, FString::Printf(TEXT("接近 %s"), *collisionLabel));
}

void ACitizenElement::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex) {
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn || OtherActor != pawn || !GEngine) return;
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, FString::Printf(TEXT("离开 %s"), *collisionLabel));
}
