// 包含在 ForeverTestSubsystem.cpp 末尾：components 与 saveload 套件。

void UForeverTestSubsystem::RunComponentsSuite()
{
	AForeverFrameworkActor* Actor = Framework.Get();
	if (!Actor) return;
	UWorld* World = Actor->GetWorld();

	int32 BuildingActors = 0, CitizenActors = 0, DirectionalLights = 0, MovableLights = 0;
	for (TActorIterator<AActor> It(World); It; ++It) {
		const FString ClassName = It->GetClass()->GetName();
		if (ClassName == TEXT("BuildingElement")) BuildingActors++;
		else if (ClassName == TEXT("CitizenElement")) CitizenActors++;
		if (ADirectionalLight* Light = Cast<ADirectionalLight>(*It)) {
			DirectionalLights++;
			if (Light->GetLightComponent() && Light->GetLightComponent()->Mobility == EComponentMobility::Movable) MovableLights++;
		}
	}
	if (Actor->GetMap()) {
		const int32 CoreBuildings = static_cast<int32>(Actor->GetMap()->GetBuildings().size());
		Check(BuildingActors == CoreBuildings, TEXT("components: one BuildingElement per Core building"),
			FString::Printf(TEXT("%d actors vs %d buildings"), BuildingActors, CoreBuildings));
	}
	// 市民按距离流式生成：静止的玩家周围不该堆出大量Actor。
	Check(CitizenActors <= 64, TEXT("components: streamed citizen actors stay bounded"), FString::FromInt(CitizenActors));
	Check(DirectionalLights == 0 || MovableLights == DirectionalLights, TEXT("components: every directional light is Movable"),
		FString::Printf(TEXT("%d of %d"), MovableLights, DirectionalLights));
	// UE导航系统已禁用，全部寻路走Core导航图。
	Check(UNavigationSystemV1::GetCurrent(World) == nullptr, TEXT("components: UE NavigationSystem is disabled"));
	APlayerController* Controller = World->GetFirstPlayerController();
	Check(Controller != nullptr && Controller->GetPawn() != nullptr, TEXT("components: the player controller possesses a pawn"));
	Check(Actor->GetPlayer() != nullptr && Actor->GetPlayer()->GetTime() != nullptr, TEXT("components: Core player clock exists"));
	Check(Actor->GetTerrainFramework() && Actor->GetRoadnetFramework() && Actor->GetTrafficFramework() && Actor->GetPopulaceFramework()
		&& Actor->GetBuildingFramework() && Actor->GetDoorFramework() && Actor->GetStoryFramework() && Actor->GetSkyFramework(),
		TEXT("components: all framework components are present"));
}

void UForeverTestSubsystem::RunSaveLoadSave()
{
	AForeverFrameworkActor* Actor = Framework.Get();
	UGameInstance* GameInstance = GetGameInstance();
	UForeverSaveSubsystem* SaveSubsystem = GameInstance ? GameInstance->GetSubsystem<UForeverSaveSubsystem>() : nullptr;
	if (!Actor || !SaveSubsystem) {
		Check(false, TEXT("saveload: save subsystem available"));
		WriteReport(TEXT("saveload"));
		Phase = EPhase::Finish;
		return;
	}
	SavedCitizens = static_cast<int32>(Actor->GetPopulace()->GetCitizens().size());
	SavedBuildings = static_cast<int32>(Actor->GetMap()->GetBuildings().size());
	SavedVehicles = static_cast<int32>(Actor->GetTraffic()->GetVehicles().size());
	SavedOrganizations = static_cast<int32>(Actor->GetSociety()->GetOrganizations().size());
	SavedTime = UTF8_TO_TCHAR(Actor->GetPlayer()->GetTime()->ToString(true, true).c_str());

	SavePath = OutDir / TEXT("test_save.fsav");
	FString Error;
	const double SaveStart = FPlatformTime::Seconds();
	const bool bSaved = SaveSubsystem->SaveToFile(SavePath, Error);
	Metric(TEXT("saveload.save_seconds"), FPlatformTime::Seconds() - SaveStart, TEXT("s"));
	Check(bSaved, TEXT("saveload: SaveToFile succeeds"), Error);
	if (!bSaved) {
		WriteReport(TEXT("saveload"));
		Phase = EPhase::Finish;
		return;
	}
	Metric(TEXT("saveload.file_bytes"), static_cast<double>(IFileManager::Get().FileSize(*SavePath)), TEXT("B"));

	FrameworkBeforeLoad = Actor;
	LoadStartSeconds = FPlatformTime::Seconds();
	const bool bRequested = SaveSubsystem->RequestLoad(SavePath, Error);
	Check(bRequested, TEXT("saveload: RequestLoad accepts the save"), Error);
	if (!bRequested) {
		WriteReport(TEXT("saveload"));
		Phase = EPhase::Finish;
		return;
	}
	Phase = EPhase::SaveLoadWait;
}

