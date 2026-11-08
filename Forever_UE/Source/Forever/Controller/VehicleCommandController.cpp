#include "Controller/VehicleCommandController.h"

#include "Element/VehicleElement.h"
#include "Framework/ForeverTrafficFrameworkComponent.h"

#include "map/map.h"
#include "map/room.h"
#include "traffic/vehicle.h"
#include "traffic/junction.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"

namespace {
	// 地图单位 -> UE单位，同ForeverTrafficFrameworkComponent.cpp的TRAFFIC_WORLD_SCALE。
	constexpr float kVehicleControllerWorldScale = 1000.f;
	constexpr float kDefaultMaxSteerDegrees = 40.f;
	// 停车时的速度门限：高于它用刹车踩停，低于它松刹车拉手刹（Chaos的"倒车当刹车"在低速踩刹车会挂倒挡）。
	constexpr float kHandbrakeSpeed = 80.f;
	constexpr float kStuckSeconds = 5.f;
	constexpr float kStuckRemainDistance = 800.f;
	constexpr float kDropOnResumeUU = 20.f; // 运动学切回物理时往上抬一点，让它自己落到地面
}

// ---------------------------------------------------------------------------------------------
// FPath
// ---------------------------------------------------------------------------------------------

FVector AVehicleCommandController::FPath::Sample(float s, FVector* outTangent) const
{
	if (points.Num() < 2) {
		if (outTangent) *outTangent = FVector::ForwardVector;
		return points.Num() == 1 ? points[0] : FVector::ZeroVector;
	}
	s = FMath::Clamp(s, 0.f, total);
	// arc单调递增，二分找第一个>=s的下标。
	int32 lo = 0, hi = arc.Num() - 1;
	while (lo < hi) {
		int32 mid = (lo + hi) / 2;
		if (arc[mid] < s) lo = mid + 1; else hi = mid;
	}
	int32 b = FMath::Clamp(lo, 1, points.Num() - 1);
	int32 a = b - 1;
	float segLen = FMath::Max(arc[b] - arc[a], KINDA_SMALL_NUMBER);
	float t = FMath::Clamp((s - arc[a]) / segLen, 0.f, 1.f);
	if (outTangent) *outTangent = (points[b] - points[a]).GetSafeNormal();
	return FMath::Lerp(points[a], points[b], t);
}

float AVehicleCommandController::FPath::SpeedAt(float s) const
{
	if (speed.Num() < 2) return 0.f;
	s = FMath::Clamp(s, 0.f, total);
	int32 lo = 0, hi = arc.Num() - 1;
	while (lo < hi) {
		int32 mid = (lo + hi) / 2;
		if (arc[mid] < s) lo = mid + 1; else hi = mid;
	}
	int32 b = FMath::Clamp(lo, 1, points.Num() - 1);
	int32 a = b - 1;
	float segLen = FMath::Max(arc[b] - arc[a], KINDA_SMALL_NUMBER);
	float t = FMath::Clamp((s - arc[a]) / segLen, 0.f, 1.f);
	return FMath::Lerp(speed[a], speed[b], t);
}

float AVehicleCommandController::FPath::Project(const FVector& pos, bool bFullSearch)
{
	if (points.Num() < 2) return 0.f;
	int32 first = bFullSearch ? 0 : FMath::Max(0, cursor - 3);
	int32 last = bFullSearch ? points.Num() - 2 : FMath::Min(points.Num() - 2, cursor + 40);
	float bestDistSq = FLT_MAX;
	int32 bestSeg = first;
	float bestT = 0.f;
	for (int32 i = first; i <= last; i++) {
		FVector a = points[i], b = points[i + 1];
		FVector ab = b - a;
		ab.Z = 0.f;
		FVector ap = pos - a;
		ap.Z = 0.f;
		float lenSq = ab.SizeSquared();
		float t = lenSq > KINDA_SMALL_NUMBER ? FMath::Clamp(FVector::DotProduct(ap, ab) / lenSq, 0.f, 1.f) : 0.f;
		FVector foot = a + ab * t;
		float distSq = FVector::DistSquared2D(pos, foot);
		if (distSq < bestDistSq) {
			bestDistSq = distSq;
			bestSeg = i;
			bestT = t;
		}
	}
	cursor = bestSeg;
	return arc[bestSeg] + bestT * (arc[bestSeg + 1] - arc[bestSeg]);
}

float AVehicleCommandController::FPath::ProjectConst(const FVector& pos, int32 first, int32 last, float& outLateral) const
{
	outLateral = FLT_MAX;
	if (points.Num() < 2) return 0.f;
	first = FMath::Clamp(first, 0, points.Num() - 2);
	last = FMath::Clamp(last, first, points.Num() - 2);
	float bestDistSq = FLT_MAX;
	int32 bestSeg = first;
	float bestT = 0.f;
	for (int32 i = first; i <= last; i++) {
		FVector ab = points[i + 1] - points[i];
		ab.Z = 0.f;
		FVector ap = pos - points[i];
		ap.Z = 0.f;
		const float lenSq = ab.SizeSquared();
		const float t = lenSq > KINDA_SMALL_NUMBER ? FMath::Clamp(FVector::DotProduct(ap, ab) / lenSq, 0.f, 1.f) : 0.f;
		const float distSq = FVector::DistSquared2D(pos, points[i] + ab * t);
		if (distSq < bestDistSq) {
			bestDistSq = distSq;
			bestSeg = i;
			bestT = t;
		}
	}
	outLateral = FMath::Sqrt(bestDistSq);
	return arc[bestSeg] + bestT * (arc[bestSeg + 1] - arc[bestSeg]);
}

