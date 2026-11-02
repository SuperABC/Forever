#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Controller/CommandQueue.h"

#include "VehicleCommandController.generated.h"

class AVehicleElement;
class UForeverTrafficFrameworkComponent;
class Map;

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

// 载具Controller基类（公交现在用，以后各种交通工具共用）：维护指令列表+当前执行哪条（TCommandQueue），
// 用和玩家WASD完全一样的油门/刹车/转向输入（AVehicleElement::SetDriveInput）开车。
//
// 路径跟随：指令 -> Map规划的折线（Map单位x1000=UE单位）-> 250uu重采样 -> 每点曲率限速
// v=sqrt(aLat/k) -> 终点v=0并反向扫描刹车距离，得到"弯前/站前提前减速"的速度剖面；转向用pure pursuit，
// 油门/刹车对速度误差做P控制，到站拉手刹。
//
// 执行模式：物理（真实Chaos驾驶）/运动学（不发输入，Controller自己积分速度沿同一条路径推进，关物理）。
// 远离玩家（地形LOD范围外，物理不可靠）自动切运动学，靠近切回物理；指令/停站状态机两种模式共用，所以切换不丢状态。
// 范围判定由owner（UForeverTrafficFrameworkComponent）通过SetPhysicsRangeQuery注入，带滞回。
//
// 控制权：drivenVehicle是弱引用。玩家搭乘（SetRideMode）时Possess权在玩家手里，这里照样每帧发输入；玩家驾驶
// （非搭乘）时不发输入；玩家放手后车没有Controller了，下一帧自动重新Possess回来。
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

	AVehicleElement* GetDrivenVehicle() const { return drivenVehicle.Get(); }
	bool IsKinematic() const { return bKinematic; }
	// 玩家要上车驾驶等场合：运动学段没有物理没法开，先切回物理。
	void ForcePhysicsMode();

	virtual void OnPossess(APawn* inPawn) override;
	virtual void Tick(float deltaTime) override;

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

	struct FPath
	{
		TArray<FVector> points;
		TArray<float> arc;    // 累计弧长
		TArray<float> speed;  // 速度剖面
		float total = 0.f;
		int32 cursor = 0;
		bool IsValid() const { return points.Num() >= 2 && total > 1.f; }
		FVector Sample(float s, FVector* outTangent = nullptr) const;
		float SpeedAt(float s) const;
		// 把pos投影到路径上（窗口向前搜索），更新cursor，返回弧长s。
		float Project(const FVector& pos, bool bFullSearch);
	};

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

	bool bKinematic = false;
	float kinSpeed = 0.f;
	float kinZOffset = 0.f;
	float kinYaw = 0.f;
};
