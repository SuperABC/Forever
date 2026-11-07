#include "Element/CitizenElement.h"

#include "Framework/ForeverPopulaceFrameworkComponent.h"
#include "Framework/ForeverFrameworkActor.h"
#include "Player/ForeverPlayerController.h"
#include "Player/ForeverCharacter.h"
#include "Player/ForeverAvatarSubsystem.h"
#include "Engine/GameInstance.h"
#include "UI/MeetOptionWidget.h"

#include "Controller/CitizenController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

#include "populace/citizen.h"
#include "populace/scheduler.h"
#include "story/script.h"
#include "populace/populace.h"
#include "player/player.h"
#include "populace/experience.h"
#include "populace/school.h"
#include "society/organization.h"
#include "map/building.h"
#include "map/room.h"
#include "common/utility.h"

// 地图单位 -> UE单位，和BuildingElement.cpp的BUILDING_WORLD_SCALE同值(各文件各自维护一份)。
#define CITIZEN_WORLD_SCALE 1000.f
// 楼层Z起算的"地坪"偏移(UE单位)，和BUILDING_HEIGHT_EPSILON同一个值/同一个理由。
#define CITIZEN_HEIGHT_EPSILON 10.f
// 地板slab厚度(UE单位)，对应BuildingElement.cpp里kSlabThickness(0.02地图单位)。Ground slab中心在楼层底部+半个厚度，
// 站人的表面(slab顶面)比floorBaseZ换算出的楼层底部高一个完整厚度，落脚点必须按顶面算，否则会陷进地板。
#define CITIZEN_GROUND_SLAB_THICKNESS 20.f
// 靠近检测碰撞盒尺寸(UE单位，纯UE层判定，与Core数据无关)：水平半宽~1.2m，垂直半高~0.9m，贴近"对话交互"的实际范围。
#define CITIZEN_PROXIMITY_HALF_XY 120.f
#define CITIZEN_PROXIMITY_HALF_Z 90.f

using namespace std;

namespace {
	// 把Floor局部坐标(原点在楼体左下角，未旋转)转成世界坐标，公式同BuildingElement.cpp的ComputeWorldPosition。
	// 函数名在整个Forever模块内必须唯一：UBT的unity build会把多个.cpp拼进同一个翻译单元，
	// 匿名namespace里的同名同签名函数会报重定义(实测踩过)。
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
	// 默认关闭Tick：绝大多数citizen静止不动；只有被玩家占有时才打开(见PossessedBy)。
	PrimaryActorTick.bCanEverTick = true;
	SetActorTickEnabled(false);

	// mesh/anim/摄像机/移动/输入全部由基类AForeverCharacter负责。默认禁用移动模式，避免重力/地面检测把Init()摆好的位置挪走；
	// 被玩家占有时切MOVE_Walking，被自己的ACitizenController带着走路时由Controller切换。
	GetCharacterMovement()->SetMovementMode(MOVE_None);

	// 玩家占有/Controller被摘走的短暂空窗期里，已有的物理移动不应被卡住。
	GetCharacterMovement()->bRunPhysicsWithNoController = true;
}