// ---------------------------------------------------------------------------------------------
// 生命周期 / 指令
// ---------------------------------------------------------------------------------------------

AVehicleCommandController::AVehicleCommandController()
{
	PrimaryActorTick.bCanEverTick = true;
	bStartAILogicOnPossess = false;
	bSetControlRotationFromPawnOrientation = false;
}

void AVehicleCommandController::Setup(Map* inMap, UForeverTrafficFrameworkComponent* inOwner)
{
	map = inMap;
	ownerFramework = inOwner;
	if (inOwner) inOwner->RegisterDriver(this);
}

void AVehicleCommandController::EndPlay(const EEndPlayReason::Type endPlayReason)
{
	ReleaseJunctions();
	Super::EndPlay(endPlayReason);
}

void AVehicleCommandController::OnPossess(APawn* inPawn)
{
	Super::OnPossess(inPawn);
	if (AVehicleElement* vehicle = Cast<AVehicleElement>(inPawn)) {
		drivenVehicle = vehicle;
	}
}

void AVehicleCommandController::PushCommand(const FVehicleCommand& command)
{
	commands.Push(command);
	if (state == EState::Idle) StartNextCommand();
}

void AVehicleCommandController::ClearCommands()
{
	if (state == EState::Dwelling && bStopEntered) OnStopLeft(stopNodeId);
	bStopEntered = false;
	commands.Clear();
	ReleaseJunctions();
	path = FPath();
	state = EState::Idle;
	lastNodeId = -1;
	stopNodeId = -1;
	targetNode = -1;
	stuckSeconds = 0.f;
}

void AVehicleCommandController::FinishCommand()
{
	commands.Advance();
	ReleaseJunctions();
	path = FPath();
	state = EState::Idle;
	StartNextCommand();
}

void AVehicleCommandController::StartNextCommand()
{
	FVehicleCommand* command = commands.Current();
	AVehicleElement* vehicle = drivenVehicle.Get();
	if (!command || !vehicle) return;

	if (command->type == FVehicleCommand::EType::NavigateToNode) {
		targetNode = command->node;
		if (!PlanAndFollow(-1, command->node)) {
			const int32 failedNode = command->node;
			FinishCommand();
			if (navigateFinished) navigateFinished(this, failedNode, false);
		}
		return;
	}

	// LoopNodes
	const int32 count = command->nodes.Num();
	if (count < 2) {
		UE_LOG(LogTemp, Warning, TEXT("AVehicleCommandController: LoopNodes节点数不足2，丢弃"));
		FinishCommand();
		return;
	}
	loopIndex = ((command->loopStart % count) + count) % count;
	if (command->bStartAtStop) {
		const int32 prev = command->nodes[(loopIndex - 1 + count) % count];
		lastNodeId = prev;
		BeginDwell(prev, command->startDwellSeconds, command->bFireEnterAtStart, command->bStopAlreadyEntered);
		return;
	}
	lastNodeId = -1; // 中途接管：第一腿从当前车位出发
	targetNode = command->nodes[loopIndex];
	if (!PlanAndFollow(-1, targetNode)) {
		// 规划失败：整个循环放弃（站点不通），避免每帧重试。
		FinishCommand();
	}
}

bool AVehicleCommandController::PlanAndFollow(int32 fromNode, int32 toNode)
{
	AVehicleElement* vehicle = drivenVehicle.Get();
	if (!vehicle || !map) return false;

	ReleaseJunctions();
	Map::VehicleRoutePlan plan;
	if (fromNode >= 0) {
		plan = map->PlanVehicleLeg(fromNode, toNode);
	}
	else {
		const FVector loc = vehicle->GetActorLocation();
		const FVector forward = vehicle->GetActorForwardVector();
		Vehicle* core = vehicle->GetVehicle();
		plan = map->PlanVehicleRouteToNode(core ? core->GetCurrentRoom() : nullptr,
			loc.X / kVehicleControllerWorldScale, loc.Y / kVehicleControllerWorldScale, toNode, true, forward.X, forward.Y);
		// 已经从房间/停车位出发了，之后不再算"停在房间里"。
		if (core && plan.status == Map::VehicleRoutePlan::OK) core->SetCurrentRoom(nullptr);
	}
	if (plan.status != Map::VehicleRoutePlan::OK || plan.points.empty()) {
		Vehicle* failCore = vehicle->GetVehicle();
		Room* failRoom = failCore ? failCore->GetCurrentRoom() : nullptr;
		UE_LOG(LogTemp, Warning, TEXT("AVehicleCommandController: 规划不到节点 %d (from %d)，status=%d(0=OK,1=无路径到终点,2=起点无出路)，起点房间=%s"),
			toNode, fromNode, static_cast<int32>(plan.status), failRoom ? UTF8_TO_TCHAR(failRoom->GetAddress().c_str()) : TEXT("无(室外)"));
		return false;
	}

	TArray<FVector> raw;
	raw.Reserve(plan.points.size());
	for (const Map::NavPoint& p : plan.points) {
		raw.Add(FVector(p.x * kVehicleControllerWorldScale, p.y * kVehicleControllerWorldScale, p.z * kVehicleControllerWorldScale));
	}
	BuildPath(raw);
	if (path.IsValid() && ownerFramework.IsValid()) {
		// 路径里属于路口的连线记成标记，之后在这些弧长区间前向路口请求通行。
		if (JunctionTraffic* junctionTraffic = ownerFramework->GetJunctionTraffic()) {
			TArray<FEdgeSpan> spans;
			for (const Map::NavEdge& edge : plan.edges) {
				if (!edge.edge || !junctionTraffic->IsManaged(edge.edge)) continue;
				if (edge.firstPoint < 0 || edge.lastPoint >= static_cast<int32>(raw.Num()) || edge.firstPoint >= edge.lastPoint) continue;
				spans.Add({ edge.edge, raw[edge.firstPoint], raw[edge.lastPoint] });
			}
			BuildJunctionMarkers(spans);
		}
	}
	if (!path.IsValid()) {
		// 起点就是终点（已经在那里）：当作立刻到达。
		state = EState::Driving;
		OnArrived();
		return true;
	}

	pathS = path.Project(vehicle->GetActorLocation(), true);
	stuckSeconds = 0.f;
	state = EState::Driving;
	return true;
}

