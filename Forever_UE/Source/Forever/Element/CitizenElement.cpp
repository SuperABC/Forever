#include "Element/CitizenElement.h"

#include "Framework/ForeverPopulaceFrameworkComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
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

// 附近市民名单：静态成员定义。
TArray<TWeakObjectPtr<ACitizenElement>> ACitizenElement::nearbyCitizens;

ACitizenElement::ACitizenElement() {
	PrimaryActorTick.bCanEverTick = false;

	// mesh/anim/摄像机/移动参数/Enhanced Input绑定这些全部由基类AForeverCharacter的构造
	// 函数负责（同一份占位mesh，见ForeverCharacter.cpp），这里不用重复设置。这次没有真正的
	// AI移动逻辑——显式禁用移动模式，避免CharacterMovementComponent自己的重力/地面检测把
	// citizen从Init()摆好的位置上挪走，只有被玩家占有时才切换成MOVE_Walking，见PossessedBy。
	GetCharacterMovement()->SetMovementMode(MOVE_None);
}

void ACitizenElement::PossessedBy(AController* NewController) {
	Super::PossessedBy(NewController); // AForeverCharacter::PossessedBy：增删Input Mapping Context
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	// 被占有期间直接禁掉自己的proximityBox——这个box本来就是"检测玩家有没有靠近我"，
	// 自己正好就是玩家的时候，box和自己的capsule必然贴在一起常驻重叠，会不停产生自己的
	// Overlap事件。但disable只能防止"以后"的自我Overlap——如果在被占有之前，自己就已经
	// 作为"别人身边的市民"被加进过nearbyCitizens(比如市民A靠近市民B时，B的proximityBox把
	// B自己加进了名单；A随后按T切换到B，B变成新的pawn，但名单里"B"这一条早就在disable
	// 生效之前就已经存在了，disable不会回头清掉它)，这一条陈旧的"自己"记录就会一直卡在
	// 名单里，永远排在最前面，导致下一次按T又切回"自己"、名单卡死——这正是实测复现的
	// "切到第二个市民后再也切不出去"的bug。所以这里必须显式地把自己从名单里摘出去，
	// disable只是防止之后再把自己加回来。
	if (proximityBox) proximityBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	nearbyCitizens.RemoveSingle(TWeakObjectPtr<ACitizenElement>(this));
}

void ACitizenElement::UnPossessed() {
	GetCharacterMovement()->SetMovementMode(MOVE_None);
	if (proximityBox) proximityBox->SetCollisionProfileName(TEXT("Trigger")); // 恢复Trigger预设(QueryOnly+各通道Overlap)
	Super::UnPossessed(); // AForeverCharacter::UnPossessed：增删Input Mapping Context
}

ACitizenElement* ACitizenElement::GetFirstNearby() {
	while (nearbyCitizens.Num() > 0) {
		if (ACitizenElement* citizen = nearbyCitizens[0].Get()) {
			return citizen;
		}
		nearbyCitizens.RemoveAt(0); // 清理已失效的弱引用
	}
	return nullptr;
}

void ACitizenElement::DebugPrintNearby() {
	if (!GEngine) return;

	if (nearbyCitizens.Num() == 0) {
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, TEXT("[T] 附近市民名单为空"));
		return;
	}

	FString combined;
	for (int32 i = 0; i < nearbyCitizens.Num(); i++) {
		ACitizenElement* nearby = nearbyCitizens[i].Get();
		combined += FString::Printf(TEXT("[%d]%s"), i, nearby ? *nearby->collisionLabel : TEXT("(已失效)"));
		if (i + 1 < nearbyCitizens.Num()) combined += TEXT(" | ");
	}
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, FString::Printf(TEXT("[T] 附近市民名单: %s"), *combined));
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
	// OtherActor == this：这个citizen自己正好就是当前被占有的pawn，proximityBox和自己的
	// capsule天然重叠(半径250 vs capsule半径42，box完全包住capsule)，这次一定会触发一次
	// "自己进自己的box"——必须排除，否则自己会被塞进nearbyCitizens[0]常驻不走，
	// GetFirstNearby()只看下标0，会一直卡在"自己"上，导致T键切换到其他citizen失效。
	if (!pawn || OtherActor != pawn || OtherActor == this) return;

	nearbyCitizens.AddUnique(TWeakObjectPtr<ACitizenElement>(this));

	if (!GEngine) return;
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, FString::Printf(TEXT("接近 %s"), *collisionLabel));
}

void ACitizenElement::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex) {
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn || OtherActor != pawn || OtherActor == this) return;

	nearbyCitizens.RemoveSingle(TWeakObjectPtr<ACitizenElement>(this));

	if (!GEngine) return;
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, FString::Printf(TEXT("离开 %s"), *collisionLabel));
}
