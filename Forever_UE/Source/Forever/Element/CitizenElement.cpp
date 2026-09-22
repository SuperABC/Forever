#include "Element/CitizenElement.h"

#include "Framework/ForeverPopulaceFrameworkComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

#include "populace/citizen.h"
#include "populace/experience.h"
#include "populace/school.h"
#include "society/organization.h"
#include "map/building.h"
#include "map/room.h"
#include "common/utility.h"

// 地图单位换算——和BuildingElement.cpp的BUILDING_WORLD_SCALE同一个值，这次按本文件既有
// 约定各自维护一份，不额外抽公共头(这个约定详见BuildingElement.cpp顶部注释)。
#define CITIZEN_WORLD_SCALE 1000.f
// 楼层Z起算的"地坪"偏移(UE单位)，和BUILDING_HEIGHT_EPSILON同一个值/同一个理由。
#define CITIZEN_HEIGHT_EPSILON 10.f
// 地板slab厚度(UE单位)——照抄BuildingElement.cpp里kSlabThickness=0.02f(地图单位)*
// BUILDING_WORLD_SCALE。BuildingElement.cpp把Ground slab的中心摆在
// "floorBaseZ对应的楼层底部+半个slab厚度"，也就是说楼层真正能站人的地板表面(slab顶面)
// 比floorBaseZ换算出来的"楼层底部"还要再高一个完整slab厚度——citizen的落脚点必须按这个
// 顶面算，不能直接用floorBaseZ换算的楼层底部，否则会陷进地板slab里(实测发现的bug：陷入
// 深度正好是这个厚度量级)。
#define CITIZEN_GROUND_SLAB_THICKNESS 20.f
// 靠近检测碰撞盒尺寸(UE单位，不走地图单位换算——这个盒子纯粹是UE层的装饰性判定，和
// Core侧map数据无关)：水平半宽~2.5m，垂直半高~1m。
#define CITIZEN_PROXIMITY_HALF_XY 250.f
#define CITIZEN_PROXIMITY_HALF_Z 100.f

using namespace std;

namespace {
	// 把Floor局部坐标(原点在楼体左下角，未旋转)转成世界坐标——和BuildingElement.cpp里
	// ComputeWorldPosition同一个公式，这次按本文件既有约定各自维护一份。**函数名不能叫
	// 同一个名字**：UBT的unity build会把这个模块(Forever)一批.cpp文件拼进同一个生成的
	// Module.Forever.cpp里编译，原本"各自的匿名namespace互不可见"的隔离性在这种情况下
	// 失效(两份匿名namespace被塞进了同一个真正的翻译单元)，两个同名同签名的函数会直接
	// 报重定义错误(实测踩过：main分支代码不变、只是Git切了一次分支导致UBT自适应unity
	// 排除名单重算，BuildingElement.cpp和CitizenElement.cpp第一次被一起打进同一个unity
	// blob就编译失败了)——本文件的版本改叫`ComputeCitizenWorldPosition`避开这个问题，
	// 其余每个文件仍然可以有自己的私有helper，只要函数名在整个Forever模块内不重复即可。
	void ComputeCitizenWorldPosition(const Building& building, float localX, float localY,
		float& outWorldX, float& outWorldY) {
		float relX = localX - building.GetBodySizeX() * 0.5f + building.GetBodyOffsetX();
		float relY = localY - building.GetBodySizeY() * 0.5f + building.GetBodyOffsetY();
		float rot = building.GetRotation();
		float c = FMath::Cos(rot), s = FMath::Sin(rot);
		outWorldX = (building.GetPosX() + relX * c - relY * s) * CITIZEN_WORLD_SCALE;
		outWorldY = (building.GetPosY() + relX * s + relY * c) * CITIZEN_WORLD_SCALE;
	}

	// LogNearbyRelationships（T键）打印用的几个类型->可读文本转换，函数名加Citizen前缀
	// 避免unity build下和其它.cpp里的同名helper撞名（本文件顶部ComputeCitizenWorldPosition
	// 的注释已经解释过这个坑）。
	FString CitizenRelativeTypeToString(RELATIVE_TYPE type) {
		switch (type) {
		case RELATIVE_SPOUSE: return TEXT("配偶");
		case RELATIVE_PARENT: return TEXT("父母");
		case RELATIVE_CHILD: return TEXT("子女");
		case RELATIVE_SIBLING: return TEXT("兄弟姐妹");
		default: return TEXT("未知");
		}
	}

