#include "Controller/CitizenController.h"

#include "Element/CitizenElement.h"
#include "Framework/ForeverPopulaceFrameworkComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

#include "populace/citizen.h"
#include "map/room.h"

namespace {
	// 到达路径点的判定阈值(UE单位)——只判水平距离，Z由CharacterMovement自己的地面检测/重力处理，
	// 不需要精确匹配路径点的高度。
	constexpr float kWaypointArrivalThresholdUU = 80.f;
}

ACitizenController::ACitizenController() {
	// 空闲时不跑Tick：绝大多数市民静止不动，PushCommand时才打开，指令全部做完自己关掉。
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void ACitizenController::OnPossess(APawn* inPawn) {
	Super::OnPossess(inPawn);
	citizenElement = Cast<ACitizenElement>(inPawn);
}

void ACitizenController::OnUnPossess() {
	// 玩家接管/市民销毁：正在走的路作废，还原速度等状态（此时pawn还没被摘走）。
	ClearCommands();
	citizenElement.Reset();
	Super::OnUnPossess();
}

void ACitizenController::PushCommand(const FCharacterCommand& command) {
	if (!citizenElement.IsValid()) return;
	commands.Push(command);
	SetActorTickEnabled(true);
}

void ACitizenController::ClearCommands() {
	while (!commands.IsEmpty()) {
		EndCommand(false);
	}
	SetActorTickEnabled(false);
}

Room* ACitizenController::GetCurrentDestination() const {
	const FCharacterCommand* command = commands.Current();
	return (command && bCommandStarted) ? command->room : nullptr;
}

bool ACitizenController::GetRemainingWaypoints(TArray<FVector>& outWaypoints, Room*& outDestination) const {
	const FCharacterCommand* command = commands.Current();
	ACitizenElement* element = citizenElement.Get();
	if (!command || !element || command->type != FCharacterCommand::EType::FollowPath || !command->room) return false;
	outWaypoints.Reset();
	// 路径点是脚底高度（行人图节点在地面上），Actor位置是胶囊体中心，要减掉半高，否则折线/交还Core的路径会整体抬高。
	outWaypoints.Add(element->GetActorLocation() - FVector(0.f, 0.f, element->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	for (int32 i = bCommandStarted ? waypointIndex : 0; i < command->waypoints.Num(); i++) {
		outWaypoints.Add(command->waypoints[i]);
	}
	outDestination = command->room;
	return true;
}

void ACitizenController::BeginCommand(FCharacterCommand& command) {
	ACitizenElement* element = citizenElement.Get();
	if (!element) return;

	if (command.type == FCharacterCommand::EType::GoToRoom) {
		TArray<FVector> planned;
		UForeverPopulaceFrameworkComponent* framework = element->GetFramework();
		if (!framework || !framework->PlanWalkPath(element->GetCitizen(), command.room, element->GetActorLocation(), planned)) {
			EndCommand(false);
			return;
		}
		command.type = FCharacterCommand::EType::FollowPath;
		command.waypoints = MoveTemp(planned);
	}

	if (command.waypoints.Num() == 0) {
		EndCommand(false);
		return;
	}

	bCommandStarted = true;
	waypointIndex = 0;
	wanderPhase = FMath::FRand() * UE_TWO_PI; // 每条路线重抽，不能放构造函数（实例会拷贝CDO的值，所有人相位相同）

	UCharacterMovementComponent* movement = element->GetCharacterMovement();
	savedMaxWalkSpeed = movement->MaxWalkSpeed;
	movement->MaxWalkSpeed = savedMaxWalkSpeed * command.speedScale;
	movement->SetMovementMode(MOVE_Walking);
	if (command.bExemptDespawn) element->SetDespawnExempt(true);
}

void ACitizenController::EndCommand(bool arrived) {
	FCharacterCommand* current = commands.Current();
	if (!current) return;
	FCharacterCommand finished = *current;

	ACitizenElement* element = citizenElement.Get();
	if (bCommandStarted && element) {
		UCharacterMovementComponent* movement = element->GetCharacterMovement();
		if (savedMaxWalkSpeed > 0.f) movement->MaxWalkSpeed = savedMaxWalkSpeed;
		if (finished.bExemptDespawn) element->SetDespawnExempt(false);
		// 玩家已经接管时不能把移动模式切回MOVE_None（PossessedBy刚把它设成Walking）。
		if (element->GetController() == this) movement->SetMovementMode(MOVE_None);
	}
	savedMaxWalkSpeed = 0.f;
	bCommandStarted = false;
	waypointIndex = 0;

	commands.Advance();
	if (commands.IsEmpty()) SetActorTickEnabled(false);

	UForeverPopulaceFrameworkComponent* framework = element ? element->GetFramework() : nullptr;
	Citizen* citizen = element ? element->GetCitizen() : nullptr;
	if (!framework || !citizen) return;
	if (arrived && finished.room) {
		framework->NotifyArrived(citizen, finished.room);
	}
}

void ACitizenController::Tick(float deltaTime) {
	Super::Tick(deltaTime);

	ACitizenElement* element = citizenElement.Get();
	FCharacterCommand* command = commands.Current();
	if (!element || !command) {
		SetActorTickEnabled(false);
		return;
	}

	if (!bCommandStarted) {
		BeginCommand(*command);
		command = commands.Current();
		if (!bCommandStarted || !command) return;
	}

	if (waypointIndex >= command->waypoints.Num()) {
		EndCommand(true);
		return;
	}

	FVector toTarget = command->waypoints[waypointIndex] - element->GetActorLocation();
	toTarget.Z = 0.f;
	if (toTarget.SizeSquared() <= kWaypointArrivalThresholdUU * kWaypointArrivalThresholdUU) {
		waypointIndex++;
		if (waypointIndex >= command->waypoints.Num()) EndCommand(true);
		return;
	}

	FVector direction = toTarget.GetSafeNormal();
	if (wanderAmplitude > 0.f) {
		const float fade = FMath::Clamp(toTarget.Size() / FMath::Max(wanderFadeDistance, 1.f), 0.f, 1.f);
		const float sway = FMath::Sin(GetWorld()->GetTimeSeconds() * wanderFrequency + wanderPhase) * wanderAmplitude * fade;
		direction = (direction + FVector(-direction.Y, direction.X, 0.f) * sway).GetSafeNormal();
	}
	element->AddMovementInput(direction, 1.f);
}
