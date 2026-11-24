#include "ForeverTestSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

#include "Framework/ForeverFrameworkActor.h"
#include "UI/FadeSubsystem.h"
#include "Mod/ForeverSaveSubsystem.h"
#include "Framework/ForeverPopulaceFrameworkComponent.h"
#include "Framework/ForeverBuildingFrameworkComponent.h"
#include "Framework/ForeverTrafficFrameworkComponent.h"
#include "Element/VehicleElement.h"
#include "Element/TransitVehicleElement.h"
#include "traffic/vehicle.h"
#include "populace/scheduler.h"
#include <memory>
#include "story/change.h"
#include "story/story.h"
#include "story/script.h"
#include "populace/citizen.h"
#include "map/room.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "GameFramework/PlayerController.h"
#include "NavigationSystem.h"

#include "common/random.h"
#include "map/map.h"
#include "map/building.h"
#include "populace/populace.h"
#include "society/society.h"
#include "society/organization.h"
#include "player/player.h"
#include "traffic/traffic.h"
#include "traffic/route.h"
#include <cmath>

DEFINE_LOG_CATEGORY_STATIC(LogForeverTest, Log, All);

bool UForeverTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// 只有命令行明确要求才创建，正常游戏零开销。
	return FString(FCommandLine::Get()).Contains(TEXT("-ForeverTest="));
}

void UForeverTestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FString SuiteList = TEXT("boot");
	FParse::Value(FCommandLine::Get(), TEXT("-ForeverTest="), SuiteList, false);
	SuiteList.ParseIntoArray(Suites, TEXT(","));
	OutDir = FPaths::ProjectSavedDir() / TEXT("TestReports/ue");
	FParse::Value(FCommandLine::Get(), TEXT("-ForeverTestOut="), OutDir);
	FParse::Value(FCommandLine::Get(), TEXT("-ForeverTestPerfSeconds="), PerfSeconds);
	IFileManager::Get().MakeDirectory(*OutDir, true);
	StartSeconds = FPlatformTime::Seconds();
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UForeverTestSubsystem::Tick), 0.0f);
	UE_LOG(LogForeverTest, Log, TEXT("ForeverTest active: suites=%s out=%s"), *SuiteList, *OutDir);
}

void UForeverTestSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	Super::Deinitialize();
}

bool UForeverTestSubsystem::WantSuite(const FString& Name) const
{
	return Suites.Contains(Name) || Suites.Contains(TEXT("all"));
}

void UForeverTestSubsystem::Check(bool bPassed, const FString& Name, const FString& Detail)
{
	Checks.Add({ Name, bPassed, Detail });
	if (!bPassed) {
		TotalFailures++;
		UE_LOG(LogForeverTest, Error, TEXT("[FAIL] %s %s"), *Name, *Detail);
	}
}

void UForeverTestSubsystem::Metric(const FString& Name, double Value, const FString& Unit)
{
	Metrics.Add({ Name, Value, Unit });
}

AForeverFrameworkActor* UForeverTestSubsystem::FindFramework() const
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World) return nullptr;
	for (TActorIterator<AForeverFrameworkActor> It(World); It; ++It) return *It;
	return nullptr;
}

