#include "Mod/ForeverSaveSubsystem.h"

#include "Mod/ForeverDialogUtils.h"
#include "Mod/ForeverModSubsystem.h"
#include "Framework/ForeverFrameworkActor.h"

#include "map/map.h"
#include "map/map_save.h"
#include "populace/populace.h"
#include "populace/populace_save.h"
#include "society/society.h"
#include "society/society_save.h"
#include "traffic/traffic.h"
#include "traffic/traffic_save.h"
#include "story/story.h"
#include "story/script_save.h"
#include "common/json.h"
#include "Element/CitizenElement.h"
#include "Element/VehicleElement.h"
#include "Element/TransitVehicleElement.h"
#include "populace/citizen.h"
#include "traffic/vehicle.h"
#include "player/player.h"
#include "player/player_save.h"
#include "Player/ForeverCharacter.h"
#include "Player/ForeverPlayerController.h"
#include "Player/ForeverWeaponComponent.h"
#include "Framework/ForeverAssetFrameworkComponent.h"
#include "Framework/ForeverPopulaceFrameworkComponent.h"
#include "Framework/ForeverStoryFrameworkComponent.h"
#include "UI/PuzzleWidget.h"
#include "UI/ForeverFadeSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Compression.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include <cstring>

namespace {
	const TCHAR* kSaveFileFilter = TEXT("Forever Save (*.fsav)|*.fsav");
	// 外层头：magic(4) + 文件版本(u32) + 未压缩长度(u64)，后面紧跟zlib压缩体。
	constexpr int32 kHeaderSize = 4 + 4 + 8;

	FString JoinErrors(const std::vector<std::string>& errors) {
		TArray<FString> lines;
		for (const std::string& error : errors) lines.Add(UTF8_TO_TCHAR(error.c_str()));
		return FString::Join(lines, TEXT("\n"));
	}

	void WriteU32(TArray<uint8>& out, uint32 v) { for (int32 i = 0; i < 4; i++) out.Add(static_cast<uint8>(v >> (8 * i))); }
	void WriteU64(TArray<uint8>& out, uint64 v) { for (int32 i = 0; i < 8; i++) out.Add(static_cast<uint8>(v >> (8 * i))); }
	uint32 ReadU32(const uint8* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32>(p[3]) << 24); }
	uint64 ReadU64(const uint8* p) { return static_cast<uint64>(ReadU32(p)) | (static_cast<uint64>(ReadU32(p + 4)) << 32); }

	// 当前关卡里的世界(不在游戏关卡里，比如开始菜单，返回nullptr)。
	AForeverFrameworkActor* FindFramework(UWorld* world) {
		return world ? Cast<AForeverFrameworkActor>(UGameplayStatics::GetActorOfClass(world, AForeverFrameworkActor::StaticClass())) : nullptr;
	}

	SaveWorld MakeSaveWorld(const AForeverFrameworkActor* framework) {
		SaveWorld world;
		world.map = framework->GetMap();
		world.populace = framework->GetPopulace();
		world.society = framework->GetSociety();
		world.traffic = framework->GetTraffic();
		world.story = framework->GetStory();
		world.player = framework->GetPlayer();
		return world;
	}

	// UE段(JSON)：玩家当前操控的市民/车辆(开车时controlledCitizen是下车后回到的那个市民)、
	// 操控对象的位置朝向、视角朝向、激活的肩膀武器和弹匣余量。
	std::string BuildUeSection(UWorld* world) {
		JsonValue root(DATA_OBJECT);
		APlayerController* playerController = world ? UGameplayStatics::GetPlayerController(world, 0) : nullptr;
		APawn* pawn = playerController ? playerController->GetPawn() : nullptr;
		if (pawn) {
			FVector location = pawn->GetActorLocation();
			FRotator rotation = pawn->GetActorRotation();
			root["pawnX"] = static_cast<double>(location.X);
			root["pawnY"] = static_cast<double>(location.Y);
			root["pawnZ"] = static_cast<double>(location.Z);
			root["pawnYaw"] = static_cast<double>(rotation.Yaw);
			FRotator control = playerController->GetControlRotation();
			root["controlPitch"] = static_cast<double>(control.Pitch);
			root["controlYaw"] = static_cast<double>(control.Yaw);
		}
		if (AVehicleElement* vehicle = Cast<AVehicleElement>(pawn)) {
			if (vehicle->GetVehicle()) root["controlledVehicle"] = vehicle->GetVehicle()->GetName();
			pawn = vehicle->GetPreviousPawn().Get();
		}
		else if (ATransitVehicleElement* transit = Cast<ATransitVehicleElement>(pawn)) {
			// 搭乘中的线路公交：同样记载具名，读档由RestoreControlledPawn走ApplyBoardVehicle。
			if (transit->GetVehicle()) root["controlledVehicle"] = transit->GetVehicle()->GetName();
			pawn = transit->GetPreviousPawn().Get();
		}
		if (ACitizenElement* citizen = Cast<ACitizenElement>(pawn)) {
			if (citizen->GetCitizen()) root["controlledCitizen"] = citizen->GetCitizen()->GetName();
			root["weaponSlot"] = static_cast<int>(citizen->GetActiveShoulderSlot());
			UForeverWeaponComponent* weapon = citizen->GetWeaponComponent();
			root["magazineAmmo"] = static_cast<int>(weapon && weapon->HasWeapon() ? weapon->GetCurrentAmmo() : -1);
		}
		return root.WriteCompact();
	}

	// 不能存档的时刻：剧情对话进行中、谜题进行中(这些中间状态不进存档)。返回拒绝原因，可以存时返回空。
	FString CheckCanSave(AForeverFrameworkActor* framework) {
		if (framework->GetStoryFramework() && framework->GetStoryFramework()->IsDialogActive()) {
			return TEXT("对话进行中，无法保存");
		}
		AForeverPlayerController* playerController = Cast<AForeverPlayerController>(
			UGameplayStatics::GetPlayerController(framework->GetWorld(), 0));
		if (playerController && playerController->GetPuzzleWidget() && playerController->GetPuzzleWidget()->IsVisible()) {
			return TEXT("谜题进行中，无法保存");
		}
		return FString();
	}
}