void UForeverTestSubsystem::RunSaveLoadVerify()
{
	AForeverFrameworkActor* Actor = Framework.Get();
	Metric(TEXT("saveload.load_seconds"), FPlatformTime::Seconds() - LoadStartSeconds, TEXT("s"));
	if (!Actor || !Actor->GetPopulace() || !Actor->GetMap() || !Actor->GetTraffic() || !Actor->GetSociety()) {
		Check(false, TEXT("saveload: restored world has all Core domains"));
		return;
	}
	const int32 Citizens = static_cast<int32>(Actor->GetPopulace()->GetCitizens().size());
	Check(Citizens == SavedCitizens, TEXT("saveload: citizen count survives"), FString::Printf(TEXT("%d vs %d"), Citizens, SavedCitizens));
	Check(static_cast<int32>(Actor->GetMap()->GetBuildings().size()) == SavedBuildings, TEXT("saveload: building count survives"));
	Check(static_cast<int32>(Actor->GetTraffic()->GetVehicles().size()) == SavedVehicles, TEXT("saveload: vehicle count survives"));
	Check(static_cast<int32>(Actor->GetSociety()->GetOrganizations().size()) == SavedOrganizations, TEXT("saveload: organization count survives"));
	Check(Actor->GetSociety()->GetLedger().Total() == 0, TEXT("saveload: ledger total is zero after load"));
	const FString RestoredTime = UTF8_TO_TCHAR(Actor->GetPlayer()->GetTime()->ToString(true, true).c_str());
	Check(RestoredTime.Left(10) == SavedTime.Left(10), TEXT("saveload: game date survives"), SavedTime + TEXT(" vs ") + RestoredTime);
}

// commute：跳到早高峰前(07:50)，把时间流速调高实时跑过通勤高峰到09:00，记录帧时间和异步寻路队列，再等队列排空。
void UForeverTestSubsystem::BeginCommute()
{
	AForeverFrameworkActor* Actor = Framework.Get();
	if (!Actor || !Actor->GetPlayer()) {
		Check(false, TEXT("commute: framework available"));
		WriteReport(TEXT("commute"));
		return;
	}
	Time target = *Actor->GetPlayer()->GetTime();
	target.SetTime(7, 50, 0, 0);
	if (!(*Actor->GetPlayer()->GetTime() < target)) target.AddDays(1);
	CommutePhaseStart = FPlatformTime::Seconds();
	CommuteLastLog = 0.0;
	CommuteLastFraction = 0.f;
	CommuteLastProgress = FPlatformTime::Seconds();
	UE_LOG(LogTemp, Display, TEXT("ForeverTest commute: begin skip %s -> %s"), UTF8_TO_TCHAR(Actor->GetPlayer()->GetTime()->ToString(true, true).c_str()), UTF8_TO_TCHAR(target.ToString(true, true).c_str()));
	Actor->BeginTimeSkip(target);
	Check(Actor->IsTimeSkipping(), TEXT("commute: time skip to 07:50 started"));
}

