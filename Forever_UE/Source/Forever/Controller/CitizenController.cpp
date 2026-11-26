#include "Controller/CitizenController.h"

#include "Framework/CoreGuard.h"

#include "Element/CitizenElement.h"
#include "Framework/ForeverPopulaceFrameworkComponent.h"
#include "Framework/ForeverTrafficFrameworkComponent.h"
#include "traffic/junction.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

#include "populace/citizen.h"
#include "map/room.h"

namespace {
	// 到达路径点的判定阈值(UE单位)——只判水平距离，Z由CharacterMovement自己的地面检测/重力处理，
	// 不需要精确匹配路径点的高度。
	constexpr float kWaypointArrivalThresholdUU = 80.f;
	// 地图单位 -> UE单位，同ForeverPopulaceFrameworkComponent.cpp的POPULACE_WORLD_SCALE。
	constexpr float kCitizenWorldScale = 1000.f;
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
	ReleaseCrosswalk();
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

bool ACitizenController::RetargetFlee(Citizen* target) {
	FCharacterCommand* command = commands.Current();
	if (!command || command->type != FCharacterCommand::EType::Flee) return false;
	command->fleeTarget = target;
	return true;
}

bool ACitizenController::GetCurrentGreenDestination(Green*& outGreen, int32& outCircle) const {
	const FCharacterCommand* command = commands.Current();
	if (!command || !bCommandStarted || !command->green) return false;
	outGreen = command->green;
	outCircle = command->greenCircle;
	return true;
}

bool ACitizenController::GetRemainingWaypoints(TArray<FVector>& outWaypoints, Room*& outDestination, Green** outGreen, int32* outGreenCircle) const {
	const FCharacterCommand* command = commands.Current();
	ACitizenElement* element = citizenElement.Get();
	if (!command || !element || command->type != FCharacterCommand::EType::FollowPath || (!command->room && !command->green)) return false;
	outWaypoints.Reset();
	// 路径点是脚底高度（行人图节点在地面上），Actor位置是胶囊体中心，要减掉半高，否则折线/交还Core的路径会整体抬高。
	outWaypoints.Add(element->GetActorLocation() - FVector(0.f, 0.f, element->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	for (int32 i = bCommandStarted ? waypointIndex : 0; i < command->waypoints.Num(); i++) {
		outWaypoints.Add(command->waypoints[i]);
	}
	outDestination = command->room;
	if (outGreen) *outGreen = command->green;
	if (outGreenCircle) *outGreenCircle = command->greenCircle;
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

	if (command.type == FCharacterCommand::EType::Flee) {
		// 起点：离市民最近的行人导航线，在两个端点里选更背离目标的；再提前选好下一个节点。
		UForeverPopulaceFrameworkComponent* framework = element->GetFramework();
		ACitizenElement* targetElement = (framework && command.fleeTarget) ? framework->FindActiveCitizen(command.fleeTarget) : nullptr;
		FVector start;
		int32 node = -1, prev = -1;
		const FVector feet = element->GetActorLocation();
		if (!targetElement || !command.fleeTarget->IsActive()) {
			fleeEndReason = "lost";
			EndCommand(false);
			return;
		}
		fleeTargetLocation = targetElement->GetActorLocation();
		command.waypoints.Reset();
		if (!framework->PlanFleeStart(feet, fleeTargetLocation, start, node, prev)) {
			fleeEndReason = "far";
			EndCommand(false);
			return;
		}
		command.waypoints.Add(start);
		fleeTailNode = node;
		fleePrevNode = prev;
		fleeCheckTimer = 0.25f;
		ExtendFlee(command);
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
	ReleaseCrosswalk();
	FCharacterCommand finished = *current;
	const char* fleeReason = fleeEndReason; // 取走：EndCommand里面可能再被脚本事件重入
	fleeEndReason = nullptr;

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
	if (finished.type == FCharacterCommand::EType::Flee) {
		// 逃跑结束：自己判定的原因优先；路径选不下去(arrived)算far，被清空/取代(其余)算interrupted。
		framework->NotifyFleeEnded(citizen, fleeReason ? fleeReason : (arrived ? "far" : "interrupted"));
		return;
	}
	if (arrived && finished.room) {
		framework->NotifyArrived(citizen, finished.room);
	}
	else if (arrived && finished.green) {
		framework->NotifyArrivedGreen(citizen, finished.green, finished.greenCircle);
	}
}

bool ACitizenController::ExtendFlee(FCharacterCommand& command) {
	ACitizenElement* element = citizenElement.Get();
	UForeverPopulaceFrameworkComponent* framework = element ? element->GetFramework() : nullptr;
	if (!framework) return false;
	FVector point;
	int32 node = -1, prev = -1;
	if (!framework->PlanFleeNext(fleeTailNode, fleePrevNode, fleeTargetLocation, point, node, prev)) return false;
	command.waypoints.Add(point);
	fleeTailNode = node;
	fleePrevNode = prev;
	return true;
}

void ACitizenController::ExtendFleeAhead(FCharacterCommand& command) {
	while (command.waypoints.Num() < waypointIndex + 2) {
		if (!ExtendFlee(command)) break;
	}
}

bool ACitizenController::TickFlee(ACitizenElement* element, FCharacterCommand& command, float deltaTime) {
	fleeTurnCooldown -= deltaTime;
	fleeCheckTimer -= deltaTime;
	if (fleeCheckTimer > 0.f) return false;
	fleeCheckTimer = 0.25f;

	UForeverPopulaceFrameworkComponent* framework = element->GetFramework();
	ACitizenElement* targetElement = (framework && command.fleeTarget) ? framework->FindActiveCitizen(command.fleeTarget) : nullptr;
	if (!targetElement || !command.fleeTarget->IsActive()) {
		fleeEndReason = "lost"; // 目标被销毁(流式销毁/死亡/离城)
		EndCommand(false);
		return true;
	}
	fleeTargetLocation = targetElement->GetActorLocation();
	if (framework && framework->IsFleeRangeExceeded(element->GetActorLocation())) {
		fleeEndReason = "far"; // 跑出实例化距离阈值
		EndCommand(false);
		return true;
	}

	// 检查跑的方向：当前前进方向(指向下一个路点)和"市民→目标"连线的夹角小于90度，说明已经在朝目标跑了(目标绕过来了/
	// 路线拐向了目标)，立刻掉头：以现在的位置重新选最近的行人线、取背离目标的端点，路径点整个换掉。
	// 正在过横道时不掉头(横道占用要先放掉再转，等过完)。
	if (fleeTurnCooldown <= 0.f && !crossing && command.waypoints.IsValidIndex(waypointIndex)) {
		const FVector here = element->GetActorLocation();
		FVector heading = command.waypoints[waypointIndex] - here;
		FVector toTarget = fleeTargetLocation - here;
		heading.Z = 0.f;
		toTarget.Z = 0.f;
		if (heading.Normalize() && toTarget.Normalize() && FVector::DotProduct(heading, toTarget) > 0.f) {
			FVector start;
			int32 node = -1, prev = -1;
			if (framework && framework->PlanFleeStart(here, fleeTargetLocation, start, node, prev)) {
				ReleaseCrosswalk();
				command.waypoints.Reset();
				command.waypoints.Add(start);
				fleeTailNode = node;
				fleePrevNode = prev;
				waypointIndex = 0;
				ExtendFlee(command);
				fleeTurnCooldown = 0.75f;
			}
		}
	}
	return false;
}

void ACitizenController::ReleaseCrosswalk() {
	if (crossing || pendingCrossing) {
		ACitizenElement* element = citizenElement.Get();
		UForeverPopulaceFrameworkComponent* populace = element ? element->GetFramework() : nullptr;
		AActor* owner = populace ? populace->GetOwner() : nullptr;
		UForeverTrafficFrameworkComponent* traffic = owner ? owner->FindComponentByClass<UForeverTrafficFrameworkComponent>() : nullptr;
		JunctionTraffic* junctionTraffic = traffic ? traffic->GetJunctionTraffic() : nullptr;
		if (junctionTraffic) {
			const int64_t entity = JunctionTraffic::MakeEntity(JunctionTraffic::EntityKind::Pedestrian, GetUniqueID());
			if (crossing) junctionTraffic->Leave(crossing, entity);
			if (pendingCrossing) junctionTraffic->Leave(pendingCrossing, entity);
		}
	}
	crossing = nullptr;
	pendingCrossing = nullptr;
	crossingEndIndex = -1;
	pendingCheckedIndex = -1;
}

bool ACitizenController::GateCrosswalk(ACitizenElement* element, const FCharacterCommand& command, const FVector& toTarget) {
	UForeverPopulaceFrameworkComponent* populace = element->GetFramework();
	AActor* owner = populace ? populace->GetOwner() : nullptr;
	UForeverTrafficFrameworkComponent* traffic = owner ? owner->FindComponentByClass<UForeverTrafficFrameworkComponent>() : nullptr;
	JunctionTraffic* junctionTraffic = traffic ? traffic->GetJunctionTraffic() : nullptr;
	if (!junctionTraffic) return false;

	const double now = traffic->GetTrafficClock();
	const int64_t entity = JunctionTraffic::MakeEntity(JunctionTraffic::EntityKind::Pedestrian, GetUniqueID());

	// 正在过横道：续租；走过横道终点就放行。
	if (crossing) {
		if (waypointIndex > crossingEndIndex) {
			junctionTraffic->Leave(crossing, entity);
			crossing = nullptr;
			crossingEndIndex = -1;
		}
		else {
			if (now >= nextCrossingRequest) {
				nextCrossingRequest = now + crossingRequestInterval;
				junctionTraffic->Request(crossing, entity, now, 0.f, 1e9f);
			}
			return false;
		}
	}

	// 下一段(当前目标点 -> 再下一个点)是横道吗：每个目标点只匹配一次。
	if (pendingCheckedIndex != waypointIndex) {
		if (pendingCrossing) junctionTraffic->Leave(pendingCrossing, entity);
		pendingCrossing = nullptr;
		pendingCheckedIndex = waypointIndex;
		if (waypointIndex + 1 < command.waypoints.Num()) {
			const FVector& a = command.waypoints[waypointIndex];
			const FVector& b = command.waypoints[waypointIndex + 1];
			pendingCrossing = junctionTraffic->FindCrosswalk(a.X / kCitizenWorldScale, a.Y / kCitizenWorldScale, b.X / kCitizenWorldScale, b.Y / kCitizenWorldScale);
		}
	}
	if (!pendingCrossing) return false;

	const float distance = toTarget.Size();
	if (distance > crossingRequestDistance) return false;
	if (now >= nextCrossingRequest) {
		nextCrossingRequest = now + crossingRequestInterval;
		if (junctionTraffic->Request(pendingCrossing, entity, now, 0.f, 1e9f)) {
			crossing = pendingCrossing;
			crossingEndIndex = waypointIndex + 1;
			pendingCrossing = nullptr;
			return false;
		}
	}
	return distance <= crossingWaitDistance; // 没放行：走到路沿就停，别再往前
}

void ACitizenController::Tick(float deltaTime) {
	Super::Tick(deltaTime);
	CORE_GUARD_BEGIN("ACitizenController::Tick")

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

	if (command->type == FCharacterCommand::EType::Flee && TickFlee(element, *command, deltaTime)) return;

	if (waypointIndex >= command->waypoints.Num()) {
		EndCommand(true);
		return;
	}

	FVector toTarget = command->waypoints[waypointIndex] - element->GetActorLocation();
	toTarget.Z = 0.f;
	if (toTarget.SizeSquared() <= kWaypointArrivalThresholdUU * kWaypointArrivalThresholdUU) {
		waypointIndex++;
		if (command->type == FCharacterCommand::EType::Flee) ExtendFleeAhead(*command);
		if (waypointIndex >= command->waypoints.Num()) EndCommand(true);
		return;
	}

	if (GateCrosswalk(element, *command, toTarget)) return; // 路口没放行：原地等

	FVector direction = toTarget.GetSafeNormal();
	if (wanderAmplitude > 0.f) {
		const float fade = FMath::Clamp(toTarget.Size() / FMath::Max(wanderFadeDistance, 1.f), 0.f, 1.f);
		const float sway = FMath::Sin(GetWorld()->GetTimeSeconds() * wanderFrequency + wanderPhase) * wanderAmplitude * fade;
		direction = (direction + FVector(-direction.Y, direction.X, 0.f) * sway).GetSafeNormal();
	}
	element->AddMovementInput(direction, 1.f);
	CORE_GUARD_END
}
