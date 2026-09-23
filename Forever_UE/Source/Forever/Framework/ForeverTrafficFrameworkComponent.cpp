#include "Framework/ForeverTrafficFrameworkComponent.h"

#include "Framework/ForeverFrameworkActor.h"
#include "Element/VehicleElement.h"
#include "Element/CitizenElement.h"
#include "traffic/traffic.h"
#include "traffic/vehicle.h"

#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

void UForeverTrafficFrameworkComponent::RequestToggleVehicle(UWorld* world, APlayerController* controller) {
	if (!world || !controller) return;

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(world, AForeverFrameworkActor::StaticClass()));
	if (framework && framework->GetTrafficFramework()) {
		framework->GetTrafficFramework()->ToggleVehicle(controller);
	}
}

void UForeverTrafficFrameworkComponent::ToggleVehicle(APlayerController* controller) {
	APawn* currentPawn = controller->GetPawn();
	if (!currentPawn) return;

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Traffic* traffic = framework ? framework->GetTraffic() : nullptr;
	if (!traffic) return;

	if (AVehicleElement* vehicleElement = Cast<AVehicleElement>(currentPawn)) {
		// 下车：用户明确要求人应该出现在车所在的地方，不是传送回上车前的原位置，所以先记下
		// 车辆当前的位置/朝向，删车之后再摆到这个位置。**只取车身的Yaw**——车翻了/侧倒之后
		// Pitch/Roll可能是任意角度，人下车应该始终站直，不应该继承车身的翻滚姿态(实测反馈：
		// 翻车后下车，市民视角被转了90度，就是这里直接照抄了车身完整旋转的锅)。
		// Z轴抬高100——车身现在是骨骼网格，Actor原点(carMesh骨骼原点)在车轮接地点附近，
		// 直接摆到这个高度会让市民的碰撞体(胶囊体中心在自己脚底往上，不是贴地)有一部分
		// 陷进地面里，实测反馈"下车直接掉到地下去"。抬高100让市民摆在车身原点上方，靠
		// 自身重力/碰撞自然落回地面，不需要精确算这个偏移量。
		FVector dismountLocation = vehicleElement->GetActorLocation() + FVector(0.f, 0.f, 100.f);
		float dismountYaw = vehicleElement->GetActorRotation().Yaw;
		FRotator dismountRotation(0.f, dismountYaw, 0.f);
		APawn* previousPawn = vehicleElement->GetPreviousPawn().Get();

		if (Vehicle* vehicle = vehicleElement->GetVehicle()) {
			traffic->DestroyVehicle(vehicle->GetName());
		}
		vehicleElement->Destroy();

		if (previousPawn) {
			// 解除上车时加的距离销毁豁免，恢复正常的流式生成/销毁管理。
			if (ACitizenElement* citizenElement = Cast<ACitizenElement>(previousPawn)) {
				citizenElement->SetDespawnExempt(false);
			}
			previousPawn->SetActorLocationAndRotation(dismountLocation, dismountRotation);
			previousPawn->SetActorHiddenInGame(false);
			previousPawn->SetActorEnableCollision(true);
			controller->Possess(previousPawn);
			// ControlRotation存在Controller身上，不随Possess切换自动清零——开车时用鼠标自由
			// 看过车身以外的角度(尤其翻车挣扎时)会残留下来，下车后市民的第三人称相机同样按
			// ControlRotation取向(AForeverCharacter::cameraBoom用bUsePawnControlRotation)，
			// 这里显式归零成只剩Yaw，避免带着开车时的视角残留。
			controller->SetControlRotation(FRotator(0.f, dismountYaw, 0.f));
		} else {
			// previousPawn已经失效(理论上不应该发生——上车时已经标记过despawn豁免，见
			// SetDespawnExempt的说明；这里只是防御性兜底，避免真出现这种情况时静默留下
			// "车没了、也没人可控"的状态却完全没有日志线索)。
			UE_LOG(LogTemp, Warning, TEXT("UForeverTrafficFrameworkComponent::ToggleVehicle: 下车时previousPawn已失效，玩家将没有可控对象"));
		}
		return;
	}

	// 上车：modId这一阶段先写死"vehicle_basic"（VehicleBasic测试车型），将来做车辆种类
	// 选择时改成参数，见traffic.h的说明。
	FString name = FString::Printf(TEXT("TestVehicle%d"), vehicleCounter++);
	Vehicle* vehicle = traffic->CreateVehicle("vehicle_basic", TCHAR_TO_UTF8(*name));
	if (!vehicle) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverTrafficFrameworkComponent::ToggleVehicle: 创建Vehicle失败,\"vehicle_basic\"是否已在config.json的vehicle_mods里启用？"));
		return;
	}

	// AVehicleElement本身只是提供驾驶/相机/输入的公共基类，具体车型的外观(骨骼网格/
	// 轮子骨骼名)由vehicle->GetBlueprintPath()指向的蓝图子类决定——这一步必须在
	// SpawnActor之前完成，因为骨骼网格必须在Actor构造函数阶段就绑定好，蓝图子类的属性
	// 覆盖早已经在蓝图保存时烘焙进了对应UClass的CDO里，这里只是选出该用哪个UClass去
	// 生成，见VehicleElement.h的类注释。
	FString blueprintPath = UTF8_TO_TCHAR(vehicle->GetBlueprintPath().data());
	UClass* vehicleClass = LoadClass<AVehicleElement>(nullptr, *blueprintPath);
	if (!vehicleClass) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverTrafficFrameworkComponent::ToggleVehicle: 加载车辆蓝图失败，路径=%s，请确认这个蓝图资产已经创建（继承AVehicleElement，设置好骨骼网格/轮子骨骼名），见VehicleElement.md"), *blueprintPath);
		traffic->DestroyVehicle(vehicle->GetName());
		return;
	}

	FVector location = currentPawn->GetActorLocation();
	FRotator rotation = currentPawn->GetActorRotation();

	// 车辆生成的位置就是当前pawn(市民/默认角色)自己所在的位置，两者的碰撞体在这一刻必然
	// 重叠——carMesh是有真实碰撞、参与物理模拟的骨骼网格(见VehicleElement.cpp构造函数)，
	// 默认的生成时碰撞检测会因为"生成点被占用"直接拒绝生成(SpawnActor返回nullptr，实测
	// 踩过这个坑："SpawnActor failed because of collision at the spawn location")，用
	// AlwaysSpawn跳过这次检测——马上就要隐藏+关掉currentPawn的碰撞了，这一刻的重叠不需要
	// 真的处理。
	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AVehicleElement* vehicleElement = GetWorld()->SpawnActor<AVehicleElement>(vehicleClass, location, rotation, spawnParams);
	if (!vehicleElement) {
		traffic->DestroyVehicle(vehicle->GetName());
		return;
	}
	vehicleElement->Init(vehicle, currentPawn);

	// 隐藏之后这个pawn会一直停在原地，但UForeverPopulaceFrameworkComponent::TickComponent
	// 的距离流式销毁只看"离当前玩家pawn多远"——玩家开车远离之后这个停在原地的市民会被判定
	// 为"走远的市民"直接Destroy掉，previousPawn变成悬空指针，下车时车没了但人也出不来、
	// 操控彻底失灵(实测踩过这个坑，见ForeverTrafficFrameworkComponent.md)。上车期间标记
	// 豁免，下车时(上面的分支)解除。
	if (ACitizenElement* citizenElement = Cast<ACitizenElement>(currentPawn)) {
		citizenElement->SetDespawnExempt(true);
	}

	currentPawn->SetActorHiddenInGame(true);
	currentPawn->SetActorEnableCollision(false);
	controller->Possess(vehicleElement);
}