bool UForeverTestSubsystem::Tick(float DeltaTime)
{
	const double Elapsed = FPlatformTime::Seconds() - StartSeconds;
	switch (Phase) {
	case EPhase::WaitForWorld: {
		AForeverFrameworkActor* Actor = FindFramework();
		if (Actor && Actor->IsInitialGenerationDone()) {
			Framework = Actor;
			Metric(TEXT("boot.seconds"), Elapsed, TEXT("s"));
			Phase = EPhase::RunBoot;
		}
		else if (Elapsed > WaitDeadlineSeconds) {
			Check(false, TEXT("world generated within the timeout"), FString::Printf(TEXT("%.0fs; is the level /Game/Levels/World?"), Elapsed));
			WriteReport(TEXT("boot"));
			Finish();
		}
		break;
	}
	case EPhase::RunBoot:
		if (WantSuite(TEXT("boot"))) {
			RunBootSuite();
			WriteReport(TEXT("boot"));
		}
		if (WantSuite(TEXT("components"))) {
			RunComponentsSuite();
			WriteReport(TEXT("components"));
		}
		if (WantSuite(TEXT("changes"))) {
			RunChangesSuite();
			WriteReport(TEXT("changes"));
		}
		if (WantSuite(TEXT("perform"))) {
			BeginPerfSample();
			Phase = EPhase::SamplePerf;
		}
		else if (WantSuite(TEXT("commute")) || WantSuite(TEXT("traffic"))) {
			BeginCommute();
			Phase = EPhase::CommuteSkip;
		}
		else if (WantSuite(TEXT("ride"))) {
			BeginRide();
		}
		else if (WantSuite(TEXT("saveload"))) {
			Phase = EPhase::SaveLoadSave;
		}
		else if (WantSuite(TEXT("terrainshot"))) {
			BeginTerrainShot();
		}
		else {
			Phase = EPhase::Finish;
		}
		break;
	case EPhase::TerrainShot:
		TickTerrainShot(DeltaTime);
		break;
	case EPhase::RideBoard:
	case EPhase::RideRun:
		TickRide(DeltaTime);
		break;
	case EPhase::CommuteSkip:
	case EPhase::CommuteRun:
	case EPhase::CommuteDrain:
		TickCommute(DeltaTime);
		break;
	case EPhase::TrafficRun:
		TickTraffic(DeltaTime);
		break;
	case EPhase::SaveLoadSave:
		RunSaveLoadSave();
		break;
	case EPhase::SaveLoadWait: {
		// 读档会重开World关卡：旧的框架Actor销毁，新的生成完成后才能校验。
		AForeverFrameworkActor* Actor = FindFramework();
		if (Actor && Actor != FrameworkBeforeLoad.Get() && Actor->IsInitialGenerationDone()) {
			Framework = Actor;
			RunSaveLoadVerify();
			WriteReport(TEXT("saveload"));
			Phase = EPhase::Finish;
		}
		else if (FPlatformTime::Seconds() - LoadStartSeconds > WaitDeadlineSeconds) {
			Check(false, TEXT("saveload: world restored within the timeout"));
			WriteReport(TEXT("saveload"));
			Phase = EPhase::Finish;
		}
		break;
	}
	case EPhase::SamplePerf:
		// 先等建筑LOD收敛(最多60秒)：开局生成/LOD切换的卡顿不算空闲基线。
		if (Framework.IsValid() && Framework->GetBuildingFramework() && !Framework->GetBuildingFramework()->IsLodSettled()
			&& FPlatformTime::Seconds() - PerfSettleStart < 60.0) {
			PerfStart = FPlatformTime::Seconds();
			break;
		}
		FrameTimes.Add(DeltaTime);
		if (FPlatformTime::Seconds() - PerfStart >= PerfSeconds) {
			EndPerfSample();
			WriteReport(TEXT("perform"));
			if (WantSuite(TEXT("commute")) || WantSuite(TEXT("traffic"))) { BeginCommute(); Phase = EPhase::CommuteSkip; }
			else if (WantSuite(TEXT("ride"))) BeginRide();
			else Phase = WantSuite(TEXT("saveload")) ? EPhase::SaveLoadSave : EPhase::Finish;
		}
		break;
	case EPhase::Finish:
		Finish();
		break;
	}
	return true;
}