bool UForeverSaveSubsystem::SaveToFile(const FString& Path, FString& OutError) {
	AForeverFrameworkActor* framework = FindFramework(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);
	if (!framework || !framework->GetMap()) {
		OutError = TEXT("当前不在游戏中，无法保存");
		return false;
	}
	OutError = CheckCanSave(framework);
	if (!OutError.IsEmpty()) return false;

	// 保存前把UE侧的状态同步回Core：等后台寻路线程停下(它们读map)，再把场景里市民Actor的位置
	// 写回Citizen；正在走路的市民按"已经走到目的地"写进存档，存完立刻还原，不影响正在进行的游戏。
	UForeverPopulaceFrameworkComponent::WaitForPendingPathfinding();
	TArray<UForeverPopulaceFrameworkComponent::FWalkingSnapshot> walking;
	if (UForeverPopulaceFrameworkComponent* populaceFramework = framework->GetPopulaceFramework()) {
		walking = populaceFramework->SyncPositionsToCore();
	}

	// 房间外的物品(被扔出去的投掷物等)：把Actor当前的位置朝向写回Player。
	if (UForeverAssetFrameworkComponent* assetFramework = framework->GetAssetFramework()) {
		assetFramework->SyncWorldAssetsToCore();
	}

	SaveArchive archive = SaveGame::BuildArchive(MakeSaveWorld(framework));
	UForeverPopulaceFrameworkComponent::RestoreAfterSave(walking);
	archive.SetJson(SAVE_SECTION_UE, 1, BuildUeSection(framework->GetWorld()));
	UE_LOG(LogTemp, Log, TEXT("ForeverSaveSubsystem: %s"), UTF8_TO_TCHAR(MapSaveCodec::GetLastReport().c_str()));
	for (const SaveSection& section : archive.GetSections()) {
		UE_LOG(LogTemp, Log, TEXT("ForeverSaveSubsystem:   section %s v%d: %llu bytes"),
			UTF8_TO_TCHAR(section.name.c_str()), section.version, static_cast<uint64>(section.data.size()));
	}
	std::vector<uint8_t> body = archive.Serialize();
	if (body.size() > static_cast<size_t>(MAX_int32)) {
		OutError = TEXT("存档体积超过2GB，无法压缩写出");
		return false;
	}

	int32 compressedBound = FCompression::CompressMemoryBound(NAME_Zlib, static_cast<int32>(body.size()));
	TArray<uint8> compressed;
	compressed.SetNumUninitialized(compressedBound);
	int32 compressedSize = compressedBound;
	if (!FCompression::CompressMemory(NAME_Zlib, compressed.GetData(), compressedSize, body.data(), static_cast<int32>(body.size()))) {
		OutError = TEXT("压缩存档失败");
		return false;
	}

	TArray<uint8> file;
	file.Reserve(kHeaderSize + compressedSize);
	file.Append(reinterpret_cast<const uint8*>(SAVE_FILE_MAGIC), 4);
	WriteU32(file, static_cast<uint32>(SAVE_FILE_VERSION));
	WriteU64(file, static_cast<uint64>(body.size()));
	file.Append(compressed.GetData(), compressedSize);

	if (!FFileHelper::SaveArrayToFile(file, *Path)) {
		OutError = FString::Printf(TEXT("无法写入存档文件：%s"), *Path);
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("ForeverSaveSubsystem: saved %s (%d bytes, %llu uncompressed)"), *Path, file.Num(), static_cast<uint64>(body.size()));
	return true;
}