void UForeverTestSubsystem::TickCommute(float DeltaTime)
{
	AForeverFrameworkActor* Actor = Framework.Get();
	if (!Actor) { Phase = EPhase::Finish; return; }
	const double Now = FPlatformTime::Seconds();
	switch (Phase) {
	case EPhase::CommuteSkip:
		if (Actor->IsTimeSkipping()) {
			UGameInstance* GI = Actor->GetGameInstance();
			UFadeSubsystem* Fade = GI ? GI->GetSubsystem<UFadeSubsystem>() : nullptr;
			float Fraction = 0.f; FText Label;
			Actor->GetTimeSkipProgress(Fraction, Label);
			const bool bHeldBlack = Fade && Fade->IsHeldBlack();
			// 看门狗：进度没动且黑屏没盖上(跳时间在等黑屏，而黑屏没出现)，60秒就判失败并留下现场，不空等到总超时。
			if (Fraction > CommuteLastFraction + 1e-6f || bHeldBlack) { CommuteLastFraction = Fraction; CommuteLastProgress = Now; }
			if (Now - CommuteLastLog >= 10.0) {
				CommuteLastLog = Now;
				UE_LOG(LogTemp, Display, TEXT("ForeverTest commute: skipping %.1f%% after %.0fs, game time %s, frame %.0fms, fade %s, held black %d"),
					Fraction * 100.f, Now - CommutePhaseStart, UTF8_TO_TCHAR(Actor->GetPlayer()->GetTime()->ToString(true, true).c_str()), DeltaTime * 1000.0,
					Fade ? TEXT("present") : TEXT("missing"), bHeldBlack ? 1 : 0);
			}
			if (Now - CommuteLastProgress > 60.0) {
				Check(false, TEXT("commute: time skip makes progress"), FString::Printf(TEXT("no progress for 60s: %.1f%%, game time %s, fade %s, held black %d"),
					Fraction * 100.f, UTF8_TO_TCHAR(Actor->GetPlayer()->GetTime()->ToString(true, true).c_str()), Fade ? TEXT("present") : TEXT("missing"), bHeldBlack ? 1 : 0));
				UE_LOG(LogTemp, Error, TEXT("ForeverTest commute: time skip stalled (fade %s, held black %d)"), Fade ? TEXT("present") : TEXT("missing"), bHeldBlack ? 1 : 0);
				WriteReport(TEXT("commute"));
				Phase = EPhase::Finish;
				break;
			}
		}
		if (!Actor->IsTimeSkipping()) {
			Metric(TEXT("commute.skip_seconds"), Now - CommutePhaseStart, TEXT("s"));
			if (!WantSuite(TEXT("commute")) && WantSuite(TEXT("traffic"))) { BeginTraffic(); break; }
			Actor->GetPlayer()->SetTimeFlowRatio(60.0); // 70分钟的高峰压缩到约70秒
			CommuteFrames.Reset();
			CommutePeakPending = 0;
			CommutePhaseStart = Now;
			Phase = EPhase::CommuteRun;
		}
		else if (Now - CommutePhaseStart > 900.0) {
			Check(false, TEXT("commute: time skip finishes within 900s"));
			WriteReport(TEXT("commute"));
			Phase = EPhase::Finish;
		}
		break;
	case EPhase::CommuteRun: {
		CommuteFrames.Add(DeltaTime);
		CommutePeakPending = FMath::Max(CommutePeakPending, UForeverPopulaceFrameworkComponent::GetPendingPathfindingTasks());
		Time nine = *Actor->GetPlayer()->GetTime();
		nine.SetTime(9, 0, 0, 0);
		const bool bDone = !(*Actor->GetPlayer()->GetTime() < nine) || Now - CommutePhaseStart > 240.0;
		if (bDone) {
			CommuteEndClock = UTF8_TO_TCHAR(Actor->GetPlayer()->GetTime()->ToString(true, true).c_str());
			TArray<float> Sorted = CommuteFrames;
			Sorted.Sort();
			if (Sorted.Num() > 0) {
				double Sum = 0;
				int32 Over100 = 0, Over250 = 0;
				for (float T : Sorted) { Sum += T; if (T > 0.1f) Over100++; if (T > 0.25f) Over250++; }
				auto Percentile = [&](double P) { return static_cast<double>(Sorted[FMath::Min(Sorted.Num() - 1, static_cast<int32>(Sorted.Num() * P))]) * 1000.0; };
				Metric(TEXT("commute.frames"), Sorted.Num());
				Metric(TEXT("commute.avg"), Sum / Sorted.Num() * 1000.0, TEXT("ms"));
				Metric(TEXT("commute.p95"), Percentile(0.95), TEXT("ms"));
				Metric(TEXT("commute.p99"), Percentile(0.99), TEXT("ms"));
				Metric(TEXT("commute.max"), static_cast<double>(Sorted.Last()) * 1000.0, TEXT("ms"));
				Metric(TEXT("commute.over100ms"), Over100);
				Metric(TEXT("commute.over250ms"), Over250);
				Check(Sorted.Last() < 1.0f, TEXT("commute: no frame over 1s during the morning rush"), FString::Printf(TEXT("max %.0fms"), Sorted.Last() * 1000.0));
			}
			Metric(TEXT("commute.peak_pending_pathfinding"), CommutePeakPending);
			Check(Now - CommutePhaseStart <= 240.0, TEXT("commute: reached 09:00 within the time limit"), CommuteEndClock);
			Actor->GetPlayer()->SetTimeFlowRatio(10.0);
			CommutePhaseStart = Now;
			Phase = EPhase::CommuteDrain;
		}
		break;
	}
	case EPhase::CommuteDrain:
		if (UForeverPopulaceFrameworkComponent::GetPendingPathfindingTasks() == 0 || Now - CommutePhaseStart > 60.0) {
			const int32 Left = UForeverPopulaceFrameworkComponent::GetPendingPathfindingTasks();
			Metric(TEXT("commute.drain_seconds"), Now - CommutePhaseStart, TEXT("s"));
			Check(Left == 0, TEXT("commute: async pathfinding queue drains after the rush"), FString::Printf(TEXT("%d tasks left"), Left));
			Check(Actor->GetSociety()->GetLedger().Total() == 0, TEXT("commute: ledger total is zero after the rush"));
			WriteReport(TEXT("commute"));
			if (WantSuite(TEXT("ride"))) BeginRide();
			else Phase = WantSuite(TEXT("saveload")) ? EPhase::SaveLoadSave : EPhase::Finish;
		}
		break;
	default:
		break;
	}
}

