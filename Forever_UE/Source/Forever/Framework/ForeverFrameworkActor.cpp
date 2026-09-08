#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverAssetFrameworkComponent.h"
#include "Framework/ForeverBuildingFrameworkComponent.h"
#include "Framework/ForeverGlobalFrameworkComponent.h"
#include "Framework/ForeverPopulaceFrameworkComponent.h"
#include "Framework/ForeverRoadnetFrameworkComponent.h"
#include "Framework/ForeverRoomFrameworkComponent.h"
#include "Framework/ForeverStoryFrameworkComponent.h"
#include "Framework/ForeverTerrainFrameworkComponent.h"
#include "Framework/ForeverTrafficFrameworkComponent.h"
#include "Framework/ForeverZoneFrameworkComponent.h"
#include "Components/SceneComponent.h"

AForeverFrameworkActor::AForeverFrameworkActor()
{
	PrimaryActorTick.bCanEverTick = false;

	sceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = sceneRoot;

	assetFramework = CreateDefaultSubobject<UForeverAssetFrameworkComponent>(TEXT("AssetFramework"));
	buildingFramework = CreateDefaultSubobject<UForeverBuildingFrameworkComponent>(TEXT("BuildingFramework"));
	globalFramework = CreateDefaultSubobject<UForeverGlobalFrameworkComponent>(TEXT("GlobalFramework"));
	populaceFramework = CreateDefaultSubobject<UForeverPopulaceFrameworkComponent>(TEXT("PopulaceFramework"));
	roadnetFramework = CreateDefaultSubobject<UForeverRoadnetFrameworkComponent>(TEXT("RoadnetFramework"));
	roomFramework = CreateDefaultSubobject<UForeverRoomFrameworkComponent>(TEXT("RoomFramework"));
	storyFramework = CreateDefaultSubobject<UForeverStoryFrameworkComponent>(TEXT("StoryFramework"));
	terrainFramework = CreateDefaultSubobject<UForeverTerrainFrameworkComponent>(TEXT("TerrainFramework"));
	trafficFramework = CreateDefaultSubobject<UForeverTrafficFrameworkComponent>(TEXT("TrafficFramework"));
	zoneFramework = CreateDefaultSubobject<UForeverZoneFrameworkComponent>(TEXT("ZoneFramework"));
}

void AForeverFrameworkActor::BeginPlay()
{
	Super::BeginPlay();

	// 阶段2临时日志:骨架里的10个域组件都还是空实现,没有任何可见效果,
	// 靠这条日志在PIE的Output Log里确认Actor+组件都正常构造/初始化。
	// 阶段4填入实际系统逻辑后可以删除或改成更有用的调试信息。
	UE_LOG(LogTemp, Log, TEXT("AForeverFrameworkActor initialized with 10 framework components: Asset/Building/Global/Populace/Roadnet/Room/Story/Terrain/Traffic/Zone"));
}
