#include "Framework/ForeverTrafficLightFrameworkComponent.h"

#include "Framework/CoreGuard.h"
#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverTrafficFrameworkComponent.h"
#include "Element/TrafficLightElement.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

#include "map/map.h"
#include "map/roadnet.h"
#include "traffic/junction.h"

#include <cmath>

namespace {
	constexpr float kTrafficLightWorldScale = 1000.f; // 地图单位 -> UE厘米，和路网组件的ROAD_WORLD_SCALE一样
	constexpr float kTrafficLightHeightEpsilon = 10.f; // 和路网组件的ROADNET_HEIGHT_EPSILON一样：路面抬高的那一点
	// 没有人行道的一侧：灯杆从路缘角点(整条路横断面的最外缘)往外放到路外的距离(地图单位)。
	constexpr float kCurbOutset = 0.04f;
	// 有人行道的一侧和行道树对齐：从人行道靠车道的内缘往外量人行道宽度的这个比例。
	// 暂时照抄 Basic 路网 mod 的 TREE_SIDEWALK_RATIO(roadnet_checkboard/jing/file 都是0.25)，见 TODO.md。
	constexpr float kSidewalkRatio = 0.25f;
	// 模型原始尺寸太小，统一放大。
	constexpr float kTrafficLightScale = 3.f;
	// 从和树对齐的那点沿道路往路口外(背离路口中心)挪这么多(地图单位，0.2=2米)，完全放在道路单元上，
	// 不压在setback线上的人行横道上。
	constexpr float kAwayFromJunction = 0.2f;
}

UForeverTrafficLightFrameworkComponent::UForeverTrafficLightFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = true;
}

void UForeverTrafficLightFrameworkComponent::RegisterJunctions(const Map& map, const JunctionTraffic& traffic) {
	entries.Reset();
	for (const RoadJunction* junction : map.GetJunctions()) {
		if (!junction || !junction->GetNode()) continue;
		const int32 id = junction->GetNode()->GetId();
		const std::vector<int>* groups = traffic.GetSignalGroups(id);
		if (!groups) continue;
		FJunctionEntry entry;
		entry.junctionId = id;
		entry.center = FVector2D(junction->GetNode()->GetX(), junction->GetNode()->GetY()) * kTrafficLightWorldScale;
		const auto& approaches = junction->GetApproaches();
		for (int32 i = 0; i < static_cast<int32>(approaches.size()) && i < static_cast<int32>(groups->size()); i++) {
			if ((*groups)[i] < 0) continue; // 只出不进的路没有车要停，不放灯
			const RoadJunctionApproach& approach = approaches[i];
			// 灯罩朝模型的-X，要朝向驶来的车，也就是这条路离开路口的方向(angle)：yaw = angle + 180°。
			const float yaw = FMath::RadiansToDegrees(approach.angle) + 180.f;
			const FVector2D left(approach.curbLeft.first, approach.curbLeft.second);
			const FVector2D right(approach.curbRight.first, approach.curbRight.second);
			const FVector2D middle = (left + right) * 0.5f;
			for (int32 side = 0; side < 2; side++) {
				// curbLeft/curbRight不对应真实左右手：isStart端side0是curbLeft，另一端side0是curbRight(roadnet.cpp)。
				const FVector2D curb = (side == 0) == approach.isStart ? left : right;
				const FVector2D outward = (curb - middle).GetSafeNormal();
				// 路缘角点到道路中线的横向距离是这一侧横断面的总宽(roadnet.cpp)，行道树在
				// 车道+停车道+人行道[0]×比例 处，所以从路缘往里收 总宽-那个距离。
				const Road* road = approach.road;
				FVector2D at = curb + outward * kCurbOutset;
				if (road && !road->GetPedestrianLanes(side).empty()) {
					float inner = 0.f;
					for (float w : road->GetVehicleLanes(side)) inner += w;
					for (float w : road->GetParkingLanes(side)) inner += w;
					const float inset = road->GetSideWidth(side) - (inner + road->GetPedestrianLanes(side)[0] * kSidewalkRatio);
					at = curb - outward * inset;
				}
				// angle就是道路离开路口的方向。
				at += FVector2D(FMath::Cos(approach.angle), FMath::Sin(approach.angle)) * kAwayFromJunction;
				const FVector location(at.X * kTrafficLightWorldScale, at.Y * kTrafficLightWorldScale,
					approach.curbZ * kTrafficLightWorldScale + kTrafficLightHeightEpsilon);
				entry.transforms.Add(FTransform(FRotator(0.f, yaw, 0.f), location, FVector(kTrafficLightScale)));
				entry.approaches.Add(i);
			}
		}
		if (entry.transforms.Num() > 0) entries.Add(MoveTemp(entry));
	}
	UE_LOG(LogTemp, Log, TEXT("红绿灯：登记了 %d 个路口"), entries.Num());
}