// ride：把玩家放上飞机/火车(搭乘模式，车自己按Core时刻表跑)，实时采样PerfSeconds秒的帧时间，再下车。
// 世界里没有这类可搭乘载具(config没启用对应mod)时只记指标不判失败。
void UForeverTestSubsystem::BeginRide()
{
	RideCategories = { TEXT("plane"), TEXT("train") };
	RideIndex = 0;
	Phase = EPhase::RideBoard;
}

void UForeverTestSubsystem::TickRide(float DeltaTime)
{
	AForeverFrameworkActor* Actor = Framework.Get();
	UWorld* World = Actor ? Actor->GetWorld() : nullptr;
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	if (!Actor || !Controller || !Actor->GetTrafficFramework() || !Actor->GetTraffic()) {
		Check(false, TEXT("ride: framework and player controller available"));
		WriteReport(TEXT("ride"));
		Phase = EPhase::Finish;
		return;
	}
	if (Phase == EPhase::RideBoard) {
		if (RideIndex >= RideCategories.Num()) {
			WriteReport(TEXT("ride"));
			Phase = WantSuite(TEXT("saveload")) ? EPhase::SaveLoadSave : EPhase::Finish;
			return;
		}
		const FString Category = RideCategories[RideIndex];
		FString Found;
		for (const auto& [Name, Vehicle] : Actor->GetTraffic()->GetVehicles()) {
			if (!Vehicle || FString(UTF8_TO_TCHAR(Vehicle->GetCategory().c_str())) != Category) continue;
			const FString VehicleName = UTF8_TO_TCHAR(Name.c_str());
			ATransitVehicleElement* Element = Actor->GetTrafficFramework()->FindTransitElement(VehicleName);
			if (Element && Element->CanBoard()) { Found = VehicleName; break; }
		}
		if (Found.IsEmpty()) {
			Metric(TEXT("ride.") + Category + TEXT(".available"), 0);
			RideIndex++;
			return;
		}
		Actor->GetTrafficFramework()->ApplyBoardVehicle(Found, Controller);
		const bool bOnBoard = Cast<ATransitVehicleElement>(Controller->GetPawn()) != nullptr;
		Check(bOnBoard, TEXT("ride: boarded a ") + Category, Found);
		if (!bOnBoard) { RideIndex++; return; }
		Metric(TEXT("ride.") + Category + TEXT(".available"), 1);
		RideFrames.Reset();
		RideStart = FPlatformTime::Seconds();
		Phase = EPhase::RideRun;
		return;
	}
	// RideRun
	RideFrames.Add(DeltaTime);
	if (FPlatformTime::Seconds() - RideStart < PerfSeconds) return;
	const FString Category = RideCategories[RideIndex];
	TArray<float> Sorted = RideFrames;
	Sorted.Sort();
	double Sum = 0;
	for (float T : Sorted) Sum += T;
	if (Sorted.Num() > 0) {
		Metric(TEXT("ride.") + Category + TEXT(".avg"), Sum / Sorted.Num() * 1000.0, TEXT("ms"));
		Metric(TEXT("ride.") + Category + TEXT(".p99"), static_cast<double>(Sorted[FMath::Min(Sorted.Num() - 1, static_cast<int32>(Sorted.Num() * 0.99))]) * 1000.0, TEXT("ms"));
		Metric(TEXT("ride.") + Category + TEXT(".max"), static_cast<double>(Sorted.Last()) * 1000.0, TEXT("ms"));
		Check(Sorted.Last() < 1.0f, TEXT("ride: no frame over 1s while riding a ") + Category, FString::Printf(TEXT("max %.0fms"), Sorted.Last() * 1000.0));
	}
	Check(Cast<ATransitVehicleElement>(Controller->GetPawn()) != nullptr, TEXT("ride: still aboard the ") + Category + TEXT(" after the sample"));
	Actor->GetTrafficFramework()->ApplyLeaveVehicle(FString(), Controller, true);
	Check(Cast<ATransitVehicleElement>(Controller->GetPawn()) == nullptr, TEXT("ride: left the ") + Category);
	RideIndex++;
	Phase = EPhase::RideBoard;
}

