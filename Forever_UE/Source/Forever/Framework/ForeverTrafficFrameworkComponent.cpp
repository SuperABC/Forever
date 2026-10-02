#include "Framework/ForeverTrafficFrameworkComponent.h"

#include "Framework/ForeverFrameworkActor.h"
#include "Element/VehicleElement.h"
#include "Element/CitizenElement.h"
#include "traffic/traffic.h"
#include "traffic/vehicle.h"
#include "map/map.h"
#include "map/building.h"
#include "map/room.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

// 和BuildingElement.cpp同名常量/同一个公式——这个项目的既有约定是每个需要room-local→世界
// 坐标换算的.cpp文件各自抄一份(ForeverPopulaceFrameworkComponent.cpp也是这么做的)，不额外
// 抽公共函数，见Source/Forever/Framework/ForeverPopulaceFrameworkComponent.cpp顶部注释。
#define TRAFFIC_WORLD_SCALE 1000.f
#define TRAFFIC_HEIGHT_EPSILON 1.f

namespace {
	void ComputeWorldPosition(const Building& building, float localX, float localY,
		float& outWorldX, float& outWorldY) {
		float relX = localX - building.GetBodySizeX() * 0.5f + building.GetBodyOffsetX();
		float relY = localY - building.GetBodySizeY() * 0.5f + building.GetBodyOffsetY();
		float rot = building.GetRotation();
		float c = FMath::Cos(rot), s = FMath::Sin(rot);
		outWorldX = (building.GetPosX() + relX * c - relY * s) * TRAFFIC_WORLD_SCALE;
		outWorldY = (building.GetPosY() + relX * s + relY * c) * TRAFFIC_WORLD_SCALE;
	}
}

void UForeverTrafficFrameworkComponent::GenerateVehicles(Map* map, Traffic* traffic) {
	if (!map || !traffic) return;

	for (auto& [name, vehicle] : traffic->GetVehicles()) {
		if (!vehicle) continue;

		Room* room = vehicle->GetRoom();
		Building* building = room ? room->GetParentBuilding() : nullptr;
		if (!room || !building) continue; // 不是预置在停车位里的车辆(理论上这次Traffic::Init
										   // 生成的车辆都会有room，防御性写法)

		float localX, localY;
		vehicle->GetParkingLocalPosition(localX, localY);

		float worldX, worldY;
		ComputeWorldPosition(*building, room->GetPosX() + localX, room->GetPosY() + localY, worldX, worldY);
		float worldZ = TRAFFIC_HEIGHT_EPSILON + building->GetFloorBaseZ(room->GetLayer()) * TRAFFIC_WORLD_SCALE;
		float worldYaw = FMath::RadiansToDegrees(building->GetRotation()) + vehicle->GetParkingRotationDegrees();

		FString blueprintPath = UTF8_TO_TCHAR(vehicle->GetBlueprintPath().data());
		UClass* vehicleClass = LoadClass<AVehicleElement>(nullptr, *blueprintPath);
		if (!vehicleClass) {
			UE_LOG(LogTemp, Warning, TEXT("UForeverTrafficFrameworkComponent::GenerateVehicles: 加载车辆蓝图失败，路径=%s，车辆=%s"),
				*blueprintPath, UTF8_TO_TCHAR(vehicle->GetName().c_str()));
			continue;
		}

		// carMesh是有真实碰撞、参与物理模拟的骨骼网格(见VehicleElement.cpp构造函数)，停在
		// 房间里这个点离墙/地板的碰撞体很容易贴得很近，默认的生成时碰撞检测会因为"生成点被
		// 占用"直接拒绝生成(SpawnActor返回nullptr)——照抄原ToggleVehicle MOUNT分支的做法，
		// 用AlwaysSpawn跳过这次检测(实测：没加这个之前，PIE日志里每一次SpawnActor都报
		// "SpawnActor failed because of collision at the spawn location"，车辆一辆都没真正
		// 生成出来，见排查记录)。
		FActorSpawnParameters spawnParams;
		spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AVehicleElement* vehicleElement = GetWorld()->SpawnActor<AVehicleElement>(
			vehicleClass, FVector(worldX, worldY, worldZ), FRotator(0.f, worldYaw, 0.f), spawnParams);
		if (!vehicleElement) continue;

		vehicleElement->Init(vehicle, nullptr); // 这时候还没人上车，previousPawn先留空
		activeVehicles.Add(UTF8_TO_TCHAR(vehicle->GetName().c_str()), vehicleElement);
	}
}

