#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Controller/CommandQueue.h"

#include "CitizenController.generated.h"

class ACitizenElement;
class Room;

// 人物Controller的一条指令。
struct FCharacterCommand
{
	enum class EType
	{
		// 沿waypoints（UE世界坐标）逐点走过去，走完回调framework->NotifyArrived(citizen, room)。
		FollowPath,
		// Controller自己向framework要一条从当前位置到room的行人路径再走（见UForeverPopulaceFrameworkComponent::PlanWalkPath）。
		GoToRoom,
	};

	EType type = EType::FollowPath;
	TArray<FVector> waypoints;
	Room* room = nullptr;
	float speedScale = 1.f;       // 走路速度倍率
	bool bExemptDespawn = false;  // 执行期间不被距离流式销毁

	static FCharacterCommand MakeFollowPath(const TArray<FVector>& inWaypoints, Room* inRoom)
	{
		FCharacterCommand command;
		command.type = EType::FollowPath;
		command.waypoints = inWaypoints;
		command.room = inRoom;
		return command;
	}

	static FCharacterCommand MakeGoToRoom(Room* inRoom)
	{
		FCharacterCommand command;
		command.type = EType::GoToRoom;
		command.room = inRoom;
		return command;
	}
};

// 一个ACitizenElement的"自主行动"Controller：只在市民被spawn期间存在（ACitizenElement::Init里创建并
// Possess，EndPlay里销毁），市民被流式销毁时它跟着销毁，正在执行的指令/路径一并消失。
// 玩家占有市民时引擎会让出这个Controller（OnUnPossess里清空指令），玩家放手后由ACitizenElement重新Possess。
// 移动：每帧朝当前路径点AddMovementInput，只判水平距离（不用UPathFollowingComponent，它的到达判定
// 含竖直方向，会因为行人图节点Z和胶囊体脚底对不上而原地打转，见CitizenElement.md）。空闲时关Tick。
UCLASS()
class FOREVER_API ACitizenController : public AAIController
{
	GENERATED_BODY()

public:
	ACitizenController();

	// 追加一条指令；之前是空闲的话立刻开始执行。
	void PushCommand(const FCharacterCommand& command);
	// 清空全部指令并还原市民状态（速度/免销毁标记/移动模式）。
	void ClearCommands();
	bool HasCommands() const { return !commands.IsEmpty(); }
	// 当前正在走的指令的目标房间，没有则nullptr（存档时用来把走路中的市民记成"已经到了"）。
	Room* GetCurrentDestination() const;
	// 当前FollowPath指令的剩余路径（世界坐标，第一个点是市民当前位置）和目标房间。没有可走的指令返回false。
	// 市民被流式销毁时用它把行进交还给Core（Citizen::ResumeWalk），"你在去哪"折线也用它。
	bool GetRemainingWaypoints(TArray<FVector>& outWaypoints, Room*& outDestination) const;

	virtual void OnPossess(APawn* inPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float deltaTime) override;

private:
	void BeginCommand(FCharacterCommand& command);
	// arrived=true 表示走到了终点；false 表示被打断。
	void EndCommand(bool arrived);

	TCommandQueue<FCharacterCommand> commands;
	TWeakObjectPtr<ACitizenElement> citizenElement;

	bool bCommandStarted = false;
	int32 waypointIndex = 0;
	float savedMaxWalkSpeed = 0.f;

	// 前进方向的横向扰动：相向而行的两个人沿同一条线对撞时会顶牛，每个人加一个随时间缓慢摆动的横向分量
	// （相位每人随机），两人摆到不同侧就错开了。amplitude是横向分量占前进分量的比例（0=关闭），
	// 离当前路径点不到wanderFadeDistance（UE单位）时线性衰减到0，保证能走进到达判定圈。
	UPROPERTY(EditAnywhere, Category = "Citizen|Steering")
	float wanderAmplitude = 0.1f;

	UPROPERTY(EditAnywhere, Category = "Citizen|Steering")
	float wanderFrequency = 1.2f; // 弧度/秒

	UPROPERTY(EditAnywhere, Category = "Citizen|Steering")
	float wanderFadeDistance = 250.f;

	float wanderPhase = 0.f;
};