// changes：对每一种Change在主线程上同步执行并计时(AForeverFrameworkActor::ApplyChange)，每种执行10次，记首次(冷)、除首次外的平均和除首次外的最大耗时。
// 参数尽量用世界里取得到的真实市民/房间/载具/建筑；取不到的用占位值，这种情况走的是"找不到目标"的快速失败路径，
// 耗时只代表入口开销，指标里用 .real_args=0 标出。会修改世界状态，建议单独跑或放在其它套件之后。
static std::string Q(const std::string& s) { return std::string("\"") + s + "\""; }

void UForeverTestSubsystem::RunChangesSuite()
{
	AForeverFrameworkActor* Actor = Framework.Get();
	if (!Actor || !Actor->GetStory() || !Actor->GetPopulace() || !Actor->GetMap()) {
		Check(false, TEXT("changes: framework and Core domains available"));
		return;
	}
	Citizen* A = nullptr; Citizen* B = nullptr;
	for (Citizen* C : Actor->GetPopulace()->GetCitizens()) {
		if (!C || !C->GetRoom()) continue;
		if (!A) A = C; else if (!B) { B = C; break; }
	}
	const bool bCitizens = A && B;
	const std::string NameA = A ? A->GetName() : "nobody";
	const std::string NameB = B ? B->GetName() : "nobody";
	const std::string AddrA = A ? A->GetRoom()->GetAddress() : "none";
	const std::string AddrB = B ? B->GetRoom()->GetAddress() : "none";
	std::string CarName = "none";
	if (Actor->GetTraffic()) for (const auto& [Name, Vehicle] : Actor->GetTraffic()->GetVehicles()) {
		if (Vehicle && Vehicle->GetCategory() == "car") { CarName = Name; break; }
	}
	std::string BuildingName = "none";
	// 计时器要挂在有脚本的实体上才有意义(没脚本的会走"找不到目标"快速失败)，所以挑有脚本的建筑和门。
	for (const auto& [Name, Building] : Actor->GetMap()->GetBuildings()) { BuildingName = Name; if (Building && Building->GetScript()) break; }
	std::string DoorName = "none";
	for (Door* D : Actor->GetMap()->GetAllDoors()) { if (D && D->GetScript() && !D->GetInteractName().empty()) { DoorName = D->GetInteractName(); break; } }

	ScriptContext Context;
	Context.self = Actor->GetStory()->GetMainScript();
	Context.system = Actor->GetStory()->GetSystemScript();
	Context.global = Actor->GetStory()->GetGlobalScript();
	Check(Context.self != nullptr, TEXT("changes: main script exists to run Changes against"));
	if (!Context.self) return;

	// 执行脚本是主线脚本(没有所有者)；Self非空时改用那份脚本当self，测"对自己操作"的直达路径(不查找)。
	Script* VehicleScript = nullptr;
	if (Actor->GetTraffic()) for (const auto& [Name, Vehicle] : Actor->GetTraffic()->GetVehicles()) {
		if (Vehicle && Vehicle->GetScript()) { VehicleScript = Vehicle->GetScript(); break; }
	}
	Script* CitizenScript = (A && A->GetScheduler()) ? A->GetScheduler()->GetScript() : nullptr;
	struct FEntry { const char* Type; bool bRealArgs; TFunction<Change*()> Make; Container* Self = nullptr; };
	TArray<FEntry> Entries;
	auto Add = [&](const char* Type, bool bReal, TFunction<Change*()> Make) { Entries.Add({ Type, bReal, MoveTemp(Make) }); };
	auto AddSelf = [&](const char* Type, Container* Self, TFunction<Change*()> Make) { Entries.Add({ Type, Self != nullptr, MoveTemp(Make), Self }); };
	// ---- 纯数据/脚本变量类 ----
	Add("place_holder", true, [] { return new PlaceHolderChange("x"); });
	Add("debug_print", true, [] { return new DebugPrintChange("changes test"); });
	Add("global_message", true, [] { return new GlobalMessageChange("changes_test"); }); // 不带category：发给全部脚本(旧行为)
	Add("global_message_main", true, [] { return new GlobalMessageChange("changes_test", "main"); });
	Add("global_message_job", true, [] { return new GlobalMessageChange("changes_test", "job,organization"); });
	Add("global_message_citizen", true, [] { return new GlobalMessageChange("changes_test", "citizen"); });
	Add("set_value", true, [] { return new SetValueChange("self.changes_test", "1"); });
	Add("remove_value", true, [] { return new RemoveValueChange("self.changes_test"); });
	Add("global_setting", true, [] { return new GlobalSettingChange("time_flow_ratio", "10"); });
	Add("add_option", true, [&] { return new AddOptionChange(Q(NameA), Q("changes_test_option")); }); // 不带category：每个域各找一遍(旧行为)
	Add("remove_option", true, [&] { return new RemoveOptionChange(Q(NameA), Q("changes_test_option")); });
	Add("add_option_citizen", bCitizens, [&] { return new AddOptionChange(Q(NameA), Q("changes_test_option"), "citizen"); });
	Add("add_option_vehicle", CarName != "none", [&] { return new AddOptionChange(Q(CarName), Q("changes_test_option"), "vehicle"); });
	Add("add_option_asset", true, [&] { return new AddOptionChange(Q("changes_no_such_asset"), Q("changes_test_option"), "asset"); }); // 找不到的资产名：量遍历房间的开销
	Add("remove_option_citizen", bCitizens, [&] { return new RemoveOptionChange(Q(NameA), Q("changes_test_option"), "citizen"); });
	Add("deactivate_milestone", false, [] { return new DeactivateMilestoneChange("changes_test"); });
	Add("create_timer", true, [] { return new CreateTimerChange("changes_test_timer", "23:00:00", "", "", "next"); });
	Add("remove_timer", true, [] { return new RemoveTimerChange("changes_test_timer"); });
	// 对自己操作：脚本记着所有者/稳定键，不应该有任何查找。
	AddSelf("add_option_self_vehicle", VehicleScript, [] { return new AddOptionChange("$$self.name", Q("changes_self_option")); });
	AddSelf("remove_option_self_vehicle", VehicleScript, [] { return new RemoveOptionChange("$$self.name", Q("changes_self_option")); });
	AddSelf("add_option_self_citizen", CitizenScript, [] { return new AddOptionChange("$$self.name", Q("changes_self_option")); });
	AddSelf("create_timer_self_vehicle", VehicleScript, [] { return new CreateTimerChange("changes_self_timer", "23:00:00", "", "", "next"); });
	AddSelf("remove_timer_self_vehicle", VehicleScript, [] { return new RemoveTimerChange("changes_self_timer"); });
	// 其它归属类别：归属脚本换算成稳定键走的是不同的路径(曾经每次重建全城脚本，30ms+)。
	Add("create_timer_building", BuildingName != "none", [&] { return new CreateTimerChange("changes_test_timer_b", "23:00:00", "building", Q(BuildingName), "next"); });
	Add("create_timer_room", bCitizens, [&] { return new CreateTimerChange("changes_test_timer_r", "23:00:00", "room", Q(AddrA), "next"); });
	Add("create_timer_door", DoorName != "none", [&] { return new CreateTimerChange("changes_test_timer_d", "23:00:00", "door", Q(DoorName), "next"); });
	Add("create_timer_vehicle", CarName != "none", [&] { return new CreateTimerChange("changes_test_timer_v", "23:00:00", "vehicle", Q(CarName), "next"); });
	Add("create_timer_citizen", bCitizens, [&] { return new CreateTimerChange("changes_test_timer_c", "23:00:00", "citizen", Q(NameA), "next"); });
	Add("record_object", true, [&] { return new RecordObjectChange(Q("building"), Q("building_empty"), Q("first"), "changes_obj"); });
	// ---- 市民 ----
	Add("npc_teleport", bCitizens, [&] { return new NPCTeleportChange(Q(NameA), Q(AddrB)); });
	Add("npc_navigate", bCitizens, [&] { return new NPCNavigateChange(Q(NameA), Q(AddrB)); });
	Add("npc_revive", false, [&] { return new NPCReviveChange(Q(NameA), "50"); });
	Add("assign_task", false, [&] { return new AssignTaskChange(Q("deliver"), Q(AddrB), "", "", ""); });
	Add("pay_wage", false, [] { return new PayWageChange("0"); });
	Add("spawn_npc", true, [] { return new SpawnNpcChange("", Q("changes_npc"), Q("male"), Q("2000-01-01"), "", "", "", "", "", "", {}, "", ""); });
	// ---- 玩家/世界 ----
	Add("teleport_player", bCitizens, [&] { return new TeleportPlayerChange(Q(AddrB)); });
	Add("change_control", bCitizens, [&] { return new ChangeControlChange(Q(NameB)); });
	Add("change_weather", true, [] { return new ChangeWeatherChange(Q("rain")); });
	Add("change_time", true, [] { return new ChangeTimeChange("0"); });
	Add("change_wanted", true, [] { return new ChangeWantedChange(Q("changes_test"), "0"); });
	Add("player_injured", true, [] { return new PlayerInjuredChange(Q("bruise")); });
	Add("player_cured", true, [] { return new PlayerCuredChange(Q("bruise")); });
	Add("player_ill", true, [] { return new PlayerIllChange(Q("cold")); });
	Add("player_recover", true, [] { return new PlayerRecoverChange(Q("cold")); });
	Add("player_sleep", true, [] { return new PlayerSleepChange("0"); });
	Add("bank_transaction", true, [&] { return new BankTransactionChange(Q(NameA), "0"); });
	Add("give_estate", bCitizens, [&] { return new GiveEstateChange(Q(AddrB), Q(NameA), Q("true")); });
	Add("remove_estate", bCitizens, [&] { return new RemoveEstateChange(Q(AddrB), Q(NameA)); });
	Add("give_vehicle", bCitizens, [&] { return new GiveVehicleChange(Q(CarName), Q(NameA), Q("true")); });
	Add("remove_vehicle", bCitizens, [&] { return new RemoveVehicleChange(Q(CarName), Q(NameA)); });
	// ---- 背包 ----
	Add("grant_asset", true, [] { return new GrantAssetChange(Q("asset_ammo_pistol"), "1"); });
	Add("deduct_asset", true, [] { return new DeductAssetChange(Q("asset_ammo_pistol"), "1"); });
	Add("drop_asset", true, [] { return new DropAssetChange(Q("asset_ammo_pistol"), "1"); });
	Add("pick_asset", false, [] { return new PickAssetChange(Q("none")); });
	// ---- 载具 ----
	Add("vehicle_navigate", CarName != "none", [&] { return new VehicleNavigateChange(Q(CarName), Q(AddrB)); });
	Add("vehicle_command", CarName != "none", [&] { return new VehicleCommandChange(Q(CarName), Q("stop")); });
	Add("show_route", false, [] { return new ShowRouteChange(Q("none")); });
	Add("enter_vehicle", CarName != "none", [&] { return new EnterVehicleChange(Q(CarName)); });
	Add("leave_vehicle", true, [] { return new LeaveVehicleChange(""); });
	Add("board_vehicle", false, [] { return new BoardVehicleChange(Q("none")); });
	Add("force_leave", true, [] { return new ForceLeaveChange(""); });
	// ---- 门/电梯/界面 ----
	Add("set_door", false, [] { return new SetDoorChange(Q("none"), "true"); });
	Add("register_door", false, [&] { return new RegisterDoorChange(Q("none"), Q(NameA), "true"); });
	Add("launch_elevator", false, [&] { return new LaunchElevatorChange(Q(BuildingName), Q("none"), Q("up")); });
	Add("open_shop", true, [] { return new OpenShopChange(""); });
	// 默认config没启用任何puzzle mod：用未启用的id验证"静默忽略并打日志、不崩溃"的路径(UPuzzleWidget::StartPuzzle)。
	Add("start_puzzle", false, [] { return new StartPuzzleChange(Q("puzzle_not_enabled")); });
	Add("play_video", false, [] { return new PlayVideoChange(Q("none")); });
	Add("play_bgm", false, [] { return new PlayBgmChange(Q("none"), "false"); });
	Add("stop_bgm", true, [] { return new StopBgmChange(); });
	// 最后做会删掉测试市民的：
	Add("remove_npc", true, [] { return new RemoveNpcChange(Q("changes_npc")); });
	// visualize_navigation会留下一条活动导航线，之后每次换Pawn(上下车)都要按新Pawn重新规划路线(几十毫秒)，所以放在最后。
	Add("visualize_navigation", bCitizens, [&] { return new VisualizeNavigationChange(Q(AddrB)); });
	// 不测：game_end(退出游戏)、jump_time(耗时由 commute 套件记录)、for_range(容器，子Change各自已测)。

	constexpr int Repeats = 10;
	constexpr double LimitMs = 100.0;
	int32 Slow = 0;
	for (const FEntry& Entry : Entries) {
		double First = 0, Max = 0, Sum = 0;
		for (int32 I = 0; I < Repeats; I++) {
			std::unique_ptr<Change> Item(Entry.Make());
			const double Start = FPlatformTime::Seconds();
			ScriptContext RunContext = Context;
			if (Entry.Self) RunContext.self = Entry.Self;
			Actor->ApplyChange(Item.get(), RunContext);
			const double Ms = (FPlatformTime::Seconds() - Start) * 1000.0;
			if (I == 0) { First = Ms; continue; }
			Max = FMath::Max(Max, Ms);
			Sum += Ms;
		}
		const FString Key = FString(TEXT("change.")) + UTF8_TO_TCHAR(Entry.Type);
		Metric(Key + TEXT(".first_ms"), First, TEXT("ms"));
		Metric(Key + TEXT(".max_ms"), Max, TEXT("ms"));
		Metric(Key + TEXT(".avg_ms"), Sum / (Repeats - 1), TEXT("ms"));
		Metric(Key + TEXT(".real_args"), Entry.bRealArgs ? 1 : 0);
		const double Worst = FMath::Max(First, Max);
		if (Worst > LimitMs) Slow++;
		Check(Worst <= LimitMs, FString::Printf(TEXT("change %s stays under %.0fms on the main thread"), UTF8_TO_TCHAR(Entry.Type), LimitMs),
			FString::Printf(TEXT("first %.2fms max %.2fms%s"), First, Max, Entry.bRealArgs ? TEXT("") : TEXT(" (placeholder args)")));
	}
	Metric(TEXT("changes.tested"), Entries.Num());
	Metric(TEXT("changes.over_limit"), Slow);
}