void ACitizenElement::PossessedBy(AController* NewController) {
	Super::PossessedBy(NewController); // AForeverCharacter::PossessedBy：增删Input Mapping Context

	// 自己的AIController占有：保持MOVE_None和关Tick，Controller开始执行指令时才切换（见CitizenController）。
	if (!Cast<APlayerController>(NewController)) return;

	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		// 被玩家占有后，AForeverCharacter::Tick()要每帧跑才能做瞄准摄像机的平滑插值，必须重新打开(UnPossessed里关回)。
	SetActorTickEnabled(true);

	// 被占有期间禁掉自己的proximityBox：自己就是玩家时box和自己的capsule必然常驻重叠，会不停产生自己的Overlap事件。
	if (proximityBox) proximityBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACitizenElement::UnPossessed() {
	// 先判断是谁在放手：玩家放手 → 恢复静止状态并让自己的AI Controller重新接管；
	// 自己的AIController被玩家顶掉 → 只是换手，PossessedBy(玩家)马上会把状态设好。
	const bool bPlayerLetGo = Cast<APlayerController>(GetController()) != nullptr;

	GetCharacterMovement()->SetMovementMode(MOVE_None);
	if (proximityBox) proximityBox->SetCollisionProfileName(TEXT("Trigger")); // 恢复Trigger预设(QueryOnly+各通道Overlap)
	SetActorTickEnabled(false); // 玩家放手后恢复"静止citizen默认不跑Tick"的省性能状态

	Super::UnPossessed(); // AForeverCharacter::UnPossessed：增删Input Mapping Context

	if (bPlayerLetGo) RepossessAIController();
}

void ACitizenElement::RepossessAIController() {
	if (!aiController) return;
	// 不能在UnPossessed调用栈里直接Possess：此时APawn::Controller还没清空，下一帧再接管。
	TWeakObjectPtr<ACitizenElement> weakThis(this);
	if (UWorld* world = GetWorld()) {
		world->GetTimerManager().SetTimerForNextTick([weakThis]() {
			ACitizenElement* self = weakThis.Get();
			if (!self || !self->aiController || self->GetController()) return;
			self->aiController->Possess(self);
		});
	}
}

void ACitizenElement::WalkTo(const TArray<FVector>& waypoints, Room* destination) {
	if (waypoints.Num() == 0 || !aiController) return;
	aiController->ClearCommands(); // 新路线取代还没走完的旧路线，不排队
	aiController->PushCommand(FCharacterCommand::MakeFollowPath(waypoints, destination));
}

Room* ACitizenElement::GetWalkDestination() const {
	return aiController ? aiController->GetCurrentDestination() : nullptr;
}

void ACitizenElement::Tick(float DeltaTime) {
	Super::Tick(DeltaTime); // AForeverCharacter::Tick：瞄准摄像机插值，被玩家占有期间必须持续跑

	// 走路已经搬到ACitizenController里；这个Actor的Tick只有被玩家占有时才需要（瞄准插值），
	// 其余时候立刻关掉。
	if (!IsPlayerControlled()) SetActorTickEnabled(false);
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

	// 头像：按这个市民的头像文件贴到头像方块上；没有头像文件时保留默认头像。
	if (UGameInstance* gameInstance = GetGameInstance()) {
		if (UForeverAvatarSubsystem* avatars = gameInstance->GetSubsystem<UForeverAvatarSubsystem>()) {
			// 按当前游戏年份算年龄，挑年龄相符的头像；全局时钟还没建时退回人口模拟的"当前年份"。
			int32 currentYear = 0;
			if (AForeverFrameworkActor* frameworkActor = Cast<AForeverFrameworkActor>(inFramework ? inFramework->GetOwner() : nullptr)) {
				if (Player* player = frameworkActor->GetPlayer(); player && player->GetTime()) currentYear = player->GetTime()->GetYear();
				else if (Populace* populace = frameworkActor->GetPopulace()) currentYear = populace->GetCurrentYear();
			}
			SetAvatarTexture(avatars->GetCitizenAvatar(citizen, currentYear));
		}
	}

	BuildProximityBox();

	// 自己的AI Controller：只在市民被spawn期间存在，EndPlay里销毁。
	if (UWorld* world = GetWorld(); world && !aiController) {
		FActorSpawnParameters params;
		params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		aiController = world->SpawnActor<ACitizenController>(ACitizenController::StaticClass(), GetActorLocation(), FRotator::ZeroRotator, params);
		if (aiController) aiController->Possess(this);
	}
}

void ACitizenElement::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	// Controller随Actor一起销毁，正在执行的指令/路径一并消失。
	if (aiController) {
		aiController->ClearCommands();
		if (GetController() == aiController) aiController->UnPossess();
		aiController->Destroy();
		aiController = nullptr;
	}
	// 不管流式销毁还是被引擎销毁，都要把自己从framework的activeInstances摘掉(TMap的Citizen*键不会被GC置空，悬空指针会让下一次轮询崩溃)。
	// 只处理真正的Destroy()：退出游戏/切关卡/PIE结束时Core的Populace、脚本可能已先释放，citizen指针已悬空，碰它们会崩。
	if (citizen && EndPlayReason == EEndPlayReason::Destroyed) {
		if (UForeverPopulaceFrameworkComponent* owner = framework.Get()) owner->HandleElementEndPlay(citizen, this);
	}
	citizen = nullptr;
	Super::EndPlay(EndPlayReason);
}

bool ACitizenElement::ComputeRoomLandingSpot(Room* room, float& outWorldX, float& outWorldY, float& outWorldZ) {
	// building必须从room->GetParentBuilding()反查，不能用citizen->GetBuilding()(那是"家"，换房间后不一定同一栋楼)。
	Building* building = room ? room->GetParentBuilding() : nullptr;
	if (!room || !building) return false;

	// 房间中心+随机偏移(抖动±0.2地图单位)，Z用Building::GetFloorBaseZ(room->GetLayer())；
	// actor location是capsule中心，要比楼板高一个capsule半高，脚底才落在楼板上。
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
	if (aiController) aiController->ClearCommands();

	SetActorLocation(FVector(worldX, worldY, worldZ));
	citizen->SetCurrentRoom(destination);
	citizen->SetPosition(worldX / CITIZEN_WORLD_SCALE, worldY / CITIZEN_WORLD_SCALE, worldZ / CITIZEN_WORLD_SCALE);
}

void ACitizenElement::BuildProximityBox() {
	if (!citizen || proximityBox) return;
	// 没有Scheduler脚本、也没有任何选项就不挂触发盒；之后选项出现时RefreshOptions会补建。
	if ((!citizen->GetScheduler() || !citizen->GetScheduler()->GetScript()) && citizen->GetOptions().empty()) return;

	UBoxComponent* box = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
	box->SetBoxExtent(FVector(CITIZEN_PROXIMITY_HALF_XY, CITIZEN_PROXIMITY_HALF_XY, CITIZEN_PROXIMITY_HALF_Z));
	box->SetCollisionProfileName(TEXT("Trigger"));
	box->SetupAttachment(RootComponent); // RootComponent就是ACharacter自带的CapsuleComponent
	box->OnComponentBeginOverlap.AddDynamic(this, &ACitizenElement::OnOverlapBegin);
	box->OnComponentEndOverlap.AddDynamic(this, &ACitizenElement::OnOverlapEnd);
	box->RegisterComponent();

	// 姓名和选项快照在这里烘焙，Overlap回调绝不解引用citizen；此时game_start广播已跑完，选项已稳定。
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
	// 必须排除OtherActor==this(自己被占有时box与自己的capsule天然重叠)，并要求OtherActor是AForeverCharacter(人物)：
	// 否则开车靠近市民也会弹选项。
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
	// 同OnOverlapBegin的过滤。
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
	if (!proximityBox) {
		BuildProximityBox(); // 之前没有脚本/选项没建盒，现在有选项了
		return;
	}
	if (AForeverPlayerController* playerController = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0))) {
		if (UMeetOptionWidget* meetOption = playerController->GetMeetOptionWidget()) {
			AForeverFrameworkActor* frameworkActor = framework.IsValid() ? Cast<AForeverFrameworkActor>(framework->GetOwner()) : nullptr;
			meetOption->ReplaceOptions(frameworkActor, citizenNameOnly, cachedOptions);
		}
	}
}