	FString CitizenRelationshipCategoryToString(RELATIONSHIP_CATEGORY category) {
		switch (category) {
		case RELATIONSHIP_KINSHIP: return TEXT("亲属");
		case RELATIONSHIP_CLASSMATE: return TEXT("同学");
		case RELATIONSHIP_COLLEAGUE: return TEXT("同事");
		case RELATIONSHIP_ROMANTIC: return TEXT("情感");
		default: return TEXT("未知");
		}
	}

	FString CitizenEducationLevelToString(EDUCATION_LEVEL level) {
		switch (level) {
		case EDUCATION_ELEMENTARY: return TEXT("小学");
		case EDUCATION_MIDDLE: return TEXT("中学");
		case EDUCATION_UNIVERSITY: return TEXT("大学");
		default: return TEXT("未知");
		}
	}

	// 按Experience::GetCategory()分派到对应派生类，拼出一行可读描述——见experience.h
	// "一对一"/"一对多"两种派生类的区别：同学/同事这两类不指向具体某个人，只能打印班级/
	// 组织信息，不像亲属/情感那样能打印对方姓名。
	FString CitizenExperienceToString(Experience* experience) {
		if (!experience) return TEXT("(空)");
		FString range = experience->IsOngoing()
			? FString::Printf(TEXT("%d年至今"), experience->GetBeginYear())
			: FString::Printf(TEXT("%d-%d年"), experience->GetBeginYear(), experience->GetEndYear());

		switch (experience->GetCategory()) {
		case RELATIONSHIP_KINSHIP: {
			KinshipExperience* kinship = static_cast<KinshipExperience*>(experience);
			Citizen* other = kinship->GetOther();
			return FString::Printf(TEXT("[亲属] %s(%s) %s"),
				*CitizenRelativeTypeToString(kinship->GetRelativeType()),
				other ? UTF8_TO_TCHAR(other->GetName().c_str()) : TEXT("?"), *range);
		}
		case RELATIONSHIP_ROMANTIC: {
			EmotionExperience* emotion = static_cast<EmotionExperience*>(experience);
			Citizen* other = emotion->GetOther();
			return FString::Printf(TEXT("[情感] %s %s"),
				other ? UTF8_TO_TCHAR(other->GetName().c_str()) : TEXT("?"), *range);
		}
		case RELATIONSHIP_CLASSMATE: {
			EducationExperience* education = static_cast<EducationExperience*>(experience);
			SchoolClass* schoolClass = education->GetSchoolClass();
			return FString::Printf(TEXT("[同学] %s(%s) %s"),
				schoolClass ? UTF8_TO_TCHAR(schoolClass->GetSchoolName().c_str()) : TEXT("?"),
				schoolClass ? *CitizenEducationLevelToString(schoolClass->GetLevel()) : TEXT("?"), *range);
		}
		case RELATIONSHIP_COLLEAGUE: {
			JobExperience* job = static_cast<JobExperience*>(experience);
			Organization* organization = job->GetOrganization();
			return FString::Printf(TEXT("[同事] %s %s"),
				organization ? UTF8_TO_TCHAR(organization->GetType().c_str()) : TEXT("?"), *range);
		}
		default:
			return TEXT("[未知类型]");
		}
	}