// traffic：跳到早高峰，时间加速，让步行的玩家每隔几秒被传到一辆正在行驶的车旁边，
// 反复触发载具Actor的流式生成/销毁和Chaos物理(打包版在路边走路时被过路车辆崩溃过)。进程崩溃由外层脚本按退出码判失败。
void UForeverTestSubsystem::BeginTraffic()
{
	Framework->GetPlayer()->SetTimeFlowRatio(30.0);
	double Seconds = 300.0;
	FParse::Value(FCommandLine::Get(), TEXT("-ForeverTestTrafficSeconds="), Seconds);
	TrafficSeconds = Seconds;
	TrafficStart = TrafficLastHop = FPlatformTime::Seconds();
	TrafficHops = TrafficPeakVehicles = TrafficCursor = 0;
	TrafficFrames.Reset();
	Phase = EPhase::TrafficRun;
}

void UForeverTestSubsystem::TickTraffic(float DeltaTime)
{
	AForeverFrameworkActor* Actor = Framework.Get();
	UWorld* World = Actor ? Actor->GetWorld() : nullptr;
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	if (!Actor || !Controller || !Controller->GetPawn() || !Actor->GetTraffic()) {
		Check(false, TEXT("traffic: framework, player controller and pawn available"));
		WriteReport(TEXT("traffic"));
		Phase = EPhase::Finish;
		return;
	}
	const double Now = FPlatformTime::Seconds();
	TrafficFrames.Add(DeltaTime);
	if (Now - TrafficLastHop >= 6.0) {
		TrafficLastHop = Now;
		TArray<Vehicle*> Driving;
		for (const auto& [Name, V] : Actor->GetTraffic()->GetVehicles()) {
			if (V && V->GetCategory() == "car" && V->IsDriving()) Driving.Add(V);
		}
		if (Driving.Num() > 0) {
			Vehicle* Target = Driving[TrafficCursor++ % Driving.Num()];
			float X, Y, Z, Yaw;
			Target->GetTransform(X, Y, Z, Yaw);
			Controller->GetPawn()->SetActorLocation(FVector(X, Y, Z + 150.f), false, nullptr, ETeleportType::TeleportPhysics);
			TrafficHops++;
		}
		int32 Spawned = 0;
		FString Classes;
		for (TActorIterator<AVehicleElement> It(World); It; ++It) { Spawned++; Classes += It->GetClass()->GetName() + TEXT(" "); }
		UE_LOG(LogTemp, Display, TEXT("ForeverTest traffic: vehicle actors: %s"), *Classes);
		TrafficPeakVehicles = FMath::Max(TrafficPeakVehicles, Spawned);
		UE_LOG(LogTemp, Display, TEXT("ForeverTest traffic: %.0fs, %d driving cars, %d vehicle actors, %d hops"), Now - TrafficStart, Driving.Num(), Spawned, TrafficHops);
	}
	if (Now - TrafficStart < TrafficSeconds) return;
	TArray<float> Sorted = TrafficFrames;
	Sorted.Sort();
	Metric(TEXT("traffic.hops"), TrafficHops);
	Metric(TEXT("traffic.peak_vehicle_actors"), TrafficPeakVehicles);
	if (Sorted.Num() > 0) {
		Metric(TEXT("traffic.p99"), static_cast<double>(Sorted[FMath::Min(Sorted.Num() - 1, static_cast<int32>(Sorted.Num() * 0.99))]) * 1000.0, TEXT("ms"));
		Metric(TEXT("traffic.max"), static_cast<double>(Sorted.Last()) * 1000.0, TEXT("ms"));
	}
	Check(TrafficHops > 0, TEXT("traffic: found driving cars to follow"), FString::Printf(TEXT("%d hops"), TrafficHops));
	Check(true, TEXT("traffic: survived the run without crashing"));
	WriteReport(TEXT("traffic"));
	Phase = EPhase::Finish;
}
