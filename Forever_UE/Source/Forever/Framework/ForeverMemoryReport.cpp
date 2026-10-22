#include "Framework/ForeverMemoryReport.h"

#include "Element/CitizenElement.h"
#include "Element/BuildingElement.h"
#include "Element/VehicleElement.h"
#include "Element/TransitVehicleElement.h"
#include "Element/AssetElement.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "UObject/UObjectArray.h"

namespace ForeverMemoryReport
{
	void Log(UWorld* world, const TCHAR* tag)
	{
		const FPlatformMemoryStats stats = FPlatformMemory::GetStats();
		const double mb = 1.0 / (1024.0 * 1024.0);
		int32 objects = GUObjectArray.GetObjectArrayNumMinusAvailable();

		int32 actors = 0, citizens = 0, buildings = 0, vehicles = 0, transits = 0, assets = 0;
		if (world) {
			for (TActorIterator<AActor> it(world); it; ++it) {
				actors++;
				AActor* actor = *it;
				if (actor->IsA<ACitizenElement>()) citizens++;
				else if (actor->IsA<ABuildingElement>()) buildings++;
				else if (actor->IsA<AVehicleElement>()) vehicles++;
				else if (actor->IsA<ATransitVehicleElement>()) transits++;
				else if (actor->IsA<AAssetElement>()) assets++;
			}
		}

		UE_LOG(LogTemp, Log, TEXT("ForeverMemory[%s]: used physical %.1f MB (peak %.1f), used virtual %.1f MB, UObjects %d, actors %d (citizens %d, buildings %d, vehicles %d, transit %d, assets %d)."),
			tag, stats.UsedPhysical * mb, stats.PeakUsedPhysical * mb, stats.UsedVirtual * mb,
			objects, actors, citizens, buildings, vehicles, transits, assets);
	}
}

static FAutoConsoleCommandWithWorld GForeverMemoryReportCommand(
	TEXT("Forever.MemoryReport"),
	TEXT("Log process memory, UObject count and Forever actor counts to Forever.log (see ForeverMemoryReport.md)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* world) {
		ForeverMemoryReport::Log(world, TEXT("console"));
	}));