void UForeverTestSubsystem::RunBootSuite()
{
	AForeverFrameworkActor* Actor = Framework.Get();
	Check(Actor != nullptr, TEXT("framework actor exists"));
	if (!Actor) return;

	Check(Actor->GetMap() && Actor->GetPopulace() && Actor->GetSociety() && Actor->GetTraffic(), TEXT("all Core domains are generated"));
	if (Actor->GetMap()) {
		const int32 Buildings = static_cast<int32>(Actor->GetMap()->GetBuildings().size());
		Metric(TEXT("core.buildings"), Buildings);
		Check(Buildings > 0, TEXT("core has buildings"));
	}
	if (Actor->GetPopulace()) {
		Metric(TEXT("core.citizens"), static_cast<double>(Actor->GetPopulace()->GetCitizens().size()));
		Check(!Actor->GetPopulace()->GetCitizens().empty(), TEXT("core has citizens"));
	}
	if (Actor->GetSociety()) Check(Actor->GetSociety()->GetLedger().Total() == 0, TEXT("ledger total is zero after boot"));
	if (Actor->GetTraffic()) Metric(TEXT("core.vehicles"), static_cast<double>(Actor->GetTraffic()->GetVehicles().size()));

	// 线路站间规划路径(含公交站辅助边)的点必须有限、在范围内、相邻点不离谱：坏点会让 BuildPath 细分上亿段卡死(见Core world套件同名检查)。
	if (Actor->GetTraffic() && Actor->GetMap()) {
		int32 Legs = 0, Bad = 0;
		FString First;
		for (const auto& [RouteName, RoutePtr] : Actor->GetTraffic()->GetRoutes()) {
			if (!RoutePtr) continue;
			for (size_t Line = 0; Line < RoutePtr->GetLines().size(); Line++) {
				const std::vector<int> Ids = RoutePtr->GetLineStopNodeIds(Line);
				for (size_t I = 0; I < Ids.size() && Ids.size() >= 2; I++) {
					const int From = Ids[I], To = Ids[(I + 1) % Ids.size()];
					if (From == To) continue;
					Legs++;
					const Map::VehicleRoutePlan Plan = Actor->GetMap()->PlanVehicleLeg(From, To);
					if (Plan.status != Map::VehicleRoutePlan::OK) continue;
					bool bOk = true;
					for (size_t K = 0; K < Plan.points.size(); K++) {
						const auto& P = Plan.points[K];
						if (!std::isfinite(P.x) || !std::isfinite(P.y) || std::fabs(P.x) > 1.0e6f || std::fabs(P.y) > 1.0e6f) bOk = false;
						if (K > 0 && !(std::hypot(P.x - Plan.points[K - 1].x, P.y - Plan.points[K - 1].y) < 5000.f)) bOk = false;
					}
					if (!bOk) { Bad++; if (First.IsEmpty()) First = FString::Printf(TEXT("%s line %d %d->%d (%d points)"), UTF8_TO_TCHAR(RouteName.c_str()), static_cast<int32>(Line), From, To, static_cast<int32>(Plan.points.size())); }
				}
			}
		}
		Metric(TEXT("core.route_legs"), Legs);
		Check(Bad == 0, TEXT("every route leg plans to finite, contiguous points"), FString::Printf(TEXT("%d of %d legs, first: %s"), Bad, Legs, *First));
	}

	// 没包随机作用域的抽取必须为0(行为不固定的信号)。
	Check(AForeverFrameworkActor::GetUnscopedRandomDraws() == 0, TEXT("no GetRandom outside a random scope"),
		FString::Printf(TEXT("%llu draws"), static_cast<unsigned long long>(AForeverFrameworkActor::GetUnscopedRandomDraws())));

	// 场景里各类Actor数量(按类名分组，写进指标便于看回归)。
	TMap<FString, int32> ByClass;
	UWorld* World = Actor->GetWorld();
	for (TActorIterator<AActor> It(World); It; ++It) ByClass.FindOrAdd(It->GetClass()->GetName())++;
	for (const TPair<FString, int32>& Pair : ByClass) Metric(TEXT("actors.") + Pair.Key, Pair.Value);
	Metric(TEXT("actors.total"), static_cast<double>(World->GetActorCount()));

	const FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
	Metric(TEXT("memory.used_physical"), static_cast<double>(Stats.UsedPhysical) / (1024.0 * 1024.0), TEXT("MB"));
}