void AVehicleCommandController::BuildPath(const TArray<FVector>& rawInput)
{
	path = FPath();
	if (rawInput.Num() == 0) return;

	// 拐点圆角：折线拐点处提前切入、做一段二次贝塞尔弧，拐角越大提前量越大（d=R*tan(θ/2)，不超过相邻
	// 两段各一半长度）。这样车在拐点之前就开始转，不会冲出拐点外侧；起点终点（站）不动。
	TArray<FVector> raw;
	raw.Add(rawInput[0]);
	for (int32 i = 1; i < rawInput.Num(); i++) {
		if (i == rawInput.Num() - 1) {
			raw.Add(rawInput[i]);
			break;
		}
		const FVector v = rawInput[i];
		FVector inVec = v - rawInput[i - 1];
		FVector outVec = rawInput[i + 1] - v;
		inVec.Z = 0.f;
		outVec.Z = 0.f;
		const float inLen = inVec.Size(), outLen = outVec.Size();
		if (inLen < 1.f || outLen < 1.f) {
			raw.Add(v);
			continue;
		}
		const FVector inDir = inVec / inLen, outDir = outVec / outLen;
		const float turn = FMath::Acos(FMath::Clamp(FVector::DotProduct(inDir, outDir), -1.f, 1.f)); // 拐角（弧度）
		if (turn < FMath::DegreesToRadians(8.f)) {
			raw.Add(v);
			continue;
		}
		const float d = FMath::Min(cornerRadius * FMath::Tan(turn * 0.5f), 0.5f * FMath::Min(inLen, outLen));
		const FVector p0 = v - FVector(inDir.X, inDir.Y, 0.f) * d;
		const FVector p2 = v + FVector(outDir.X, outDir.Y, 0.f) * d;
		const int32 steps = 6;
		for (int32 k = 0; k <= steps; k++) {
			const float t = static_cast<float>(k) / steps;
			raw.Add((1 - t) * (1 - t) * p0 + 2 * (1 - t) * t * v + t * t * p2);
		}
	}

	// 保留原始折线的全部顶点（弯角位置准），长段按pathSpacing细分，过近的点（<100uu）丢掉免得曲率被噪声放大。
	TArray<FVector>& pts = path.points;
	pts.Add(raw[0]);
	for (int32 i = 1; i < raw.Num(); i++) {
		const FVector from = raw[i - 1];
		const FVector segment = raw[i] - from;
		const float segLen = segment.Size();
		if (segLen < 1.f) continue;
		const int32 pieces = FMath::Max(1, FMath::CeilToInt(segLen / pathSpacing));
		for (int32 k = 1; k <= pieces; k++) {
			const FVector p = from + segment * (static_cast<float>(k) / pieces);
			const bool bLast = (i == raw.Num() - 1 && k == pieces);
			if (!bLast && FVector::Dist(p, pts.Last()) < 100.f) continue;
			if (bLast && FVector::Dist(p, pts.Last()) < 100.f && pts.Num() > 1) {
				pts.Last() = p; // 终点覆盖过近的前一个点
				continue;
			}
			pts.Add(p);
		}
	}
	if (pts.Num() < 2) {
		path = FPath();
		return;
	}

	const int32 n = pts.Num();
	path.arc.SetNum(n);
	path.arc[0] = 0.f;
	for (int32 i = 1; i < n; i++) {
		path.arc[i] = path.arc[i - 1] + FVector::Dist2D(pts[i], pts[i - 1]);
	}
	path.total = path.arc.Last();

	// 每点曲率（三点外接圆，Menger），再取相邻三点最大值，偏保守。
	TArray<float> curvature;
	curvature.Init(0.f, n);
	for (int32 i = 1; i < n - 1; i++) {
		FVector2D a(pts[i - 1].X, pts[i - 1].Y), b(pts[i].X, pts[i].Y), c(pts[i + 1].X, pts[i + 1].Y);
		float ab = FVector2D::Distance(a, b), bc = FVector2D::Distance(b, c), ca = FVector2D::Distance(c, a);
		float cross = FMath::Abs(FVector2D::CrossProduct(b - a, c - a)); // 2*面积
		float denom = ab * bc * ca;
		curvature[i] = denom > KINDA_SMALL_NUMBER ? 2.f * cross / denom : 0.f;
	}
	TArray<float> smooth = curvature;
	for (int32 i = 1; i < n - 1; i++) {
		smooth[i] = FMath::Max3(curvature[i - 1], curvature[i], curvature[i + 1]);
	}

	path.speed.SetNum(n);
	for (int32 i = 0; i < n; i++) {
		float k = FMath::Max(smooth[i], 1e-6f);
		path.speed[i] = FMath::Clamp(FMath::Sqrt(maxLateralAccel / k), minCornerSpeed, maxSpeed);
	}
	path.speed[n - 1] = 0.f; // 每条腿的终点（车站/目的地）都要停
	for (int32 i = n - 2; i >= 0; i--) {
		float ds = path.arc[i + 1] - path.arc[i];
		path.speed[i] = FMath::Min(path.speed[i], FMath::Sqrt(path.speed[i + 1] * path.speed[i + 1] + 2.f * maxBrakeDecel * ds));
	}
	// 正向扫描：过弯后只能按profileAccel加速，两个相距很近的弯之间不会被允许冲起来。
	for (int32 i = 1; i < n; i++) {
		float ds = path.arc[i] - path.arc[i - 1];
		path.speed[i] = FMath::Min(path.speed[i], FMath::Sqrt(path.speed[i - 1] * path.speed[i - 1] + 2.f * profileAccel * ds));
	}
	path.cursor = 0;
}