void UForeverTrafficFrameworkComponent::ApplyEnterVehicle(const FString& vehicleName, APlayerController* controller) {
	if (!controller) return;

	TObjectPtr<AVehicleElement>* found = activeVehicles.Find(vehicleName);
	AVehicleElement* vehicleElement = found ? found->Get() : nullptr;
	if (!vehicleElement) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverTrafficFrameworkComponent::ApplyEnterVehicle: 找不到车辆 %s"), *vehicleName);
		return;
	}

	APawn* currentPawn = controller->GetPawn();
	if (!currentPawn || currentPawn == vehicleElement) return;

	// 隐藏之后这个pawn会一直停在原地，但UForeverPopulaceFrameworkComponent::TickComponent的
	// 距离流式销毁只看"离当前玩家pawn多远"——玩家开车远离之后这个停在原地的市民会被判定为
	// "走远的市民"直接Destroy掉，previousPawn变成悬空指针，下车时车没了但人也出不来、操控
	// 彻底失灵(照抄原ToggleVehicle MOUNT分支踩过的坑)。上车期间标记豁免，下车时解除。
	if (ACitizenElement* citizenElement = Cast<ACitizenElement>(currentPawn)) {
		citizenElement->SetDespawnExempt(true);
	}

	currentPawn->SetActorHiddenInGame(true);
	currentPawn->SetActorEnableCollision(false);
	vehicleElement->SetPreviousPawn(currentPawn);
	controller->Possess(vehicleElement);
}

void UForeverTrafficFrameworkComponent::ExitVehicle(APlayerController* controller) {
	if (!controller) return;

	AVehicleElement* vehicleElement = Cast<AVehicleElement>(controller->GetPawn());
	if (!vehicleElement) return;

	Vehicle* vehicle = vehicleElement->GetVehicle();
	if (!vehicle) return;

	// 下车点——车身局部坐标系下的偏移(VehicleMod指定，默认左侧)，按车辆当前transform(位置+
	// 完整旋转，侧翻/颠簸时也一起考虑)变换成候选世界坐标。
	FVector localOffset(vehicle->GetExitOffsetX(), vehicle->GetExitOffsetY(), vehicle->GetExitOffsetZ());
	FVector candidateLocation = vehicleElement->GetActorTransform().TransformPositionNoScale(localOffset);

	// 检测这个位置站不站得下人——用ECC_Pawn通道，不能用ECC_Visibility(这个项目的Pawn预设
	// 碰撞档案会忽略ECC_Visibility，见[[memory:pawn_preset_ignores_visibility]])。胶囊体
	// 尺寸照抄AForeverCharacter构造函数里InitCapsuleSize(42.f, 96.0f)的实际数值。
	bool blocked = GetWorld()->OverlapAnyTestByChannel(
		candidateLocation, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(42.f, 96.f));
	if (blocked) {
		if (GEngine) {
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("下车位置被挡住，无法下车"));
		}
		return;
	}

	float dismountYaw = vehicleElement->GetActorRotation().Yaw;
	APawn* previousPawn = vehicleElement->GetPreviousPawn().Get();
	if (!previousPawn) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverTrafficFrameworkComponent::ExitVehicle: previousPawn已失效，玩家将没有可控对象"));
		return;
	}

	// 解除上车时加的距离销毁豁免，恢复正常的流式生成/销毁管理。
	if (ACitizenElement* citizenElement = Cast<ACitizenElement>(previousPawn)) {
		citizenElement->SetDespawnExempt(false);
	}

	// 只取车身的Yaw站直——侧翻/颠簸之后Pitch/Roll可能是任意角度，人下车应该始终站直，不应该
	// 继承车身的翻滚姿态，照抄原ToggleVehicle DISMOUNT分支的说明。
	previousPawn->SetActorLocationAndRotation(candidateLocation, FRotator(0.f, dismountYaw, 0.f));
	previousPawn->SetActorHiddenInGame(false);
	previousPawn->SetActorEnableCollision(true);
	controller->Possess(previousPawn);
	controller->SetControlRotation(FRotator(0.f, dismountYaw, 0.f));

	// 车辆本身不销毁——这次是预置在停车位的真实物件，不是一次性测试对象，下车后原地留着
	// 供下次再上车。
}