void UForeverTrafficLightFrameworkComponent::DestroyAll(JunctionTraffic* traffic) {
	for (FJunctionEntry& entry : entries) {
		if (ATrafficLightElement* element = entry.element.Get()) element->Destroy();
		entry.element = nullptr;
		if (traffic) traffic->SetSignalActive(entry.junctionId, false, 0.0);
	}
}

void UForeverTrafficLightFrameworkComponent::EndPlay(const EEndPlayReason::Type endPlayReason) {
	// Core的路口表可能已经没了，这里不碰它；Actor随关卡一起销毁。
	DestroyAll(nullptr);
	entries.Reset();
	registeredFor = nullptr;
	Super::EndPlay(endPlayReason);
}

void UForeverTrafficLightFrameworkComponent::TickComponent(float deltaTime, ELevelTick tickType, FActorComponentTickFunction* thisTickFunction) {
	Super::TickComponent(deltaTime, tickType, thisTickFunction);
	CORE_GUARD_BEGIN("UForeverTrafficLightFrameworkComponent::TickComponent")
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	if (!framework || !framework->IsInitialGenerationDone() || !GetWorld()) return;
	UForeverTrafficFrameworkComponent* trafficFramework = framework->GetTrafficFramework();
	JunctionTraffic* traffic = trafficFramework ? trafficFramework->GetJunctionTraffic() : nullptr;
	Map* map = framework->GetMap();
	if (!traffic || !map) return;
	if (traffic != registeredFor) {
		DestroyAll(nullptr); // 旧表已经被交通组件释放了
		RegisterJunctions(*map, *traffic);
		registeredFor = traffic;
	}
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn) return;

	const FVector2D player(pawn->GetActorLocation());
	const double spawnLimit = FMath::Max(0.f, trafficLightSpawnDistance) * kTrafficLightWorldScale;
	const double despawnLimit = FMath::Max(trafficLightSpawnDistance, trafficLightDespawnDistance) * kTrafficLightWorldScale;
	const double spawnSquared = spawnLimit * spawnLimit, despawnSquared = despawnLimit * despawnLimit;
	const double now = trafficFramework->GetTrafficClock(); // 和车辆请求路口用同一个时钟(现实秒，暂停时停)

	for (FJunctionEntry& entry : entries) {
		ATrafficLightElement* element = entry.element.Get();
		const double distSquared = FVector2D::DistSquared(player, entry.center);
		if (!element) {
			if (distSquared >= spawnSquared) continue;
			FActorSpawnParameters params;
			params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FVector location(entry.center, entry.transforms[0].GetLocation().Z);
			element = GetWorld()->SpawnActor<ATrafficLightElement>(location, FRotator::ZeroRotator, params);
			if (!element) continue;
			element->Build(entry.junctionId, entry.transforms, entry.approaches);
			entry.element = element;
			traffic->SetSignalActive(entry.junctionId, true, now); // 从第一组绿灯开始
		}
		else if (distSquared > despawnSquared) {
			element->Destroy();
			entry.element = nullptr;
			traffic->SetSignalActive(entry.junctionId, false, now);
			continue;
		}
		element->RefreshSignals(*traffic, now);
	}
	CORE_GUARD_END
}