void AVehicleCommandController::BeginDwell(int32 nodeId, float seconds, bool bFireEnter, bool bAlreadyEntered)
{
	state = EState::Dwelling;
	stopNodeId = nodeId;
	dwellRemaining = seconds;
	dwellTotal = seconds;
	bStopEntered = bFireEnter || bAlreadyEntered;
	if (bFireEnter) OnStopEntered(nodeId, true);
}

bool AVehicleCommandController::QueryLoopProgress(int32& outLegIndex, float& outLegFraction, bool& outDwelling, float& outDwellFraction) const
{
	const FVehicleCommand* command = commands.Current();
	if (!command || command->type != FVehicleCommand::EType::LoopNodes) return false;
	const int32 count = command->nodes.Num();
	if (count < 2) return false;

	if (state == EState::Driving) {
		if (!path.IsValid()) return false;
		outDwelling = false;
		outLegIndex = ((loopIndex - 1) % count + count) % count; // 正驶向nodes[loopIndex]
		outLegFraction = FMath::Clamp(pathS / path.total, 0.f, 1.f);
		outDwellFraction = 0.f;
		return true;
	}
	if (state == EState::Dwelling) {
		outDwelling = true;
		outLegIndex = ((loopIndex - 2) % count + count) % count; // 停在nodes[loopIndex-1]，到达它的那一腿
		outLegFraction = 1.f;
		outDwellFraction = dwellTotal > 0.f ? FMath::Clamp(1.f - dwellRemaining / dwellTotal, 0.f, 1.f) : 1.f;
		return true;
	}
	return false;
}

bool AVehicleCommandController::GetRemainingWaypoints(TArray<FVector>& outWaypoints) const
{
	AVehicleElement* vehicle = drivenVehicle.Get();
	if (!vehicle || state != EState::Driving || !path.IsValid()) return false;
	outWaypoints.Reset();
	outWaypoints.Add(vehicle->GetActorLocation());
	for (int32 i = path.cursor + 1; i < path.points.Num(); i++) {
		outWaypoints.Add(path.points[i]);
	}
	return outWaypoints.Num() >= 2;
}

void AVehicleCommandController::DepartFromDwell()
{
	if (bStopEntered) OnStopLeft(stopNodeId);
	bStopEntered = false;

	FVehicleCommand* command = commands.Current();
	if (!command) {
		state = EState::Idle;
		return;
	}
	lastNodeId = stopNodeId;
	stopNodeId = -1;
	targetNode = command->nodes[loopIndex];
	if (!PlanAndFollow(lastNodeId, targetNode)) {
		FinishCommand();
	}
}

void AVehicleCommandController::OnArrived()
{
	FVehicleCommand* command = commands.Current();
	if (!command) {
		state = EState::Idle;
		return;
	}

	if (command->type == FVehicleCommand::EType::NavigateToNode) {
		lastNodeId = targetNode;
		const int32 arrivedNode = targetNode;
		FinishCommand();
		if (navigateFinished) navigateFinished(this, arrivedNode, true);
		return;
	}

	// LoopNodes：到了nodes[loopIndex]，进站、停靠，之后去下一个。
	const int32 count = command->nodes.Num();
	const int32 arrivedNode = command->nodes[loopIndex];
	loopIndex = (loopIndex + 1) % count;
	lastNodeId = arrivedNode;
	BeginDwell(arrivedNode, GetDwellSeconds(arrivedNode), true);
}

// ---------------------------------------------------------------------------------------------
// 有序通行：跟车 + 路口请求
// ---------------------------------------------------------------------------------------------