bool UForeverSaveSubsystem::SaveWithDialog(FString& OutError) {
	OutError.Reset();
	FString path;
	if (!ForeverDialog::SaveFile(TEXT("保存游戏"), kSaveFileFilter, TEXT("save.fsav"), path)) return false;
	if (FPaths::GetExtension(path).IsEmpty()) path += TEXT(".fsav");
	return SaveToFile(path, OutError);
}

bool UForeverSaveSubsystem::ReadArchive(const FString& Path, SaveArchive& OutArchive, FString& OutError) const {
	TArray<uint8> file;
	if (!FFileHelper::LoadFileToArray(file, *Path)) {
		OutError = FString::Printf(TEXT("无法读取存档文件：%s"), *Path);
		return false;
	}
	if (file.Num() < kHeaderSize || std::memcmp(file.GetData(), SAVE_FILE_MAGIC, 4) != 0) {
		OutError = TEXT("这不是Forever的存档文件");
		return false;
	}
	int32 fileVersion = static_cast<int32>(ReadU32(file.GetData() + 4));
	if (fileVersion > SAVE_FILE_VERSION) {
		OutError = FString::Printf(TEXT("存档来自更新的版本（文件格式版本%d，当前最高支持%d）"), fileVersion, SAVE_FILE_VERSION);
		return false;
	}
	if (fileVersion < SAVE_FILE_MIN_VERSION) {
		OutError = FString::Printf(TEXT("存档文件格式太旧（版本%d，当前最低支持%d）"), fileVersion, SAVE_FILE_MIN_VERSION);
		return false;
	}
	uint64 uncompressedSize = ReadU64(file.GetData() + 8);
	if (uncompressedSize > static_cast<uint64>(MAX_int32)) {
		OutError = TEXT("存档已损坏（解压后长度异常）");
		return false;
	}

	std::vector<uint8_t> body(static_cast<size_t>(uncompressedSize));
	if (!FCompression::UncompressMemory(NAME_Zlib, body.data(), static_cast<int32>(uncompressedSize),
		file.GetData() + kHeaderSize, file.Num() - kHeaderSize)) {
		OutError = TEXT("存档已损坏（解压失败）");
		return false;
	}

	std::string error;
	if (!OutArchive.Parse(body.data(), body.size(), error)) {
		OutError = FString::Printf(TEXT("存档已损坏：%s"), UTF8_TO_TCHAR(error.c_str()));
		return false;
	}
	return true;
}

bool UForeverSaveSubsystem::RequestLoad(const FString& Path, FString& OutError) {
	TSharedPtr<FForeverPendingLoad> load = MakeShared<FForeverPendingLoad>();
	load->path = Path;
	if (!ReadArchive(Path, load->archive, OutError)) return false;

	// 1) 不改任何状态的校验：段版本、manifest、mod是否都有DLL提供、依赖、脚本/布局资源。
	std::vector<std::string> errors = SaveGame::ValidateArchive(load->archive, load->manifest);
	if (!errors.empty()) {
		OutError = TEXT("无法加载存档：\n") + JoinErrors(errors);
		return false;
	}

	// 2) 把存档的mod集合设成当前配置，确保DLL都已加载、mod状态版本兼容；失败就恢复原配置。
	SaveGame::ConfigSnapshot snapshot = SaveGame::ApplyManifestConfig(load->manifest);
	UGameInstance* gameInstance = GetGameInstance();
	if (gameInstance) {
		if (UForeverModSubsystem* modSubsystem = gameInstance->GetSubsystem<UForeverModSubsystem>()) {
			modSubsystem->EnsureModsRegistered();
		}
	}
	errors = SaveGame::ValidateModsRegistered(load->manifest);
	if (errors.empty()) errors = SaveGame::ValidateModStateVersions(load->manifest);
	if (!errors.empty()) {
		SaveGame::RestoreConfig(snapshot);
		OutError = TEXT("无法加载存档：\n") + JoinErrors(errors);
		return false;
	}

	// 3) 通过：登记读档请求，重新进入World关卡(菜单和游戏内加载走同一条路径)。
	pendingLoad = load;
	if (gameInstance) {
		// 先变黑再进World(OpenLevel要同步加载很久，期间画面不刷新)，见UForeverFadeSubsystem。
		if (UForeverFadeSubsystem* fade = gameInstance->GetSubsystem<UForeverFadeSubsystem>()) {
			fade->OpenWorldLevel();
		}
		else {
			UGameplayStatics::OpenLevel(gameInstance, FName(TEXT("/Game/Levels/World.World")));
		}
	}
	return true;
}