	// 把一个citizen的acquaintances(熟人关系强度)+experiences(四类人际关系历史记录)完整
	// 打印到log，index只用来在多行输出里标记"这是附近名单里第几个人"，方便肉眼分组阅读。
	void LogCitizenRelationshipData(Citizen* citizen, int32 index) {
		if (!citizen) return;
		UE_LOG(LogTemp, Log, TEXT("[T][%d] ========== %s =========="),
			index, UTF8_TO_TCHAR(citizen->GetName().c_str()));

		const auto& acquaintances = citizen->GetAcquaintances();
		UE_LOG(LogTemp, Log, TEXT("[T][%d] Acquaintances(%d):"), index, static_cast<int32>(acquaintances.size()));
		for (const auto& [name, relation] : acquaintances) {
			UE_LOG(LogTemp, Log, TEXT("[T][%d]   %s [%s]: 熟悉=%.2f 尊敬=%.2f 好感=%.2f 信任=%.2f 竞争=%.2f 依赖=%.2f"),
				index, UTF8_TO_TCHAR(name.c_str()), *CitizenRelationshipCategoryToString(relation.category),
				relation[RELATION_FAMILIARITY], relation[RELATION_RESPECT], relation[RELATION_FAVOUR],
				relation[RELATION_TRUST], relation[RELATION_COMPETING], relation[RELATION_RELIABILITY]);
		}

		const auto& experiences = citizen->GetExperiences();
		UE_LOG(LogTemp, Log, TEXT("[T][%d] Experiences(%d):"), index, static_cast<int32>(experiences.size()));
		for (Experience* experience : experiences) {
			UE_LOG(LogTemp, Log, TEXT("[T][%d]   %s"), index, *CitizenExperienceToString(experience));
		}
	}
}

// 附近市民名单：静态成员定义。
TArray<TWeakObjectPtr<ACitizenElement>> ACitizenElement::nearbyCitizens;

ACitizenElement::ACitizenElement() {
	// bCanEverTick=true+默认关闭Tick(SetActorTickEnabled(false))：绝大多数citizen静止
	// 不动，不需要每帧开销；只有WalkTo()正在带它走路的这段时间才打开Tick，走完立刻关掉，
	// 见WalkTo/Tick。
	PrimaryActorTick.bCanEverTick = true;
	SetActorTickEnabled(false);

	// mesh/anim/摄像机/移动参数/Enhanced Input绑定这些全部由基类AForeverCharacter的构造
	// 函数负责（同一份占位mesh，见ForeverCharacter.cpp），这里不用重复设置。这次没有真正的
	// AI移动逻辑——显式禁用移动模式，避免CharacterMovementComponent自己的重力/地面检测把
	// citizen从Init()摆好的位置上挪走，只有被玩家占有、或者被WalkTo带着走路时才切换成
	// MOVE_Walking，见PossessedBy/WalkTo。
	GetCharacterMovement()->SetMovementMode(MOVE_None);

	// 关键：这个项目没有AIController，WalkTo带着走路期间这个citizen的Pawn::Controller
	// 必然是nullptr(按定义——正被玩家占有的citizen不会触发调度自己上下班，见RequestWalk
	// 的possessed跳过分支)。UCharacterMovementComponent::TickComponent默认只有
	// Controller非空时才会调用PerformMovement()把AddMovementInput积累的输入转成真正的
	// Velocity——Controller为空时PerformMovement()整个不会跑，AddMovementInput因此是纯
	// 空操作，WalkTo每帧调用了也不会有任何效果(实测复现：MovementMode一直是MOVE_Walking，
	// AddMovementInput每帧都在调用，但GetVelocity()和GetActorLocation()几十帧下来纹丝
	// 不动，PIE日志确认过)。必须显式打开这个开关，让PerformMovement()在没有Controller时
	// 也照常跑。
	GetCharacterMovement()->bRunPhysicsWithNoController = true;
}

void ACitizenElement::PossessedBy(AController* NewController) {
	Super::PossessedBy(NewController); // AForeverCharacter::PossessedBy：增删Input Mapping Context
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	// 被占有期间直接禁掉自己的proximityBox——这个box本来就是"检测玩家有没有靠近我"，
	// 自己正好就是玩家的时候，box和自己的capsule必然贴在一起常驻重叠，会不停产生自己的
	// Overlap事件。但disable只能防止"以后"的自我Overlap——如果在被占有之前，自己就已经
	// 作为"别人身边的市民"被加进过nearbyCitizens(比如市民A靠近市民B时，B的proximityBox把
	// B自己加进了名单；A随后按T切换到B，B变成新的pawn，但名单里"B"这一条早就在disable
	// 生效之前就已经存在了，disable不会回头清掉它)，这一条陈旧的"自己"记录就会一直卡在
	// 名单里，永远排在最前面，导致下一次按T又切回"自己"、名单卡死——这正是实测复现的
	// "切到第二个市民后再也切不出去"的bug。所以这里必须显式地把自己从名单里摘出去，
	// disable只是防止之后再把自己加回来。
	if (proximityBox) proximityBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	nearbyCitizens.RemoveSingle(TWeakObjectPtr<ACitizenElement>(this));
}

