#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForeverFrameworkActor.generated.h"

class USceneComponent;
class UForeverAssetFrameworkComponent;
class UForeverBuildingFrameworkComponent;
class UForeverGlobalFrameworkComponent;
class UForeverPopulaceFrameworkComponent;
class UForeverRoadnetFrameworkComponent;
class UForeverRoomFrameworkComponent;
class UForeverStoryFrameworkComponent;
class UForeverTerrainFrameworkComponent;
class UForeverTrafficFrameworkComponent;
class UForeverZoneFrameworkComponent;

// 阶段2:场景里唯一的Framework入口Actor,取代旧工程里分散的9个Framework Actor
// (Asset/Building/Global/Populace/Roadnet/Room/Story/Terrain/Traffic/Zone)。
// 内部按域组件划分职责,详见ForeverFrameworkActor.md。
UCLASS()
class FOREVER_API AForeverFrameworkActor : public AActor
{
	GENERATED_BODY()

public:
	AForeverFrameworkActor();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<USceneComponent> sceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverAssetFrameworkComponent> assetFramework;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverBuildingFrameworkComponent> buildingFramework;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverGlobalFrameworkComponent> globalFramework;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverPopulaceFrameworkComponent> populaceFramework;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverRoadnetFrameworkComponent> roadnetFramework;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverRoomFrameworkComponent> roomFramework;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverStoryFrameworkComponent> storyFramework;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverTerrainFrameworkComponent> terrainFramework;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverTrafficFrameworkComponent> trafficFramework;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Framework")
	TObjectPtr<UForeverZoneFrameworkComponent> zoneFramework;

public:
	FORCEINLINE UForeverAssetFrameworkComponent* GetAssetFramework() const { return assetFramework; }
	FORCEINLINE UForeverBuildingFrameworkComponent* GetBuildingFramework() const { return buildingFramework; }
	FORCEINLINE UForeverGlobalFrameworkComponent* GetGlobalFramework() const { return globalFramework; }
	FORCEINLINE UForeverPopulaceFrameworkComponent* GetPopulaceFramework() const { return populaceFramework; }
	FORCEINLINE UForeverRoadnetFrameworkComponent* GetRoadnetFramework() const { return roadnetFramework; }
	FORCEINLINE UForeverRoomFrameworkComponent* GetRoomFramework() const { return roomFramework; }
	FORCEINLINE UForeverStoryFrameworkComponent* GetStoryFramework() const { return storyFramework; }
	FORCEINLINE UForeverTerrainFrameworkComponent* GetTerrainFramework() const { return terrainFramework; }
	FORCEINLINE UForeverTrafficFrameworkComponent* GetTrafficFramework() const { return trafficFramework; }
	FORCEINLINE UForeverZoneFrameworkComponent* GetZoneFramework() const { return zoneFramework; }
};