bool UForeverSaveSubsystem::LoadWithDialog(FString& OutError) {
	OutError.Reset();
	FString path;
	if (!ForeverDialog::OpenFile(TEXT("加载游戏"), kSaveFileFilter, path)) return false;
	return RequestLoad(path, OutError);
}

TSharedPtr<FForeverPendingLoad> UForeverSaveSubsystem::ConsumePendingLoad() {
	TSharedPtr<FForeverPendingLoad> load = pendingLoad;
	pendingLoad.Reset();
	return load;
}

// ---- 测试用控制台命令(暂停界面/菜单按钮在P1接入)：Forever.Save [路径] / Forever.Load [路径]，不给路径就弹对话框 ----

namespace {
	UForeverSaveSubsystem* GetSaveSubsystem(UWorld* world) {
		UGameInstance* gameInstance = world ? world->GetGameInstance() : nullptr;
		return gameInstance ? gameInstance->GetSubsystem<UForeverSaveSubsystem>() : nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs GSaveCommand(
		TEXT("Forever.Save"),
		TEXT("Save the game: Forever.Save [path.fsav] (no path opens a save dialog)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world) {
			UForeverSaveSubsystem* subsystem = GetSaveSubsystem(world);
			if (!subsystem) return;
			FString error;
			bool ok = args.Num() > 0 ? subsystem->SaveToFile(args[0], error) : subsystem->SaveWithDialog(error);
			if (!ok && !error.IsEmpty()) UE_LOG(LogTemp, Error, TEXT("Forever.Save: %s"), *error);
		}));

