#include "Element/CitizenElement.h"

#include "Framework/ForeverPopulaceFrameworkComponent.h"
#include "Framework/ForeverFrameworkActor.h"
#include "Player/ForeverPlayerController.h"
#include "Player/ForeverCharacter.h"
#include "UI/MeetOptionWidget.h"

#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

#include "populace/citizen.h"
#include "populace/experience.h"
#include "populace/school.h"
#include "society/organization.h"
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
// Core侧map数据无关)：水平半宽~1.2m，垂直半高~0.9m——原先250/100(半宽~2.5m)明显过大，
// 玩家离市民还有段距离MeetOption就弹出来了，缩小到贴近"对话交互"的实际范围。
#define CITIZEN_PROXIMITY_HALF_XY 120.f
#define CITIZEN_PROXIMITY_HALF_Z 90.f

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
	// bCanEverTick=true+默认关闭Tick(SetActorTickEnabled(false))：绝大多数citizen静止
	// 不动，不需要每帧开销；只有WalkTo()正在带它走路的这段时间才打开Tick，走完立刻关掉，
	// 见WalkTo/Tick。
	PrimaryActorTick.bCanEverTick = true;
	SetActorTickEnabled(false);

	// mesh/anim/摄像机/移动参数/Enhanced Input绑定这些全部由基类AForeverCharacter的构造
	// 函数负责（同一份占位mesh，见ForeverCharacter.cpp），这里不用重复设置。这次没有真正的
	// AI移动逻辑——显式禁用移动模式，避免CharacterMovementComponent自己的重力/地面检测把
	// citizen从Init()摆好的位置上挪走，只有被玩家占有、或者被WalkTo带着走路时才切换成
	// MOVE_Walking，见PossessedBy/WalkTo。
	GetCharacterMovement()->SetMovementMode(MOVE_None);

	// 关键：这个项目没有AIController，WalkTo带着走路期间这个citizen的Pawn::Controller
	// 必然是nullptr(按定义——正被玩家占有的citizen不会触发调度自己上下班，见RequestWalk
	// 的possessed跳过分支)。UCharacterMovementComponent::TickComponent默认只有
	// Controller非空时才会调用PerformMovement()把AddMovementInput积累的输入转成真正的
	// Velocity——Controller为空时PerformMovement()整个不会跑，AddMovementInput因此是纯
	// 空操作：MovementMode显示MOVE_Walking、每帧都在调用AddMovementInput，但
	// GetVelocity()/GetActorLocation()纹丝不动。必须显式打开这个开关，让PerformMovement()
	// 在没有Controller时也照常跑。
	GetCharacterMovement()->bRunPhysicsWithNoController = true;
}