float AVehicleCommandController::GetHalfLength(AVehicleElement* vehicle)
{
	if (halfLength <= 0.f && vehicle) {
		if (USkeletalMeshComponent* mesh = vehicle->GetCarMesh()) {
			halfLength = mesh->CalcBounds(FTransform::Identity).BoxExtent.X;
		}
		if (halfLength < 100.f) halfLength = 250.f; // 取不到/异常小：按普通轿车半长兜底
	}
	return halfLength;
}

bool AVehicleCommandController::GetTrafficState(FTrafficState& out) const
{
	AVehicleElement* vehicle = drivenVehicle.Get();
	if (!vehicle) return false;
	out.location = vehicle->GetActorLocation();
	out.forward = vehicle->GetActorForwardVector();
	out.speed = bKinematic ? kinSpeed : vehicle->GetForwardSpeed();
	out.halfLength = halfLength > 0.f ? halfLength : 250.f;
	return true;
}

void AVehicleCommandController::BuildJunctionMarkers(const TArray<FEdgeSpan>& spans)
{
	path.junctions.Reset();
	path.junctionCursor = 0;
	bObstacle = false;
	const int32 last = path.points.Num() - 2;
	float previousEnd = 0.f;
	for (const FEdgeSpan& span : spans) {
		float lateral;
		const float startS = path.ProjectConst(span.from, 0, last, lateral);
		const float endS = path.ProjectConst(span.to, 0, last, lateral);
		// 圆角和采样会让投影有几十cm误差；起点不回退到上一条之前，太短/反向的丢掉。
		if (endS <= startS + 1.f || startS < previousEnd - 100.f) continue;
		FJunctionMarker marker;
		marker.conn = span.conn;
		marker.startS = FMath::Max(startS, previousEnd);
		marker.endS = endS;
		path.junctions.Add(marker);
		previousEnd = endS;
	}
}

void AVehicleCommandController::ReleaseJunctions()
{
	UForeverTrafficFrameworkComponent* framework = ownerFramework.Get();
	JunctionTraffic* junctionTraffic = framework ? framework->GetJunctionTraffic() : nullptr;
	if (junctionTraffic) {
		const int64_t entity = JunctionTraffic::MakeEntity(JunctionTraffic::EntityKind::Vehicle, GetUniqueID());
		for (FJunctionMarker& marker : path.junctions) {
			if (marker.bHeld || marker.bRequested) junctionTraffic->Leave(marker.conn, entity);
			marker.bHeld = marker.bRequested = false;
		}
	}
}

bool AVehicleCommandController::FindLeader(float minCenterS, float myHalfLength, FLeader& out) const
{
	UForeverTrafficFrameworkComponent* framework = ownerFramework.Get();
	AVehicleElement* self = drivenVehicle.Get();
	if (!framework || !self || !path.IsValid()) return false;

	const FVector myLocation = self->GetActorLocation();
	const int32 first = FMath::Max(0, path.cursor - 3);
	const int32 last = path.cursor + 120;
	bool bFound = false;
	for (const TWeakObjectPtr<AVehicleCommandController>& weak : framework->GetDrivers()) {
		const AVehicleCommandController* other = weak.Get();
		if (!other || other == this) continue;
		FTrafficState otherState;
		if (!other->GetTrafficState(otherState)) continue;
		if (FVector::Dist2D(otherState.location, myLocation) > trafficLookahead + myHalfLength + otherState.halfLength) continue;

		float lateral;
		const float s = path.ProjectConst(otherState.location, first, last, lateral);
		if (lateral > pathHalfWidth || s <= minCenterS) continue;
		if (bFound && s >= out.centerS) continue;

		FVector tangent;
		const FVector onPath = path.Sample(s, &tangent);
		if (FMath::Abs(onPath.Z - otherState.location.Z) > 400.f) continue; // 上下层（桥/隧道）不算前车

		const FVector2D forward2D(otherState.forward.X, otherState.forward.Y);
		const float along = otherState.speed * FVector2D::DotProduct(forward2D.GetSafeNormal(), FVector2D(tangent.X, tangent.Y).GetSafeNormal());
		out.centerS = s;
		out.rearS = s - otherState.halfLength;
		out.speed = FMath::Max(0.f, along);
		bFound = true;
	}
	return bFound;
}