void ACitizenElement::UnPossessed() {
	GetCharacterMovement()->SetMovementMode(MOVE_None);
	if (proximityBox) proximityBox->SetCollisionProfileName(TEXT("Trigger")); // 恢复Trigger预设(QueryOnly+各通道Overlap)
	Super::UnPossessed(); // AForeverCharacter::UnPossessed：增删Input Mapping Context
}

namespace {
	// 到达路径点的判定阈值(UE单位)——只判水平距离，Z由CharacterMovement自己的地面
	// 检测/重力处理，不需要精确匹配路径点的高度。
	constexpr float kWaypointArrivalThresholdUU = 80.f;
}

void ACitizenElement::WalkTo(const TArray<FVector>& waypoints, Room* destination) {
	if (waypoints.Num() == 0) return;
	pendingWaypoints = waypoints;
	waypointIndex = 0;
	walkDestination = destination;
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	SetActorTickEnabled(true);
}

void ACitizenElement::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	if (waypointIndex >= pendingWaypoints.Num()) {
		SetActorTickEnabled(false);
		return;
	}

	FVector target = pendingWaypoints[waypointIndex];
	FVector location = GetActorLocation();
	FVector toTarget = target - location;
	toTarget.Z = 0.f;

	if (toTarget.SizeSquared() <= kWaypointArrivalThresholdUU * kWaypointArrivalThresholdUU) {
		waypointIndex++;
		if (waypointIndex >= pendingWaypoints.Num()) {
			GetCharacterMovement()->SetMovementMode(MOVE_None);
			SetActorTickEnabled(false);
			Room* arrived = walkDestination;
			walkDestination = nullptr;
			pendingWaypoints.Reset();
			if (framework.IsValid() && citizen) {
				framework->NotifyArrived(citizen, arrived);
			}
		}
		return;
	}

	AddMovementInput(toTarget.GetSafeNormal(), 1.f);
}

void ACitizenElement::LogNearbyRelationships() {
	if (nearbyCitizens.Num() == 0) {
		UE_LOG(LogTemp, Log, TEXT("[T] 附近市民名单为空"));
		return;
	}

	for (int32 i = 0; i < nearbyCitizens.Num(); i++) {
		ACitizenElement* nearby = nearbyCitizens[i].Get();
		if (!nearby || !nearby->citizen) {
			UE_LOG(LogTemp, Log, TEXT("[T][%d] (已失效)"), i);
			continue;
		}
		LogCitizenRelationshipData(nearby->citizen, i);
	}
}

void ACitizenElement::Init(Citizen* inCitizen, UForeverPopulaceFrameworkComponent* inFramework) {
	citizen = inCitizen;
	framework = inFramework;
	if (!citizen) return;

	float worldX = 0.f, worldY = 0.f, worldZ = 0.f;
	if (citizen->HasPosition()) {
		// 反之直接在记录的位置出现——不再随机抖动一次。
		float mapX, mapY, mapZ;
		citizen->GetPosition(mapX, mapY, mapZ);
		worldX = mapX * CITIZEN_WORLD_SCALE;
		worldY = mapY * CITIZEN_WORLD_SCALE;
		worldZ = mapZ * CITIZEN_WORLD_SCALE;
	}
	else {
		// 换了新房间之后从未在场景里实例化过：房间中心+随机偏移+楼层高度换算落脚点，见
		// ComputeRoomLandingSpot()。算出来立刻写回Citizen，这样"首次随机、此后复用"的
		// 记录在这一步就完成。
		ComputeRoomLandingSpot(citizen->GetCurrentRoom(), worldX, worldY, worldZ);
		citizen->SetPosition(worldX / CITIZEN_WORLD_SCALE, worldY / CITIZEN_WORLD_SCALE,
			worldZ / CITIZEN_WORLD_SCALE);
	}

	SetActorLocation(FVector(worldX, worldY, worldZ));

	BuildProximityBox();
}

void ACitizenElement::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	citizen = nullptr;
	Super::EndPlay(EndPlayReason);
}

