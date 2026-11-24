#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "ForeverTestSubsystem.generated.h"

class AForeverFrameworkActor;

// 带画面UE测试驱动：命令行 -ForeverTest=boot,perf [-ForeverTestOut=<dir>] [-ForeverTestPerfSeconds=30]。
// 等World关卡里的AForeverFrameworkActor生成完，按套件依次执行，写 ue_<suite>.json/.txt，最后带失败数退出进程。
UCLASS()
class UForeverTestSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	struct FCheck { FString Name; bool bPassed; FString Detail; };
	struct FMetric { FString Name; double Value; FString Unit; };

	enum class EPhase : uint8 { WaitForWorld, RunBoot, SamplePerf, CommuteSkip, CommuteRun, CommuteDrain, TrafficRun, SaveLoadSave, SaveLoadWait, RideBoard, RideRun, TerrainShot, Finish };

	bool Tick(float DeltaTime);
	void Check(bool bPassed, const FString& Name, const FString& Detail = FString());
	void Metric(const FString& Name, double Value, const FString& Unit = FString());
	void RunBootSuite();
	void RunComponentsSuite();
	void RunChangesSuite();
	void BeginRide();
	void TickRide(float DeltaTime);
	void BeginTraffic();
	void TickTraffic(float DeltaTime);
	void BeginCommute();
	void TickCommute(float DeltaTime);
	void RunSaveLoadSave();
	void RunSaveLoadVerify();
	void BeginTerrainShot();
	void TickTerrainShot(float DeltaTime);
	void BeginPerfSample();
	void EndPerfSample();
	void WriteReport(const FString& Suite);
	void Finish();
	bool WantSuite(const FString& Name) const;
	AForeverFrameworkActor* FindFramework() const;

	FTSTicker::FDelegateHandle TickHandle;
	EPhase Phase = EPhase::WaitForWorld;
	TArray<FString> Suites;
	FString OutDir;
	double StartSeconds = 0.0;
	double WaitDeadlineSeconds = 600.0;
	double PerfSeconds = 30.0;
	double PerfStart = 0.0;
	double PerfSettleStart = 0.0;
	TArray<float> FrameTimes;
	int32 SuiteIndex = 0;
	TArray<FCheck> Checks;
	TArray<FMetric> Metrics;
	int32 TotalFailures = 0;
	TWeakObjectPtr<AForeverFrameworkActor> Framework;
	// saveload：存档前的Core计数，读档后新世界必须一致。
	TWeakObjectPtr<AForeverFrameworkActor> FrameworkBeforeLoad;
	int32 SavedCitizens = 0, SavedBuildings = 0, SavedVehicles = 0, SavedOrganizations = 0;
	FString SavedTime;
	FString SavePath;
	double LoadStartSeconds = 0.0;
	// commute：跳到早高峰前，再实时跑过通勤高峰，记录帧时间和异步寻路队列。
	double CommuteStartSeconds = 0.0;
	double CommutePhaseStart = 0.0;
	double CommuteLastLog = 0.0;
	double CommuteLastProgress = 0.0;
	float CommuteLastFraction = 0.f;
	int32 CommutePeakPending = 0;
	TArray<float> CommuteFrames;
	FString CommuteEndClock;
	// traffic：跳到早高峰后让玩家(步行)每隔几秒被传到一辆在路上跑的车旁边，反复触发载具Actor的生成/销毁/物理，持续一段时间。
	double TrafficStart = 0.0, TrafficLastHop = 0.0, TrafficSeconds = 300.0;
	int32 TrafficHops = 0, TrafficPeakVehicles = 0, TrafficCursor = 0;
	TArray<float> TrafficFrames;
	// ride：依次搭乘飞机/火车各一段时间，记录帧时间，检查玩家一直在车上。
	TArray<FString> RideCategories;
	int32 RideIndex = 0;
	double RideStart = 0.0;
	TArray<float> RideFrames;
	// terrainshot：把相机依次放到几个固定机位(山顶/山坡/平原/沙滩/河海岸)，各截一张图到OutDir(需要带画面运行)。
	struct FShotPose { FString Name; FVector Location; FRotator Rotation; };
	TArray<FShotPose> ShotPoses;
	int32 ShotIndex = 0, ShotFrames = 0;
	TWeakObjectPtr<AActor> ShotCamera;
};
