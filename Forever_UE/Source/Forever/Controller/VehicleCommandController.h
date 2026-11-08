#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Controller/CommandQueue.h"

#include "VehicleCommandController.generated.h"

class AVehicleElement;
class UForeverTrafficFrameworkComponent;
class Map;
class Connection;
class JunctionTraffic;

// 交通工具Controller的一条指令。Core/UE只负责往Controller里塞指令，开车细节全在Controller里。
struct FVehicleCommand
{
	enum class EType
	{
		// 从当前位置导航到某个节点（vehicleNavGraph节点id，或Map辅助导航节点=公交站的stationNode id），到了这条指令结束。
		NavigateToNode,
		// 按nodes循环行驶，永不结束，直到ClearCommands/StopNow。到站（Map辅助节点）时由子类决定停靠/发事件。
		LoopNodes,
	};

	EType type = EType::NavigateToNode;
	int32 node = -1;
	TArray<int32> nodes;
	int32 loopStart = 0;               // LoopNodes：第一个要去的节点在nodes里的下标
	bool bStartAtStop = false;         // LoopNodes：车当前就停在nodes[loopStart-1]（循环的上一个节点）
	float startDwellSeconds = 0.f;     // bStartAtStop时先在那个站再停多久（真实秒）
	bool bFireEnterAtStart = false;    // bStartAtStop时是否对那个站补发进站（NavigateToNode刚到站后接循环用）
	bool bStopAlreadyEntered = false;  // bStartAtStop时进站事件已经发过（Core时刻表接管给物理时），只负责之后补发出站

	static FVehicleCommand MakeNavigate(int32 inNode)
	{
		FVehicleCommand command;
		command.type = EType::NavigateToNode;
		command.node = inNode;
		return command;
	}

	static FVehicleCommand MakeLoop(const TArray<int32>& inNodes, int32 inStart)
	{
		FVehicleCommand command;
		command.type = EType::LoopNodes;
		command.nodes = inNodes;
		command.loopStart = inStart;
		return command;
	}
};

// 载具Controller基类：指令队列驱动的自动驾驶，路径跟随/物理-运动学双模式/控制权交接见VehicleCommandController.md。
UCLASS()
class FOREVER_API AVehicleCommandController : public AAIController
{
	GENERATED_BODY()

public:
	AVehicleCommandController();

	void Setup(Map* inMap, UForeverTrafficFrameworkComponent* inOwner);
	// 返回true=该位置应该用物理；第二个参数是当前是不是物理（用来做滞回）。
	void SetPhysicsRangeQuery(TFunction<bool(const FVector&, bool)> query) { physicsRangeQuery = MoveTemp(query); }

	void PushCommand(const FVehicleCommand& command);
	// 清空全部指令：停站中补发出站，行驶中刹停，之后原地保持刹车。
	void ClearCommands();
	void StopNow() { ClearCommands(); }
	bool HasCommands() const { return !commands.IsEmpty(); }

	// 正在执行LoopNodes时的循环进度，供交还Core时刻表时对齐进度用（见UForeverTrafficFrameworkComponent::BusToKinematic）。
	// legIndex：车所在的那一腿（leg i = 从nodes[i]到nodes[i+1]）；行驶中legFraction是这一腿已走的比例；
	// 停靠中dwelling=true，车在nodes[legIndex+1]里，dwellFraction是停靠已过的比例。
	// 不在循环指令里/没有有效路径时返回false。
	bool QueryLoopProgress(int32& outLegIndex, float& outLegFraction, bool& outDwelling, float& outDwellFraction) const;

	// 正在行驶时的剩余路径（世界坐标，第一个点是车当前位置，其后是路径上还没走过的点）。
	// 空闲/停靠中/没有有效路径返回false。"你在去哪"折线（UForeverTrafficFrameworkComponent::StartTestNavigation）用它。
	bool GetRemainingWaypoints(TArray<FVector>& outWaypoints) const;

	// NavigateToNode指令结束时回调(bArrived=false表示规划不到)，在指令已出队、控制器回到Idle之后调用，
	// 所以回调里可以直接PushCommand。LoopNodes不回调。
	void SetNavigateFinishedCallback(TFunction<void(AVehicleCommandController*, int32, bool)> callback) { navigateFinished = MoveTemp(callback); }

	AVehicleElement* GetDrivenVehicle() const { return drivenVehicle.Get(); }
	bool IsKinematic() const { return bKinematic; }
	// 玩家要上车驾驶等场合：运动学段没有物理没法开，先切回物理。
	void ForcePhysicsMode();

	// 给同一路网上其它Controller做跟车判断用的当前状态（UE单位，cm）。没有驾驶的车返回false。
	struct FTrafficState
	{
		FVector location = FVector::ZeroVector;
		FVector forward = FVector::ForwardVector;
		float speed = 0.f;      // 沿车头方向的速度，倒车为负
		float halfLength = 0.f;
	};
	bool GetTrafficState(FTrafficState& out) const;