bool ACitizenElement::ComputeRoomLandingSpot(Room* room, float& outWorldX, float& outWorldY, float& outWorldZ) {
	// building必须从room->GetParentBuilding()反查，不能用citizen->GetBuilding()——后者是
	// "家"所在的building，进入society域之后room可能是别的building(比如工作单位)的room，
	// 见ForeverPopulaceFrameworkComponent.cpp里ComputeLogicalPosition同一处修复的说明。
	Building* building = room ? room->GetParentBuilding() : nullptr;
	if (!room || !building) return false;

	// 房间中心+随机偏移(老工程原公式，抖动范围±0.2地图单位)，Z用
	// Building::GetFloorBaseZ(room->GetLayer())——比老工程"layer*楼层固定高度"的粗糙
	// 算法更准，这栋楼各层高度本来就不均匀。actor location(capsule中心)要比楼板高一个
	// capsule半高，脚底才会正好落在楼板上。
	ComputeCitizenWorldPosition(*building, room->GetPosX(), room->GetPosY(), outWorldX, outWorldY);
	outWorldX += (GetRandom(11) / 10.f - 0.5f) * 0.4f * CITIZEN_WORLD_SCALE;
	outWorldY += (GetRandom(11) / 10.f - 0.5f) * 0.4f * CITIZEN_WORLD_SCALE;
	float floorZ = CITIZEN_HEIGHT_EPSILON + building->GetFloorBaseZ(room->GetLayer()) * CITIZEN_WORLD_SCALE
		+ CITIZEN_GROUND_SLAB_THICKNESS;
	outWorldZ = floorZ + GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	return true;
}

void ACitizenElement::TeleportToRoom(Room* destination) {
	if (!citizen || !destination) return;

	float worldX, worldY, worldZ;
	if (!ComputeRoomLandingSpot(destination, worldX, worldY, worldZ)) return;

	SetActorLocation(FVector(worldX, worldY, worldZ));
	citizen->SetCurrentRoom(destination);
	citizen->SetPosition(worldX / CITIZEN_WORLD_SCALE, worldY / CITIZEN_WORLD_SCALE, worldZ / CITIZEN_WORLD_SCALE);
}

void ACitizenElement::BuildProximityBox() {
	if (!citizen) return;

	UBoxComponent* box = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
	box->SetBoxExtent(FVector(CITIZEN_PROXIMITY_HALF_XY, CITIZEN_PROXIMITY_HALF_XY, CITIZEN_PROXIMITY_HALF_Z));
	box->SetCollisionProfileName(TEXT("Trigger"));
	box->SetupAttachment(RootComponent); // RootComponent就是ACharacter自带的CapsuleComponent
	box->OnComponentBeginOverlap.AddDynamic(this, &ACitizenElement::OnOverlapBegin);
	box->OnComponentEndOverlap.AddDynamic(this, &ACitizenElement::OnOverlapEnd);
	box->RegisterComponent();

	// 只烘焙一份显示用的姓名字符串，Overlap回调绝不解引用citizen——和ABuildingElement同一套
	// 安全原则。
	collisionLabel = FString::Printf(TEXT("Citizen: %s"), UTF8_TO_TCHAR(citizen->GetName().c_str()));
	proximityBox = box;
}

void ACitizenElement::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	// OtherActor == this：这个citizen自己正好就是当前被占有的pawn，proximityBox和自己的
	// capsule天然重叠(半径250 vs capsule半径42，box完全包住capsule)，这次一定会触发一次
	// "自己进自己的box"——必须排除，否则"附近市民"名单里会常驻一个其实是自己的条目。
	if (!pawn || OtherActor != pawn || OtherActor == this) return;

	nearbyCitizens.AddUnique(TWeakObjectPtr<ACitizenElement>(this));

	if (!GEngine) return;
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, FString::Printf(TEXT("接近 %s"), *collisionLabel));
}

void ACitizenElement::OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex) {
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!pawn || OtherActor != pawn || OtherActor == this) return;

	nearbyCitizens.RemoveSingle(TWeakObjectPtr<ACitizenElement>(this));

	if (!GEngine) return;
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Yellow, FString::Printf(TEXT("离开 %s"), *collisionLabel));
}
