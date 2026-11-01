#include "Controller/VehicleCommandController.h"

#include "Element/VehicleElement.h"
#include "Framework/ForeverTrafficFrameworkComponent.h"

#include "map/map.h"
#include "traffic/vehicle.h"

#include "Components/SkeletalMeshComponent.h"

namespace {
	// 地图单位 -> UE单位，同ForeverTrafficFrameworkComponent.cpp的TRAFFIC_WORLD_SCALE。
	constexpr float kWorldScale = 1000.f;
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
		if (!PlanAndFollow(-1, command->node)) FinishCommand();
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

	Map::VehicleRoutePlan plan;
	if (fromNode >= 0) {
		plan = map->PlanVehicleLeg(fromNode, toNode);
	}
	else {
		const FVector loc = vehicle->GetActorLocation();
		const FVector forward = vehicle->GetActorForwardVector();
		Vehicle* core = vehicle->GetVehicle();
		plan = map->PlanVehicleRouteToNode(core ? core->GetCurrentRoom() : nullptr,
			loc.X / kWorldScale, loc.Y / kWorldScale, toNode, true, forward.X, forward.Y);
		// 已经从房间/停车位出发了，之后不再算"停在房间里"。
		if (core && plan.status == Map::VehicleRoutePlan::OK) core->SetCurrentRoom(nullptr);
	}
	if (plan.status != Map::VehicleRoutePlan::OK || plan.points.empty()) {
		UE_LOG(LogTemp, Warning, TEXT("AVehicleCommandController: 规划不到节点 %d (from %d)"), toNode, fromNode);
		return false;
	}

	TArray<FVector> raw;
	raw.Reserve(plan.points.size());
	for (const Map::NavPoint& p : plan.points) {
		raw.Add(FVector(p.x * kWorldScale, p.y * kWorldScale, p.z * kWorldScale));
	}
	BuildPath(raw);
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
	bStopEntered = bFireEnter || bAlreadyEntered;
	if (bFireEnter) OnStopEntered(nodeId, true);
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
		FinishCommand();
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

	// 到站判定：够近且够慢；或者非常近；或者在终点附近卡住不动。
	if (remain < 40.f || (remain <= arriveDistance && FMath::Abs(speed) < arriveSpeed)) {
		vehicle->SetDriveInput(0.f, 0.f, 0.f);
		vehicle->SetHandbrake(true);
		OnArrived();
		return;
	}
	if (remain < kStuckRemainDistance && FMath::Abs(speed) < 20.f) {
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
	const float wheelbase = FMath::Max(vehicle->ComputeWheelbase(), 200.f);
	(void)wheelbase;
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
	const float targetSpeed = path.SpeedAt(pathS + FMath::Max(speed, 0.f) * 0.3f);
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
	const float targetSpeed = path.SpeedAt(pathS);
	if (kinSpeed < targetSpeed) kinSpeed = FMath::Min(targetSpeed, kinSpeed + maxAccel * deltaTime);
	else kinSpeed = FMath::Max(targetSpeed, kinSpeed - maxBrakeDecel * deltaTime);
	// 离终点很近时给一个最低爬行速度，避免离散积分在终点前停住。
	const float remain = path.total - pathS;
	if (remain > 40.f && kinSpeed < 60.f) kinSpeed = FMath::Min(60.f, FMath::Max(targetSpeed, 60.f));
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