void ACitizenElement::PossessedBy(AController* NewController) {
	Super::PossessedBy(NewController); // AForeverCharacter::PossessedBy：增删Input Mapping Context
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	// 构造函数里默认关掉了Tick(见上，绝大多数citizen静止不需要每帧开销)，但被玩家占有
	// 之后，AForeverCharacter::Tick()要靠每帧跑才能做瞄准摄像机的平滑插值(cameraBoom
	// SocketOffset/TargetArmLength)——必须显式重新打开，否则右键瞄准的输入事件正常触发
	// (StartAim/StopAim能执行、bIsAiming能翻转)，但摄像机纹丝不动，因为Tick()整个没被
	// 引擎调用。见UnPossessed里对应的关闭。
	SetActorTickEnabled(true);

	// 被占有期间直接禁掉自己的proximityBox——这个box本来就是"检测玩家有没有靠近我"，
	// 自己正好就是玩家的时候，box和自己的capsule必然贴在一起常驻重叠，会不停产生自己的
	// Overlap事件。
	if (proximityBox) proximityBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACitizenElement::UnPossessed() {
	GetCharacterMovement()->SetMovementMode(MOVE_None);
	if (proximityBox) proximityBox->SetCollisionProfileName(TEXT("Trigger")); // 恢复Trigger预设(QueryOnly+各通道Overlap)

	// 玩家换到别的角色后，恢复"静止citizen默认不跑Tick"的省性能状态——但如果这个citizen
	// 正好还在WalkTo()带的路径中间(pendingWaypoints非空)，不能关，Tick()自己走完路径后
	// 会在Tick()内部自行SetActorTickEnabled(false)，这里关了反而会打断走路逻辑。
	if (pendingWaypoints.Num() == 0) {
		SetActorTickEnabled(false);
	}

	Super::UnPossessed(); // AForeverCharacter::UnPossessed：增删Input Mapping Context
}

namespace {
	// 到达路径点的判定阈值(UE单位)——只判水平距离，Z由CharacterMovement自己的地面
	// 检测/重力处理，不需要精确匹配路径点的高度。
	constexpr float kWaypointArrivalThresholdUU = 80.f;
}

void ACitizenElement::WalkTo(const TArray<FVector>& waypoints, Room* destination) {
	if (waypoints.Num() == 0) return;
	pendingWaypoints = waypoints;
	waypointIndex = 0;
	walkDestination = destination;
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	SetActorTickEnabled(true);
}

void ACitizenElement::Tick(float DeltaTime) {
	Super::Tick(DeltaTime); // AForeverCharacter::Tick：瞄准摄像机插值，被玩家占有期间必须持续跑

	if (waypointIndex >= pendingWaypoints.Num()) {
		// 被玩家占有期间不能自己把Tick关掉——PossessedBy打开Tick是为了让上面Super::Tick()的
		// 瞄准插值每帧都能跑，但player-driven的移动不会经过WalkTo()/pendingWaypoints，所以
		// 一旦没有待走的路径点就会立刻满足这个条件，如果不加IsPlayerControlled()这个例外，
		// 会在被占有后的下一帧就把刚打开的Tick又关掉(实测踩过的坑，见ForeverWeaponComponent.md
		// "瞄准"一节)。UnPossessed里会在还给AI之前把Tick关掉，不需要这里补。
		if (!IsPlayerControlled()) {
			SetActorTickEnabled(false);
		}
		return;
	}

	FVector target = pendingWaypoints[waypointIndex];
	FVector location = GetActorLocation();
	FVector toTarget = target - location;
	toTarget.Z = 0.f;

	if (toTarget.SizeSquared() <= kWaypointArrivalThresholdUU * kWaypointArrivalThresholdUU) {
		waypointIndex++;
		if (waypointIndex >= pendingWaypoints.Num()) {
			GetCharacterMovement()->SetMovementMode(MOVE_None);
			if (!IsPlayerControlled()) {
				SetActorTickEnabled(false);
			}
			Room* arrived = walkDestination;
			walkDestination = nullptr;
			pendingWaypoints.Reset();
			if (framework.IsValid() && citizen) {
				framework->NotifyArrived(citizen, arrived);
			}
		}
		return;
	}

	AddMovementInput(toTarget.GetSafeNormal(), 1.f);
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
		// 换了新房间之后从未在场景里实例化过：房间中心+随机偏移+楼层高度换算落脚点，见
		// ComputeRoomLandingSpot()。算出来立刻写回Citizen，这样"首次随机、此后复用"的
		// 记录在这一步就完成。
		ComputeRoomLandingSpot(citizen->GetCurrentRoom(), worldX, worldY, worldZ);
		citizen->SetPosition(worldX / CITIZEN_WORLD_SCALE, worldY / CITIZEN_WORLD_SCALE,
			worldZ / CITIZEN_WORLD_SCALE);
	}

	SetActorLocation(FVector(worldX, worldY, worldZ));

	// 身体网格按性别：男Manny、女Quinn。
	ApplyBodyMesh(citizen->GetGender() == GENDER_FEMALE);

	BuildProximityBox();
}

void ACitizenElement::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	citizen = nullptr;
	Super::EndPlay(EndPlayReason);
}

bool ACitizenElement::ComputeRoomLandingSpot(Room* room, float& outWorldX, float& outWorldY, float& outWorldZ) {
	// building必须从room->GetParentBuilding()反查，不能用citizen->GetBuilding()——后者是
	// "家"所在的building，进入society域之后room可能是别的building(比如工作单位)的room，
	// 见ForeverPopulaceFrameworkComponent.cpp里ComputeLogicalPosition同一处修复的说明。
	Building* building = room ? room->GetParentBuilding() : nullptr;
	if (!room || !building) return false;

	// 房间中心+随机偏移(老工程原公式，抖动范围±0.2地图单位)，Z用
	// Building::GetFloorBaseZ(room->GetLayer())——比老工程"layer*楼层固定高度"的粗糙
	// 算法更准，这栋楼各层高度本来就不均匀。actor location(capsule中心)要比楼板高一个
	// capsule半高，脚底才会正好落在楼板上。
	ComputeCitizenWorldPosition(*building, room->GetPosX(), room->GetPosY(), outWorldX, outWorldY);
	outWorldX += (GetRandom(11) / 10.f - 0.5f) * 0.4f * CITIZEN_WORLD_SCALE;
	outWorldY += (GetRandom(11) / 10.f - 0.5f) * 0.4f * CITIZEN_WORLD_SCALE;
	float floorZ = CITIZEN_HEIGHT_EPSILON + building->GetFloorBaseZ(room->GetLayer()) * CITIZEN_WORLD_SCALE
		+ CITIZEN_GROUND_SLAB_THICKNESS;
	outWorldZ = floorZ + GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	return true;
}