void AVehicleCommandController::ScanObstacles(AVehicleElement* vehicle, float frontS, float speed, double now)
{
	if (now < nextObstacleScan) return;
	nextObstacleScan = now + 0.1;
	bObstacle = false;

	UWorld* world = GetWorld();
	UForeverTrafficFrameworkComponent* framework = ownerFramework.Get();
	if (!world || !framework) return;

	const float decel = maxBrakeDecel * 0.8f;
	const float forwardSpeed = FMath::Max(speed, 0.f);
	const float scanLength = FMath::Min(trafficLookahead, forwardSpeed * forwardSpeed / (2.f * decel) + forwardSpeed * followTimeGap + 1200.f);

	// 已登记的被驱动车辆由FindLeader按路径判断(带速度)，这里只找其它东西：行人、玩家、没人驾驶的车。
	FCollisionQueryParams params(SCENE_QUERY_STAT(VehicleObstacleScan), false, vehicle);
	for (const TWeakObjectPtr<AVehicleCommandController>& weak : framework->GetDrivers()) {
		if (const AVehicleCommandController* other = weak.Get()) {
			if (AVehicleElement* otherVehicle = other->GetDrivenVehicle()) params.AddIgnoredActor(otherVehicle);
		}
	}
	FCollisionObjectQueryParams objects;
	objects.AddObjectTypesToQuery(ECC_Pawn);
	objects.AddObjectTypesToQuery(ECC_Vehicle);

	constexpr float kStep = 250.f;
	constexpr float kRadius = 170.f;
	for (float distance = 100.f; distance <= scanLength; distance += kStep) {
		const float s = frontS + distance;
		if (s >= path.total) break;
		FVector tangent;
		const FVector point = path.Sample(s, &tangent);
		TArray<FOverlapResult> overlaps;
		if (!world->OverlapMultiByObjectType(overlaps, point + FVector(0.f, 0.f, 150.f), FQuat::Identity, objects,
			FCollisionShape::MakeSphere(kRadius), params)) continue;
		for (const FOverlapResult& overlap : overlaps) {
			const AActor* actor = overlap.GetActor();
			if (!actor) continue;
			obstacle.centerS = s;
			obstacle.rearS = s - kRadius;
			obstacle.speed = FMath::Max(0.f, static_cast<float>(FVector::DotProduct(actor->GetVelocity(), tangent)));
			bObstacle = true;
			return;
		}
	}
}

float AVehicleCommandController::ComputeTrafficLimit(AVehicleElement* vehicle, float speed, bool& bOutStopping)
{
	bOutStopping = false;
	UForeverTrafficFrameworkComponent* framework = ownerFramework.Get();
	if (!bEnableTraffic || !framework || !vehicle || !path.IsValid()) return FLT_MAX;

	const float half = GetHalfLength(vehicle);
	const float frontS = pathS + half;
	const float decel = maxBrakeDecel * 0.8f; // 留余量：实际刹车达不到标称减速度
	const float speedForward = FMath::Max(speed, 0.f);
	float limit = FLT_MAX;

	// 1. 同路径前车：Krauss安全速度 v = -bT + sqrt((bT)^2 + vl^2 + 2*b*gap)，保证前车急刹也撞不上。
	FLeader leader;
	const bool bHasLeader = FindLeader(pathS, half, leader);
	if (bHasLeader) {
		const float gap = FMath::Max(0.f, leader.rearS - frontS - standstillGap);
		const float bt = decel * followTimeGap;
		limit = FMath::Min(limit, -bt + FMath::Sqrt(bt * bt + leader.speed * leader.speed + 2.f * decel * gap));
	}

	// 1b. 行人/玩家等不在登记表里的障碍：同样按Krauss提前减速，不能等撞上之后靠推挤。
	ScanObstacles(vehicle, frontS, speed, framework->GetTrafficClock());
	if (bObstacle) {
		const float gap = FMath::Max(0.f, obstacle.rearS - frontS - standstillGap);
		const float bt = decel * followTimeGap;
		limit = FMath::Min(limit, -bt + FMath::Sqrt(bt * bt + obstacle.speed * obstacle.speed + 2.f * decel * gap));
	}

	// 2. 路口：只和连线打交道。进入视距后每junctionRequestInterval请求一次，放行后继续续租，
	//    车尾出了连线终点才Leave；没放行就在停止线前停住。
	JunctionTraffic* junctionTraffic = path.junctions.Num() > 0 ? framework->GetJunctionTraffic() : nullptr;
	if (junctionTraffic) {
		const double now = framework->GetTrafficClock();
		const int64_t entity = JunctionTraffic::MakeEntity(JunctionTraffic::EntityKind::Vehicle, GetUniqueID());
		const float lengthMap = 2.f * half / kVehicleControllerWorldScale;
		for (int32 i = path.junctionCursor; i < path.junctions.Num(); i++) {
			FJunctionMarker& marker = path.junctions[i];
			if (pathS - half > marker.endS) { // 车尾已经出了这条连线
				if (marker.bHeld || marker.bRequested) junctionTraffic->Leave(marker.conn, entity);
				marker.bHeld = marker.bRequested = false;
				if (i == path.junctionCursor) path.junctionCursor++;
				continue;
			}

			const float distToEntry = marker.startS - frontS;
			const float lookDistance = speedForward * speedForward / (2.f * decel) + speedForward * junctionRequestInterval + junctionRequestMargin;
			if (!marker.bHeld && distToEntry > lookDistance) break;
			// 只有队头才请求：我和入口之间还有别的车时，前车自己会请求，我靠跟车限速排队。
			if (!marker.bHeld && bHasLeader && leader.centerS < marker.startS) break;

			if (now >= marker.nextRequest) {
				marker.nextRequest = now + junctionRequestInterval;
				// 出口检查：连线终点之后的车道还剩多少空位（地图单位）。
				float exitFree = 1e9f;
				FLeader ahead;
				if (FindLeader(marker.endS, half, ahead)) exitFree = FMath::Max(0.f, ahead.rearS - marker.endS) / kVehicleControllerWorldScale;
				const bool bGo = junctionTraffic->Request(marker.conn, entity, now, lengthMap, exitFree);
				marker.bRequested = true;
				// 车头已经压进连线了就不再退回去(停在路口里更糟)，当作持有。
				marker.bHeld = bGo || distToEntry < 0.f;
			}
			if (!marker.bHeld) {
				const float distToStop = marker.startS - stopLineMargin - frontS;
				limit = FMath::Min(limit, distToStop <= 0.f ? 0.f : FMath::Sqrt(2.f * decel * distToStop));
				break; // 被挡在这条前面，后面的路口不用看
			}
		}
	}

	bOutStopping = limit < 30.f;
	return limit;
}