void UForeverTestSubsystem::BeginPerfSample()
{
	FrameTimes.Reset();
	PerfStart = FPlatformTime::Seconds();
	PerfSettleStart = PerfStart;
	UE_LOG(LogForeverTest, Log, TEXT("perf sampling %.0fs of idle frames"), PerfSeconds);
}

void UForeverTestSubsystem::EndPerfSample()
{
	Check(FrameTimes.Num() > 10, TEXT("perf: collected frames"), FString::FromInt(FrameTimes.Num()));
	if (FrameTimes.Num() == 0) return;
	TArray<float> Sorted = FrameTimes;
	Sorted.Sort();
	double Sum = 0;
	int32 Over33 = 0, Over100 = 0;
	for (float T : Sorted) { Sum += T; if (T > 0.033f) Over33++; if (T > 0.1f) Over100++; }
	auto Percentile = [&](double P) { return static_cast<double>(Sorted[FMath::Min(Sorted.Num() - 1, static_cast<int32>(Sorted.Num() * P))]) * 1000.0; };
	Metric(TEXT("perf.idle.frames"), Sorted.Num());
	Metric(TEXT("perf.idle.avg"), Sum / Sorted.Num() * 1000.0, TEXT("ms"));
	Metric(TEXT("perf.idle.p50"), Percentile(0.50), TEXT("ms"));
	Metric(TEXT("perf.idle.p95"), Percentile(0.95), TEXT("ms"));
	Metric(TEXT("perf.idle.p99"), Percentile(0.99), TEXT("ms"));
	Metric(TEXT("perf.idle.max"), static_cast<double>(Sorted.Last()) * 1000.0, TEXT("ms"));
	Metric(TEXT("perf.idle.over33ms"), Over33);
	Metric(TEXT("perf.idle.over100ms"), Over100);
	// 站着不动时不该有100ms以上的卡顿(首版阈值，基线稳定后收紧)。
	Check(Over100 == 0, TEXT("perf: no idle frame over 100ms"), FString::Printf(TEXT("%d frames"), Over100));
}

void UForeverTestSubsystem::WriteReport(const FString& Suite)
{
	FString Json = FString::Printf(TEXT("{\n  \"suite\": \"ue_%s\",\n  \"passed\": %d,\n  \"failed\": %d,\n  \"checks\": [\n"), *Suite,
		Checks.Num() - TotalFailures, TotalFailures);
	for (int32 i = 0; i < Checks.Num(); i++) {
		Json += FString::Printf(TEXT("    {\"name\": \"%s\", \"passed\": %s, \"detail\": \"%s\"}%s\n"), *Checks[i].Name.ReplaceCharWithEscapedChar(),
			Checks[i].bPassed ? TEXT("true") : TEXT("false"), *Checks[i].Detail.ReplaceCharWithEscapedChar(), i + 1 < Checks.Num() ? TEXT(",") : TEXT(""));
	}
	Json += TEXT("  ],\n  \"metrics\": [\n");
	for (int32 i = 0; i < Metrics.Num(); i++) {
		Json += FString::Printf(TEXT("    {\"name\": \"%s\", \"value\": %f, \"unit\": \"%s\"}%s\n"), *Metrics[i].Name, Metrics[i].Value, *Metrics[i].Unit,
			i + 1 < Metrics.Num() ? TEXT(",") : TEXT(""));
	}
	Json += TEXT("  ]\n}\n");
	FFileHelper::SaveStringToFile(Json, *(OutDir / FString::Printf(TEXT("ue_%s.json"), *Suite)), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void UForeverTestSubsystem::Finish()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	UE_LOG(LogForeverTest, Log, TEXT("ForeverTest finished: %d checks, %d failed"), Checks.Num(), TotalFailures);
	FPlatformMisc::RequestExitWithStatus(true, TotalFailures > 0 ? 1 : 0);
}

#include "ForeverTestSuites.inl"