	// 往返一致性检查：当前世界 -> 存档段 -> 读回成新的Map/Populace/Society -> 再写一次存档段，逐段
	// 比较字节。全部一致说明这几个域没有漏存的字段(不比较manifest：计数器会因为读档新建mod而变)。
	FAutoConsoleCommandWithWorldAndArgs GRoundTripCommand(
		TEXT("Forever.SaveRoundTrip"),
		TEXT("Check that saving and loading the map loses nothing: save -> load -> save, then compare the map sections"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world) {
			AForeverFrameworkActor* framework = FindFramework(world);
			if (!framework || !framework->GetMap()) {
				UE_LOG(LogTemp, Error, TEXT("Forever.SaveRoundTrip: not in a game level"));
				return;
			}
			SaveWorld current = MakeSaveWorld(framework);
			SaveArchive first = SaveGame::BuildArchive(current);

			std::string error;
			Map* restoredMap = MapSaveCodec::Load(first, error);
			if (!restoredMap) {
				UE_LOG(LogTemp, Error, TEXT("Forever.SaveRoundTrip: map load failed: %s"), UTF8_TO_TCHAR(error.c_str()));
				return;
			}
			// 交通读档时站点会按需建旁路节点，让宿主的Node::count继续往上涨；这些节点不进地图段，
			// 但Node::count写在路网段开头。比较前先把计数器设回刚读完地图时的值，比完再还原。
			int nodeCountAfterMapLoad = Node::GetCount();
			std::vector<std::pair<JobExperience*, int>> pendingOrganizations;
			Populace* restoredPopulace = current.populace ? PopulaceSaveCodec::Load(first, *restoredMap, pendingOrganizations, error) : nullptr;
			if (current.populace && !restoredPopulace) {
				UE_LOG(LogTemp, Error, TEXT("Forever.SaveRoundTrip: populace load failed: %s"), UTF8_TO_TCHAR(error.c_str()));
			}
			Society* restoredSociety = (current.society && restoredPopulace)
				? SocietySaveCodec::Load(first, *restoredMap, *restoredPopulace, pendingOrganizations, error) : nullptr;
			if (current.society && restoredPopulace && !restoredSociety) {
				UE_LOG(LogTemp, Error, TEXT("Forever.SaveRoundTrip: society load failed: %s"), UTF8_TO_TCHAR(error.c_str()));
			}

			Traffic* restoredTraffic = current.traffic ? TrafficSaveCodec::Load(first, *restoredMap, true, error) : nullptr;
			if (current.traffic && !restoredTraffic) {
				UE_LOG(LogTemp, Error, TEXT("Forever.SaveRoundTrip: traffic load failed: %s"), UTF8_TO_TCHAR(error.c_str()));
			}
			Player* restoredPlayer = current.player ? PlayerSaveCodec::Load(first, *restoredMap, error) : nullptr;
			if (current.player && !restoredPlayer) {
				UE_LOG(LogTemp, Error, TEXT("Forever.SaveRoundTrip: player load failed: %s"), UTF8_TO_TCHAR(error.c_str()));
			}
			Story* restoredStory = nullptr;
			if (current.story) {
				restoredStory = new Story();
				restoredStory->Init();
			}
			if (!ScriptSaveCodec::Load(first, restoredMap, restoredPopulace, restoredSociety, restoredTraffic, restoredStory, error) && !error.empty()) {
				UE_LOG(LogTemp, Error, TEXT("Forever.SaveRoundTrip: script load failed: %s"), UTF8_TO_TCHAR(error.c_str()));
			}

			SaveWorld restoredWorld;
			restoredWorld.map = restoredMap;
			restoredWorld.populace = restoredPopulace;
			restoredWorld.society = restoredSociety;
			restoredWorld.traffic = restoredTraffic;
			restoredWorld.story = restoredStory;
			restoredWorld.player = restoredPlayer;
			int liveNodeCount = Node::GetCount();
			Node::SetCount(nodeCountAfterMapLoad);
			SaveArchive second = SaveGame::BuildArchive(restoredWorld);
			Node::SetCount(liveNodeCount);
			// 先删引用Map(房间/组件/道路)的域，最后删Map。
			delete restoredPlayer;
			delete restoredStory;
			delete restoredTraffic;
			delete restoredSociety;
			delete restoredPopulace;
			delete restoredMap;

			bool allMatch = true;
			for (const SaveSection& section : first.GetSections()) {
				if (section.name == SAVE_SECTION_MANIFEST) continue;
				const SaveSection* other = second.Find(section.name);
				size_t firstDiff = 0;
				bool match = other && other->data.size() == section.data.size();
				if (other) {
					size_t n = FMath::Min(other->data.size(), section.data.size());
					while (firstDiff < n && other->data[firstDiff] == section.data[firstDiff]) firstDiff++;
					match = match && firstDiff == section.data.size();
				}
				allMatch = allMatch && match;
				UE_LOG(LogTemp, Log, TEXT("Forever.SaveRoundTrip: %s %s (%llu bytes%s)"),
					UTF8_TO_TCHAR(section.name.c_str()), match ? TEXT("OK") : TEXT("MISMATCH"),
					static_cast<uint64>(section.data.size()),
					match ? TEXT("") : *FString::Printf(TEXT(", first difference at byte %llu"), static_cast<uint64>(firstDiff)));
			}
			FString summary = allMatch ? TEXT("Forever.SaveRoundTrip: all sections match") : TEXT("Forever.SaveRoundTrip: MISMATCH, see log");
			UE_LOG(LogTemp, Log, TEXT("%s"), *summary);
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, allMatch ? FColor::Green : FColor::Red, summary);
		}));

	FAutoConsoleCommandWithWorldAndArgs GLoadCommand(
		TEXT("Forever.Load"),
		TEXT("Load a save: Forever.Load [path.fsav] (no path opens a file dialog)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world) {
			UForeverSaveSubsystem* subsystem = GetSaveSubsystem(world);
			if (!subsystem) return;
			FString error;
			bool ok = args.Num() > 0 ? subsystem->RequestLoad(args[0], error) : subsystem->LoadWithDialog(error);
			if (!ok && !error.IsEmpty()) UE_LOG(LogTemp, Error, TEXT("Forever.Load: %s"), *error);
		}));
}