// ---------------------------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------------------------

void AVehicleCommandController::Tick(float deltaTime)
{
	Super::Tick(deltaTime);

	AVehicleElement* vehicle = drivenVehicle.Get();
	if (!vehicle) {
		Destroy();
		return;
	}

	// 玩家放手（下车）后车没有Controller了，自动收回；玩家驾驶/搭乘期间它是玩家的。
	if (!vehicle->GetController()) {
		Possess(vehicle);
	}
	const bool bRiding = vehicle->IsRideMode();
	if (vehicle->GetController() != this && !bRiding) return; // 玩家正在自己开

	UpdateExecMode(vehicle);

	if (commands.IsEmpty() && state == EState::Idle) {
		HoldStill(vehicle);
		return;
	}
	if (state == EState::Idle) {
		StartNextCommand();
		if (state == EState::Idle) {
			HoldStill(vehicle);
			return;
		}
	}

	if (state == EState::Dwelling) {
		HoldStill(vehicle);
		dwellRemaining -= deltaTime;
		if (dwellRemaining <= 0.f) DepartFromDwell();
		return;
	}

	TickDriving(vehicle, deltaTime);
}

void AVehicleCommandController::HoldStill(AVehicleElement* vehicle)
{
	if (bKinematic) {
		kinSpeed = 0.f;
		return;
	}
	const float speed = vehicle->GetForwardSpeed();
	if (FMath::Abs(speed) > kHandbrakeSpeed) {
		// 还在滑行：踩刹车停下来（命令刚清空/刚到站速度还没降完）。
		vehicle->SetDriveInput(0.f, 1.f, 0.f);
		vehicle->SetHandbrake(false);
	}
	else {
		vehicle->SetDriveInput(0.f, 0.f, 0.f);
		vehicle->SetHandbrake(true);
	}
}

void AVehicleCommandController::UpdateExecMode(AVehicleElement* vehicle)
{
	bool bWantPhysics = true;
	if (physicsRangeQuery && !vehicle->IsRideMode()) {
		bWantPhysics = physicsRangeQuery(vehicle->GetActorLocation(), !bKinematic);
	}
	if (bWantPhysics && bKinematic) SwitchToPhysics(vehicle);
	else if (!bWantPhysics && !bKinematic) SwitchToKinematic(vehicle);
}

void AVehicleCommandController::ForcePhysicsMode()
{
	AVehicleElement* vehicle = drivenVehicle.Get();
	if (vehicle && bKinematic) SwitchToPhysics(vehicle);
}

void AVehicleCommandController::SwitchToKinematic(AVehicleElement* vehicle)
{
	kinSpeed = FMath::Max(0.f, vehicle->GetForwardSpeed());
	kinYaw = vehicle->GetActorRotation().Yaw;
	if (USkeletalMeshComponent* mesh = vehicle->GetCarMesh()) {
		const FBoxSphereBounds local = mesh->CalcBounds(FTransform::Identity);
		kinZOffset = FMath::Max(0.f, -(local.Origin.Z - local.BoxExtent.Z));
	}
	if (path.IsValid()) pathS = path.Project(vehicle->GetActorLocation(), false);
	vehicle->SetKinematicMode(true);
	bKinematic = true;
}

