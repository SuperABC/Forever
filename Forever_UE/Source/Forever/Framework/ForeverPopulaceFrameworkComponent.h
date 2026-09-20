#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"
#include "ForeverPopulaceFrameworkComponent.generated.h"

class Map;
class Populace;
class Citizen;
class Room;
class ACitizenElement;

// 对应旧Framework Actor `Populace`(C++ Base:PopulaceBase)。这次进入populace域时第一次
// 真正填内容：按玩家距离流式生成/销毁ACitizenElement(老工程APopulaceBase同款做法，和
// ABuildingElement"开局全建好、常驻到关卡结束"不是同一套架构——人口规模通常比建筑数量
// 大得多，流式管理是更保险的默认选择)。职责说明见ForeverPopulaceFrameworkComponent.md。
UCLASS()
class FOREVER_API UForeverPopulaceFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverPopulaceFrameworkComponent();

	// 由AForeverFrameworkActor在map->Checkin(*populace)之后调用一次——这次不在这里
	// SpawnActor，只是缓存populace->GetCitizens()列表，真正的生成/销毁全部在
	// TickComponent里按距离做。map生命周期由调用方持有。
	void GenerateCitizens(Map* inMap, Populace* inPopulace);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// PIE停止/退出游戏时置空map——不主动Destroy()场上还活着的ACitizenElement，关卡卸载会
	// 自动清理所有Actor，activeInstances这个TMap本身也会在组件销毁时一起被回收，不需要
	// 手动清空，和UForeverBuildingFrameworkComponent::EndPlay同一个模式。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 按Citizen姓名查找/强制生成对应的ACitizenElement——不看玩家距离，用于"剧情/系统指定
	// 某个具体市民"的场景（如ChangeControlChange，见ForeverStoryFrameworkComponent.cpp）。
	// 已经在activeInstances里直接返回；否则调用SpawnCitizen强制生成一个。找不到同名citizen
	// 或强制生成失败时返回nullptr。
	// @name: 目标citizen姓名（UTF-8转FString比较）
	ACitizenElement* FindOrSpawnCitizenByName(const FString& name);

	// Job按调度产出NPCNavigateChange时，AForeverFrameworkActor::Tick的回调转发到这里：
	// 用destination和市民当前所在room各自的GetNavigationNode()对map->FindPedestrianPath
	// 寻路——citizen当前有已生成的ACitizenElement(查activeInstances)就调它的
	// WalkTo播放真实走路动画；否则(不在流式加载范围内，没有Actor，或者寻路失败)直接瞬移
	// 逻辑状态：SetCurrentRoom(destination)+ClearPosition()(不是SetPosition，见
	// Citizen::ClearPosition()的说明)。
	void RequestWalk(Citizen* citizen, Room* destination);

	// ACitizenElement::WalkTo全部路径点走完后的回调：更新Citizen::SetCurrentRoom。
	void NotifyArrived(Citizen* citizen, Room* destination);

private:
	// 生成一个citizen对应的ACitizenElement并登记进activeInstances——从TickComponent的
	// 按距离生成逻辑里提炼出来的公共部分，TickComponent和FindOrSpawnCitizenByName共用。
	// 调用方负责保证citizen不为空、不在activeInstances里。
	// @citizen: 待生成的citizen
	ACitizenElement* SpawnCitizen(Citizen* citizen);

	Map* map = nullptr;

	// GenerateCitizens时从populace->GetCitizens()缓存一份扁平数组，方便按下标做轮询分片
	// (populace自己的std::vector<Citizen*>是Core类型，这里另存一份UE容器方便TArray惯用法，
	// 两者都只是"读"同一批Citizen*，不涉及所有权)。
	TArray<Citizen*> allCitizens;
	TMap<Citizen*, TObjectPtr<ACitizenElement>> activeInstances;
	int32 pollCursor = 0;

	// 每帧只轮询1/streamingBatchStride的citizen，老工程APopulaceBase同款节流思路，避免
	// citizen总数很大时每帧全量距离检测。
	UPROPERTY(EditDefaultsOnly, Category = "Populace")
	int32 streamingBatchStride = 20;

	// 进入这个距离(地图单位)才生成Actor——老工程APopulaceBase同款字面值(2.0)。
	UPROPERTY(EditDefaultsOnly, Category = "Populace")
	float citizenSpawnDistance = 2.f;

	// 超过这个距离才销毁——老工程同款字面值(4.0)，比spawn距离大，形成迟滞区，和building的
	// LOD双阈值同一个防抖动理由。
	UPROPERTY(EditDefaultsOnly, Category = "Populace")
	float citizenDespawnDistance = 4.f;
};