	virtual void OnPossess(APawn* inPawn) override;
	virtual void Tick(float deltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

	// ---- 可调参数（UE单位，cm） ----
	UPROPERTY(EditAnywhere, Category = "Drive")
	float maxSpeed = 1500.f;
	// 允许的横向加速度，决定弯道限速（防侧翻关键）。
	UPROPERTY(EditAnywhere, Category = "Drive")
	float maxLateralAccel = 250.f;
	UPROPERTY(EditAnywhere, Category = "Drive")
	float maxBrakeDecel = 350.f;
	UPROPERTY(EditAnywhere, Category = "Drive")
	float maxAccel = 250.f; // 运动学模式的加速度
	// 速度剖面里过弯后的加速度上限（越小，弯与弯之间越不敢加速）。
	UPROPERTY(EditAnywhere, Category = "Drive")
	float profileAccel = 120.f;
	UPROPERTY(EditAnywhere, Category = "Drive")
	float minCornerSpeed = 350.f;
	UPROPERTY(EditAnywhere, Category = "Drive")
	float throttleKp = 0.006f;
	UPROPERTY(EditAnywhere, Category = "Drive")
	float brakeKp = 0.01f;
	UPROPERTY(EditAnywhere, Category = "Drive")
	float lookaheadBase = 150.f; // 航向参考的前视距离
	UPROPERTY(EditAnywhere, Category = "Drive")
	float lookaheadGain = 0.2f; // 秒
	// 横向偏差修正增益（越大越紧贴导航线，太大会左右摆）。
	UPROPERTY(EditAnywhere, Category = "Drive")
	float crossTrackGain = 3.f;
	UPROPERTY(EditAnywhere, Category = "Drive")
	float arriveDistance = 250.f;
	UPROPERTY(EditAnywhere, Category = "Drive")
	float arriveSpeed = 120.f;
	// 导航线拐点的圆角半径：拐角越大提前切入越多（提前量=半径*tan(拐角/2)），越大越早开始转。
	UPROPERTY(EditAnywhere, Category = "Drive")
	float cornerRadius = 1500.f;
	UPROPERTY(EditAnywhere, Category = "Drive")
	float pathSpacing = 250.f;

	// ---- 有序通行（跟车 + 路口请求），见VehicleCommandController.md"有序通行" ----
	UPROPERTY(EditAnywhere, Category = "Traffic")
	bool bEnableTraffic = true;
	// 跟车时距（秒）：Krauss安全速度里的反应时间T。
	UPROPERTY(EditAnywhere, Category = "Traffic")
	float followTimeGap = 0.8f;
	// 停稳后车头离前车车尾的最小间距。
	UPROPERTY(EditAnywhere, Category = "Traffic")
	float standstillGap = 200.f;
	// 被路口挡住时车头停在路口入口（停止线）之前多远。
	UPROPERTY(EditAnywhere, Category = "Traffic")
	float stopLineMargin = 250.f;
	// 路口请求的视距余量：刹停距离 + 速度*请求间隔 + 这个值以内才开始对下一条路口连线请求。
	UPROPERTY(EditAnywhere, Category = "Traffic")
	float junctionRequestMargin = 600.f;
	// 对路口请求的间隔（交通时钟秒，租约是它的10倍）。
	UPROPERTY(EditAnywhere, Category = "Traffic")
	float junctionRequestInterval = 0.1f;
	// 前方多宽以内的车算"在我的路径上"（中心线横向距离，UE单位）。
	UPROPERTY(EditAnywhere, Category = "Traffic")
	float pathHalfWidth = 240.f;
	// 跟车/路口出口检查往前看多远。
	UPROPERTY(EditAnywhere, Category = "Traffic")
	float trafficLookahead = 6000.f;

protected:
	// 到站/出站钩子（子类发事件、决定停靠时长）。bLoop=true表示是LoopNodes里的站。
	virtual void OnStopEntered(int32 nodeId, bool bLoop) {}
	virtual void OnStopLeft(int32 nodeId) {}
	// 到达循环里的节点后停靠多久（真实秒），<=0不停靠直接走。
	virtual float GetDwellSeconds(int32 nodeId) const { return 0.f; }

	Map* GetMap() const { return map; }
	UForeverTrafficFrameworkComponent* GetOwnerFramework() const { return ownerFramework.Get(); }

private:
	enum class EState { Idle, Driving, Dwelling };

	// 路径上的一条路口连线（JunctionTraffic管理的Connection）所占的弧长区间，和请求/持有状态。
	struct FJunctionMarker
	{
		const Connection* conn = nullptr;
		float startS = 0.f;
		float endS = 0.f;
		bool bHeld = false;        // 路口已放行（持有中，续租）
		bool bRequested = false;   // 曾经请求过（离开/放弃时要Leave，清掉排队记录）
		double nextRequest = 0.0;  // 下次可请求的交通时钟
	};

	// 规划结果里一条路口连线对应的世界坐标起终点，BuildJunctionMarkers用它在建好的路径上找弧长。
	struct FEdgeSpan
	{
		const Connection* conn = nullptr;
		FVector from = FVector::ZeroVector;
		FVector to = FVector::ZeroVector;
	};

	struct FPath
	{
		TArray<FVector> points;
		TArray<float> arc;    // 累计弧长
		TArray<float> speed;  // 速度剖面
		TArray<FJunctionMarker> junctions; // 按弧长升序
		int32 junctionCursor = 0;          // 第一个车尾还没出去的连线
		float total = 0.f;
		int32 cursor = 0;
		bool IsValid() const { return points.Num() >= 2 && total > 1.f; }
		FVector Sample(float s, FVector* outTangent = nullptr) const;
		float SpeedAt(float s) const;
		// 把pos投影到路径上（窗口向前搜索），更新cursor，返回弧长s。
		float Project(const FVector& pos, bool bFullSearch);
		// 只读版本：在线段[first,last]里找最近点，返回弧长，outLateral是水平距离。
		float ProjectConst(const FVector& pos, int32 first, int32 last, float& outLateral) const;
	};

	// 前方最近的一辆车（跟车对象）或障碍。
	struct FLeader
	{
		float centerS = 0.f; // 它的中心在我路径上的弧长
		float rearS = 0.f;   // 它的车尾在我路径上的弧长（和我的车头比较得到间距）
		float speed = 0.f;   // 沿我行进方向的速度（>=0）
	};

	void BuildJunctionMarkers(const TArray<FEdgeSpan>& spans);
	// 释放全部路口持有/排队（换路径、清指令、销毁时）。
	void ReleaseJunctions();
	float GetHalfLength(AVehicleElement* vehicle);
	// 沿路径找中心在minCenterS之后、横向在pathHalfWidth内的最近的其它被驱动车辆。
	bool FindLeader(float minCenterS, float halfLength, FLeader& out) const;
	// 用物体类型(Pawn/Vehicle)沿路径向前扫一遍，找行人、玩家等不在登记表里的障碍，结果缓存0.1秒。
	void ScanObstacles(AVehicleElement* vehicle, float frontS, float speed, double now);
	// 跟车 + 路口 gate 给出的速度上限（UE单位）；没有限制返回FLT_MAX。bOutStopping=上限已逼近0（要停住）。
	float ComputeTrafficLimit(AVehicleElement* vehicle, float speed, bool& bOutStopping);

	TFunction<void(AVehicleCommandController*, int32, bool)> navigateFinished;
	void StartNextCommand();
	void FinishCommand();
	// fromNode<0 = 从当前车位规划；返回是否规划成功并开始行驶。
	bool PlanAndFollow(int32 fromNode, int32 toNode);
	void BuildPath(const TArray<FVector>& rawInput);
	void BeginDwell(int32 nodeId, float seconds, bool bFireEnter, bool bAlreadyEntered = false);
	void OnArrived();
	void DepartFromDwell();

	void TickDriving(AVehicleElement* vehicle, float deltaTime);
	void TickDrivingPhysics(AVehicleElement* vehicle, float deltaTime);
	void TickDrivingKinematic(AVehicleElement* vehicle, float deltaTime);
	void HoldStill(AVehicleElement* vehicle);
	void UpdateExecMode(AVehicleElement* vehicle);
	void SwitchToKinematic(AVehicleElement* vehicle);
	void SwitchToPhysics(AVehicleElement* vehicle);

	TCommandQueue<FVehicleCommand> commands;
	TWeakObjectPtr<AVehicleElement> drivenVehicle;
	Map* map = nullptr;
	TWeakObjectPtr<UForeverTrafficFrameworkComponent> ownerFramework;
	TFunction<bool(const FVector&, bool)> physicsRangeQuery;

	EState state = EState::Idle;
	FPath path;
	float pathS = 0.f;          // 当前沿路径位置（弧长）
	int32 loopIndex = 0;        // LoopNodes：下一个要去的节点下标
	int32 targetNode = -1;      // 当前腿的目标节点
	int32 lastNodeId = -1;      // 刚离开/停靠的节点，<0表示不在节点上（中途接管）
	int32 stopNodeId = -1;      // 正在停靠的节点
	bool bStopEntered = false;  // 停靠中且已经发过进站（出站要配对发）
	float dwellRemaining = 0.f;
	float dwellTotal = 0.f;
	float stuckSeconds = 0.f;
	float halfLength = 0.f;     // 车身半长（UE单位），第一次用到时从骨骼网格取
	FLeader obstacle;           // ScanObstacles的缓存结果(rearS在路径弧长上)
	bool bObstacle = false;
	double nextObstacleScan = 0.0;

	bool bKinematic = false;
	float kinSpeed = 0.f;
	float kinZOffset = 0.f;
	float kinYaw = 0.f;
};