void AVehicleCommandController::SwitchToPhysics(AVehicleElement* vehicle)
{
	bKinematic = false;
	FVector tangent = vehicle->GetActorForwardVector();
	if (path.IsValid()) {
		FVector pos = path.Sample(pathS, &tangent);
		pos.Z += kinZOffset + kDropOnResumeUU;
		vehicle->SetActorLocationAndRotation(pos, FRotator(0.f, tangent.Rotation().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	}
	else {
		FVector pos = vehicle->GetActorLocation();
		pos.Z += kDropOnResumeUU;
		vehicle->SetActorLocation(pos, false, nullptr, ETeleportType::TeleportPhysics);
	}
	vehicle->ResumePhysics(tangent * (state == EState::Driving ? kinSpeed : 0.f));
	stuckSeconds = 0.f;
}

void AVehicleCommandController::TickDriving(AVehicleElement* vehicle, float deltaTime)
{
	if (!path.IsValid()) {
		FinishCommand();
		return;
	}
	if (bKinematic) TickDrivingKinematic(vehicle, deltaTime);
	else TickDrivingPhysics(vehicle, deltaTime);
}

void AVehicleCommandController::TickDrivingPhysics(AVehicleElement* vehicle, float deltaTime)
{
	const FVector pos = vehicle->GetActorLocation();
	pathS = FMath::Max(pathS, path.Project(pos, false));
	const float speed = vehicle->GetForwardSpeed();
	const float remain = path.total - pathS;
	bool bTrafficStopping = false;
	const float trafficLimit = ComputeTrafficLimit(vehicle, speed, bTrafficStopping);

	// 到站判定：够近且够慢；或者非常近；或者在终点附近卡住不动。
	if (remain < 40.f || (remain <= arriveDistance && FMath::Abs(speed) < arriveSpeed)) {
		vehicle->SetDriveInput(0.f, 0.f, 0.f);
		vehicle->SetHandbrake(true);
		OnArrived();
		return;
	}
	if (remain < kStuckRemainDistance && FMath::Abs(speed) < 20.f && !bTrafficStopping) { // 被前车/路口挡住不算卡住
		stuckSeconds += deltaTime;
		if (stuckSeconds >= kStuckSeconds) {
			vehicle->SetDriveInput(0.f, 0.f, 0.f);
			vehicle->SetHandbrake(true);
			OnArrived();
			return;
		}
	}
	else {
		stuckSeconds = 0.f;
	}

	// 转向：Stanley（航向误差 + 横向偏差修正），紧贴导航线，偏一点就打方向。
	// 航向参考取前方一小段（lookaheadBase+lookaheadGain*v）处的路径切线，弯道提前开始转。
	float maxSteer = vehicle->GetMaxSteerAngleDegrees();
	if (maxSteer < 1.f) maxSteer = kDefaultMaxSteerDegrees;
	const float preview = lookaheadBase + lookaheadGain * FMath::Max(speed, 0.f);
	// 用前轴位置（不是车中心）判断离导航线多远：前轴才是转向的作用点。
	const FVector frontPos = vehicle->GetFrontAxleLocation();
	const float frontS = path.Project(frontPos, false);
	FVector nearTangent;
	const FVector nearest = path.Sample(frontS, &nearTangent);
	FVector aheadTangent;
	path.Sample(frontS + preview, &aheadTangent);
	const float pathYaw = aheadTangent.Rotation().Yaw;
	const float headingError = FMath::FindDeltaAngleDegrees(vehicle->GetActorRotation().Yaw, pathYaw); // >0：路径在右边
	// 车相对导航线的横向偏差（右为正）；偏右就往左修（负）。
	const FVector right(-nearTangent.Y, nearTangent.X, 0.f);
	const float crossTrack = FVector::DotProduct(frontPos - nearest, right.GetSafeNormal());
	const float crossTrackAngle = FMath::RadiansToDegrees(FMath::Atan2(-crossTrackGain * crossTrack, FMath::Max(speed, 0.f) + 150.f));
	const float steer = FMath::Clamp((headingError + crossTrackAngle) / maxSteer, -1.f, 1.f);

	// 速度：剖面已经包含弯前/站前提前减速；这里只做油门/刹车的P控制。
	const float targetSpeed = FMath::Min(path.SpeedAt(pathS + FMath::Max(speed, 0.f) * 0.3f), trafficLimit);
	if (bTrafficStopping && FMath::Abs(speed) < kHandbrakeSpeed) {
		// 要停住且已经很慢：松刹车拉手刹原地等（同HoldStill，低速踩刹车会挂倒挡）。
		vehicle->SetDriveInput(0.f, 0.f, 0.f);
		vehicle->SetHandbrake(true);
		return;
	}
	const float error = targetSpeed - speed;
	float throttle = 0.f, brake = 0.f;
	if (error > 0.f) {
		throttle = FMath::Clamp(throttleKp * error, 0.f, 1.f);
	}
	else if (speed > kHandbrakeSpeed) {
		brake = FMath::Clamp(-brakeKp * error, 0.f, 1.f);
	}
	vehicle->SetHandbrake(false);
	vehicle->SetDriveInput(throttle, brake, steer);
}

void AVehicleCommandController::TickDrivingKinematic(AVehicleElement* vehicle, float deltaTime)
{
	bool bTrafficStopping = false;
	const float trafficLimit = ComputeTrafficLimit(vehicle, kinSpeed, bTrafficStopping);
	const float targetSpeed = FMath::Min(path.SpeedAt(pathS), trafficLimit);
	if (kinSpeed < targetSpeed) kinSpeed = FMath::Min(targetSpeed, kinSpeed + maxAccel * deltaTime);
	else kinSpeed = FMath::Max(targetSpeed, kinSpeed - maxBrakeDecel * deltaTime);
	// 离终点很近时给一个最低爬行速度，避免离散积分在终点前停住。
	const float remain = path.total - pathS;
	if (remain > 40.f && kinSpeed < 60.f && trafficLimit >= 60.f) kinSpeed = FMath::Min(60.f, FMath::Max(targetSpeed, 60.f));
	pathS = FMath::Min(path.total, pathS + kinSpeed * deltaTime);

	FVector tangent;
	FVector pos = path.Sample(pathS, &tangent);
	pos.Z += kinZOffset;
	const float targetYaw = tangent.Rotation().Yaw;
	kinYaw = FMath::FixedTurn(kinYaw, targetYaw, 120.f * deltaTime);
	const float horiz = FVector(tangent.X, tangent.Y, 0.f).Size();
	const float pitch = FMath::RadiansToDegrees(FMath::Atan2(tangent.Z, FMath::Max(horiz, KINDA_SMALL_NUMBER)));
	vehicle->SetActorLocationAndRotation(pos, FRotator(pitch, kinYaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);

	const float remainNow = path.total - pathS;
	if (remainNow < 5.f || (remainNow <= arriveDistance && kinSpeed < arriveSpeed)) {
		kinSpeed = 0.f;
		OnArrived();
	}
}