void ACitizenElement::TeleportToRoom(Room* destination) {
	if (!citizen || !destination) return;

	float worldX, worldY, worldZ;
	if (!ComputeRoomLandingSpot(destination, worldX, worldY, worldZ)) return;

	// 瞬移会打断正在走的路(否则走完会回调NotifyArrived把currentRoom改回原来的目标)。
	pendingWaypoints.Reset();
	waypointIndex = 0;
	walkDestination = nullptr;

	SetActorLocation(FVector(worldX, worldY, worldZ));
	citizen->SetCurrentRoom(destination);
	citizen->SetPosition(worldX / CITIZEN_WORLD_SCALE, worldY / CITIZEN_WORLD_SCALE, worldZ / CITIZEN_WORLD_SCALE);
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

	// 姓名和选项快照在这里烘焙好，Overlap回调绝不解引用citizen——和ABuildingElement同一套
	// 安全原则。这个时间点
	// game_start广播早已跑完（见ForeverStoryFrameworkComponent::BroadcastGameStart对每个
	// 市民Scheduler Script的广播），citizen->GetOptions()已经是稳定内容。
	citizenNameOnly = UTF8_TO_TCHAR(citizen->GetName().c_str());

	cachedOptions.Reset();
	for (const std::string& option : citizen->GetOptions()) {
		cachedOptions.Add(UTF8_TO_TCHAR(option.c_str()));
	}

	proximityBox = box;
}

void ACitizenElement::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	// OtherActor == this：这个citizen自己正好就是当前被占有的pawn，proximityBox和自己的
	// capsule天然重叠(半径250 vs capsule半径42，box完全包住capsule)，这次一定会触发一次
	// "自己进自己的box"——必须排除，否则会给自己弹出自己的对话选项。
	//
	// 还必须要求OtherActor是AForeverCharacter(人物)——不加这条，开车靠近市民也会触发(玩家
	// 当前pawn是AVehicleElement，依然满足OtherActor==pawn)，实测反馈"开车靠近另一辆车/靠近
	// 市民都弹出了不该出现的选项"。ACitizenElement本身也继承自AForeverCharacter，只要是
	// 玩家当前实际操控的那个人形pawn(可能是玩家自己的角色，也可能是被ChangeControlChange
	// 换过去的某个市民)，这条Cast都会成立，不影响正常的人物靠近市民场景。
	if (!pawn || OtherActor != pawn || OtherActor == this || !Cast<AForeverCharacter>(OtherActor)) return;

	// 把这个市民当前的对话选项(cachedOptions快照)推给MeetOption UI——不解引用citizen，
	// 见citizenNameOnly/cachedOptions字段声明处的安全原则说明。
	if (AForeverPlayerController* playerController = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0))) {
		if (UMeetOptionWidget* meetOption = playerController->GetMeetOptionWidget()) {
			AForeverFrameworkActor* frameworkActor = framework.IsValid() ? Cast<AForeverFrameworkActor>(framework->GetOwner()) : nullptr;
			for (int32 idx = 0; idx < cachedOptions.Num(); idx++) {
				meetOption->AddOption(frameworkActor, cachedOptions[idx], citizenNameOnly, idx);
			}
		}
	}
}

void ACitizenElement::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex) {
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	// 和OnOverlapBegin同一条过滤，见那边的注释。
	if (!pawn || OtherActor != pawn || OtherActor == this || !Cast<AForeverCharacter>(OtherActor)) return;

	if (AForeverPlayerController* playerController = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0))) {
		if (UMeetOptionWidget* meetOption = playerController->GetMeetOptionWidget()) {
			meetOption->RemoveName(citizenNameOnly);
		}
	}
}

void ACitizenElement::RefreshOptions() {
	if (!citizen) return;
	cachedOptions.Reset();
	for (const std::string& option : citizen->GetOptions()) {
		cachedOptions.Add(UTF8_TO_TCHAR(option.c_str()));
	}
	if (AForeverPlayerController* playerController = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0))) {
		if (UMeetOptionWidget* meetOption = playerController->GetMeetOptionWidget()) {
			AForeverFrameworkActor* frameworkActor = framework.IsValid() ? Cast<AForeverFrameworkActor>(framework->GetOwner()) : nullptr;
			meetOption->ReplaceOptions(frameworkActor, citizenNameOnly, cachedOptions);
		}
	}
}
