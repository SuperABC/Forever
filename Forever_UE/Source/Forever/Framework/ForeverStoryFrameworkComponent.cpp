#include "Framework/ForeverStoryFrameworkComponent.h"

#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverPopulaceFrameworkComponent.h"
#include "Framework/ForeverBuildingFrameworkComponent.h"
#include "Framework/ForeverTrafficFrameworkComponent.h"
#include "Element/CitizenElement.h"
#include "Player/ForeverPlayerController.h"
#include "UI/SectionSpeakingWidget.h"
#include "UI/BackgroundSpeakingWidget.h"
#include "UI/SectionOptionWidget.h"
#include "UI/MeetOptionWidget.h"
#include "UI/PuzzleWidget.h"

#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

#include "story/story.h"
#include "story/script.h"
#include "story/dialog.h"
#include "story/change.h"
#include "story/event.h"
#include "society/society.h"
#include "society/organization.h"
#include "society/job.h"
#include "populace/populace.h"
#include "populace/citizen.h"
#include "populace/scheduler.h"
#include "traffic/traffic.h"
#include "map/door.h"
#include "map/map.h"
#include "map/cabin.h"
#include "map/zone.h"
#include "map/building.h"
#include "map/room.h"
#include "story/script_save.h"
#include "player/player.h"
#include "common/implement.h"

using namespace std;

namespace {
	// 从BroadcastGameStart原来的onActions lambda体搬出来的自由函数——OptionDialog()这次
	// 需要复用同一套"Dialog交给ShowDialog展示/Change转发给ApplyChange"的处理逻辑，不再只是
	// BroadcastGameStart内部的一次性lambda。不进头文件：ScriptAction是std::variant，没法
	// 前置声明，进头文件要多#include一个story/script_mod.h，没必要。
	void ProcessScriptActions(AForeverFrameworkActor* framework, const vector<ScriptAction>& actions,
		const ScriptContext& context) {
		for (const auto& action : actions) {
			if (auto dialogPtr = get_if<const Dialog*>(&action)) {
				// 不在这里直接打印，交给
				// UForeverStoryFrameworkComponent::ShowDialog统一展示/排队，见该函数声明处
				// 注释。
				if (framework && framework->GetStoryFramework()) {
					framework->GetStoryFramework()->ShowDialog(*dialogPtr, context);
				}
			}
			else if (auto changePtr = get_if<const Change*>(&action)) {
				// 统一交给AForeverFrameworkActor::ApplyChange消费——不再在这里各自手写
				// dynamic_cast dispatch，见ForeverFrameworkActor.md"统一的Change消费入口"
				// 一节。
				if (framework) framework->ApplyChange(*changePtr, context);
			}
		}
	}
}

void UForeverStoryFrameworkComponent::Init(Story* inStory) {
	story = inStory;
}

void UForeverStoryFrameworkComponent::UpdateSystemTime() {
	if (!story) return;
	Script* systemScript = story->GetSystemScript();
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Player* player = framework ? framework->GetPlayer() : nullptr;
	Time* time = player ? player->GetTime() : nullptr;
	if (!systemScript || !time) return;
	systemScript->SetValue("time.year", ValueType(time->GetYear()));
	systemScript->SetValue("time.month", ValueType(time->GetMonth()));
	systemScript->SetValue("time.day", ValueType(time->GetDay()));
	systemScript->SetValue("time.hour", ValueType(time->GetHour()));
	systemScript->SetValue("time.minute", ValueType(time->GetMinute()));
	systemScript->SetValue("time.second", ValueType(time->GetSecond()));
}

void UForeverStoryFrameworkComponent::BroadcastGameStart() {
	if (!story || !GEngine) return;

	// 现场构造一个PostImplement，供WrapScript通过Post()查询Core状态（这次唯一用到的查询是
	// "random citizen"，见Core/common/implement.md）。生命周期只需要覆盖这次广播，不用长期
	// 持有。
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Society* society = framework ? framework->GetSociety() : nullptr;
	PostImplement postImplement(
		framework ? framework->GetMap() : nullptr,
		framework ? framework->GetPopulace() : nullptr,
		society,
		story,
		framework ? framework->GetIndustry() : nullptr,
		framework ? framework->GetTraffic() : nullptr,
		framework ? framework->GetPlayer() : nullptr);

	auto onActions = [framework](const vector<ScriptAction>& actions, const ScriptContext& context) {
		ProcessScriptActions(framework, actions, context);
		};

	UpdateSystemTime();
	story->BroadcastGameStart(onActions, &postImplement);

	// 主线以外的全部Script(组织/岗位/市民Scheduler/车辆/门)也要各广播一次game_start，否则它们
	// 各自脚本里的game_start milestone(加交互选项等)永远不会被触发。
	GameStartEvent event;
	BroadcastToAllScripts(event, postImplement, /*includeMain=*/false);
}

void UForeverStoryFrameworkComponent::BroadcastToScript(Event& event, Script* script, PostImplement& postImplement) {
	if (!story || !script) return;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	ScriptContext context;
	context.self = script;
	context.system = story->GetSystemScript();
	context.global = story->GetGlobalScript();
	context.local = &event;
	ProcessScriptActions(framework, script->MatchEvent(&event, context, &postImplement), context);
}

void UForeverStoryFrameworkComponent::BroadcastToAllScripts(Event& event, PostImplement& postImplement, bool includeMain) {
	if (!story) return;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());

	auto broadcastOne = [&](Script* script) {
		BroadcastToScript(event, script, postImplement);
		};

	if (includeMain) broadcastOne(story->GetMainScript());

	if (Society* society = framework ? framework->GetSociety() : nullptr) {
		for (Organization* org : society->GetOrganizations()) {
			broadcastOne(org->GetScript());
			for (Job* job : org->GetJobs()) {
				broadcastOne(job->GetScript());
			}
		}
	}

	// 每个市民独占持有一份Scheduler，Scheduler自己的Script(schedule_empty.script)。
	if (Populace* populace = framework ? framework->GetPopulace() : nullptr) {
		for (Citizen* citizen : populace->GetCitizens()) {
			if (citizen && citizen->GetScheduler()) {
				broadcastOne(citizen->GetScheduler()->GetScript());
			}
		}
	}

	// 每辆车独占持有一份Script(vehicle_basic.script)。
	if (Traffic* traffic = framework ? framework->GetTraffic() : nullptr) {
		for (auto& [name, vehicle] : traffic->GetVehicles()) {
			if (vehicle) broadcastOne(vehicle->GetScript());
		}
	}

	// 只有spec.name非空的门才会有Script(见Core/map/door.md)，过滤掉纯自动门；电梯同理(spec指定了
	// 里程碑文件才有Script)。
	if (Map* map = framework ? framework->GetMap() : nullptr) {
		for (Door* door : map->GetAllDoors()) {
			if (door && door->GetScript()) broadcastOne(door->GetScript());
		}
		for (auto& [name, building] : map->GetBuildings()) {
			if (!building) continue;
			for (Cabin* cabin : building->GetCabinEntities()) {
				if (cabin && cabin->GetScript()) broadcastOne(cabin->GetScript());
			}
			// 实体脚本(建筑/房间/园区)：收game_start、计时器等普通广播；它们的进出事件不走这里，
			// 只由BroadcastPlayerLocation发给被进出的那一个。
			broadcastOne(building->GetScript());
			for (Room* room : building->GetRooms()) {
				if (room) broadcastOne(room->GetScript());
			}
		}
		for (auto& [name, zone] : map->GetZones()) {
			if (zone) broadcastOne(zone->GetScript());
		}
	}
}

void UForeverStoryFrameworkComponent::BroadcastGlobalMessage(const string& message) {
	if (!story) return;
	if (globalMessageDepth >= kMaxGlobalMessageDepth) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverStoryFrameworkComponent::BroadcastGlobalMessage: nested global_message depth exceeded %d, dropped \"%s\" (do two milestones keep triggering each other?)"),
			kMaxGlobalMessageDepth, UTF8_TO_TCHAR(message.c_str()));
		return;
	}

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	PostImplement postImplement(
		framework ? framework->GetMap() : nullptr,
		framework ? framework->GetPopulace() : nullptr,
		framework ? framework->GetSociety() : nullptr,
		story,
		framework ? framework->GetIndustry() : nullptr,
		framework ? framework->GetTraffic() : nullptr,
		framework ? framework->GetPlayer() : nullptr);

	GlobalMessageEvent event(message);
	globalMessageDepth++;
	BroadcastToAllScripts(event, postImplement, /*includeMain=*/true);
	globalMessageDepth--;
}

void UForeverStoryFrameworkComponent::OptionDialog(const FString& name, const FString& option) {
	if (!story || !GEngine) return;

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Populace* populace = framework ? framework->GetPopulace() : nullptr;

	PostImplement postImplement(
		framework ? framework->GetMap() : nullptr,
		populace,
		framework ? framework->GetSociety() : nullptr,
		story,
		framework ? framework->GetIndustry() : nullptr,
		framework ? framework->GetTraffic() : nullptr,
		framework ? framework->GetPlayer() : nullptr);

	string nameUtf8 = TCHAR_TO_UTF8(*name);
	string optionUtf8 = TCHAR_TO_UTF8(*option);
	OptionDialogEvent event(nameUtf8, optionUtf8);

	auto matchOne = [&](Script* script) {
		if (!script) return;
		ScriptContext context;
		context.self = script;
		context.system = story->GetSystemScript();
		context.global = story->GetGlobalScript();
		context.local = &event;
		ProcessScriptActions(framework, script->MatchEvent(&event, context, &postImplement), context);
		};

	matchOne(story->GetMainScript());

	Citizen* target = populace ? populace->FindCitizenByName(nameUtf8) : nullptr;
	if (target && target->GetScheduler()) {
		matchOne(target->GetScheduler()->GetScript());
	}

	// 载具系统落地新增：name也可能是一辆车(见vehicle_basic.script的"上车"选项)，同样额外
	// 匹配它自己的Script，和市民分支并列，互不影响(一个name不会同时是市民又是车辆)。
	Traffic* traffic = framework ? framework->GetTraffic() : nullptr;
	Vehicle* targetVehicle = traffic ? traffic->FindVehicleByName(nameUtf8) : nullptr;
	if (targetVehicle && targetVehicle->GetScript()) {
		matchOne(targetVehicle->GetScript());
	}

	// 门系统落地新增：name也可能是一扇门的交互名(见door_shop_entrance.script的"敲门"选项)，
	// 同样额外匹配，和市民/车辆分支并列。
	Map* map = framework ? framework->GetMap() : nullptr;
	Door* targetDoor = map ? map->FindDoorByName(nameUtf8) : nullptr;
	if (targetDoor && targetDoor->GetScript()) {
		matchOne(targetDoor->GetScript());
	}

	// 电梯：name是电梯的交互名("<building名> 电梯<n>")。
	Cabin* targetCabin = map ? map->FindCabinByName(nameUtf8) : nullptr;
	if (targetCabin && targetCabin->GetScript()) {
		matchOne(targetCabin->GetScript());
	}
}

void UForeverStoryFrameworkComponent::BroadcastCitizenDecease(const FString& name, const FString& reason) {
	if (!story || !GEngine) return;

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());

	PostImplement postImplement(
		framework ? framework->GetMap() : nullptr,
		framework ? framework->GetPopulace() : nullptr,
		framework ? framework->GetSociety() : nullptr,
		story,
		framework ? framework->GetIndustry() : nullptr,
		framework ? framework->GetTraffic() : nullptr,
		framework ? framework->GetPlayer() : nullptr);

	string nameUtf8 = TCHAR_TO_UTF8(*name);
	CitizenDeceaseEvent event(nameUtf8, string(TCHAR_TO_UTF8(*reason)));

	auto matchOne = [&](Script* script) {
		if (!script) return;
		ScriptContext context;
		context.self = script;
		context.system = story->GetSystemScript();
		context.global = story->GetGlobalScript();
		context.local = &event;
		ProcessScriptActions(framework, script->MatchEvent(&event, context, &postImplement), context);
		};

	// 和OptionDialog同一个模式：主线脚本 + 死者自己的Scheduler脚本(按name找)。死亡时只销毁了
	// UE侧的Actor，Core里的Citizen还在(标记为死亡)，这时它的Scheduler脚本仍然有效。
	matchOne(story->GetMainScript());

	Populace* populace = framework ? framework->GetPopulace() : nullptr;
	Citizen* target = populace ? populace->FindCitizenByName(nameUtf8) : nullptr;
	if (target && target->GetScheduler()) {
		matchOne(target->GetScheduler()->GetScript());
	}
}

void UForeverStoryFrameworkComponent::ApplyControlChange(const ChangeControlChange* change, const ScriptContext& context) {
	FString name = UTF8_TO_TCHAR(ToString(EvaluateExpression(change->GetName(), context)).data());

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	UForeverPopulaceFrameworkComponent* populaceFramework = framework ? framework->GetPopulaceFramework() : nullptr;
	if (!populaceFramework) return;

	ACitizenElement* target = populaceFramework->FindOrSpawnCitizenByName(name);
	if (!target) {
		debugf("Warning: ChangeControlChange target citizen not found: %s.\n", TCHAR_TO_UTF8(*name));
		return;
	}

	if (APlayerController* playerController = UGameplayStatics::GetPlayerController(GetWorld(), 0)) {
		playerController->Possess(target);
	}

	// 每次切换玩家控制权都可能是"游戏刚开始"这种建筑近处LOD(楼层/房间)还没来得及排队建完
	// 的时刻——玩家/市民会先掉到还没生成细节的地面上，等建筑加载完才落地，观感很差。这里
	// 请求冻结世界(UE时间倍率归0，物理/移动全部停摆)直到所有building的LOD切换队列清空，
	// 见UForeverBuildingFrameworkComponent::RequestFreezeUntilLodSettled。
	if (framework && framework->GetBuildingFramework()) {
		framework->GetBuildingFramework()->RequestFreezeUntilLodSettled();
	}
}

void UForeverStoryFrameworkComponent::ApplyStartPuzzle(const StartPuzzleChange* change, const ScriptContext& context) {
	FString puzzleId = UTF8_TO_TCHAR(ToString(EvaluateExpression(change->GetPuzzle(), context)).data());
	// param是剧情脚本给小游戏的启动参数，空串表示脚本没传，不求值
	FString param;
	if (!change->GetParam().empty()) {
		param = UTF8_TO_TCHAR(ToString(EvaluateExpression(change->GetParam(), context)).data());
	}

	AForeverPlayerController* playerController = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (!playerController || !playerController->GetPuzzleWidget()) return;

	playerController->GetPuzzleWidget()->StartPuzzle(puzzleId, param);
}

void UForeverStoryFrameworkComponent::ApplyEnterVehicle(const EnterVehicleChange* change, const ScriptContext& context) {
	FString name = UTF8_TO_TCHAR(ToString(EvaluateExpression(change->GetVehicle(), context)).data());

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	UForeverTrafficFrameworkComponent* trafficFramework = framework ? framework->GetTrafficFramework() : nullptr;
	if (!trafficFramework) return;

	APlayerController* playerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!playerController) return;

	trafficFramework->ApplyEnterVehicle(name, playerController);
}

void UForeverStoryFrameworkComponent::EnqueueDialog(const Dialog* dialog, const ScriptContext& context, int32 insertIndex) {
	if (!dialog) return;

	// Dialog自己的condition：非空且求值为假就整段不展示(求值时机是入队的这一刻，所以同一个里程碑里
	// 排在前面的changes已经生效，见Script::MatchEvent先放changes再放dialogs)。
	if (!dialog->GetCondition().empty() && !EvaluateExpressionBool(dialog->GetCondition(), context)) return;

	// self/system指向长期存活的Script*/systemScript，照抄传入的context；local指向调用方
	// 栈上的运行时Event，这次调用一结束就悬垂，强制清空，见ShowDialog声明处注释。
	ScriptContext deferredContext = context;
	deferredContext.local = nullptr;

	TArray<FPendingItem> items;
	for (Section section : dialog->GetDialogs()) {
		FPendingItem item;
		item.context = deferredContext;

		if (section.IsBranch()) {
			item.bBranch = true;
			for (const Option& option : section.GetOptions()) {
				// 分支选项的condition：求值为假的选项不出现。
				if (!option.GetCondition().empty() && !EvaluateExpressionBool(option.GetCondition(), context)) continue;
				FString text = UTF8_TO_TCHAR(ToString(EvaluateExpression(option.GetOption(), context)).data());
				item.optionTexts.Add(text);
				item.options.Add(option);
			}
		}
		else {
			section.EvaluateText(context);
			auto [speaker, content, label, voice] = section.GetSpeaking();
			item.speaker = UTF8_TO_TCHAR(speaker.data());
			item.content = UTF8_TO_TCHAR(content.data());
			item.label = label;
			item.background = section.GetBackground();
		}

		// 分支的选项全被condition过滤掉了：没有可点的选项，整个分支跳过，免得界面卡在空选项上。
		if (item.bBranch && item.options.Num() == 0) continue;

		items.Add(MoveTemp(item));
	}

	insertIndex = FMath::Clamp(insertIndex, 0, pendingQueue.Num());
	pendingQueue.Insert(items, insertIndex);
}

void UForeverStoryFrameworkComponent::ShowDialog(const Dialog* dialog, const ScriptContext& context) {
	bool wasEmpty = pendingQueue.Num() == 0;
	EnqueueDialog(dialog, context, pendingQueue.Num());
	if (wasEmpty) {
		ShowCurrentItem();
	}
}

void UForeverStoryFrameworkComponent::AdvanceDialog() {
	if (pendingQueue.Num() == 0) return;

	std::string finishedLabel = pendingQueue[0].bBranch ? std::string() : pendingQueue[0].label;
	pendingQueue.RemoveAt(0);
	// 先广播再显示下一项：命中的milestone如果又弹出对话，会通过ShowDialog排到队尾(队列空时
	// ShowDialog自己就显示)；下面的ShowCurrentItem重复显示同一项没有副作用。
	if (!finishedLabel.empty()) BroadcastSpeakingFinish(finishedLabel);
	ShowCurrentItem();
}

void UForeverStoryFrameworkComponent::BroadcastPlayerLocation(Event& event, const string& kind, const string& name, bool entering,
	Script* target) {
	if (!story) return;

	if (Script* systemScript = story->GetSystemScript()) {
		string key = "player." + kind;
		if (entering) {
			systemScript->SetValue(key, ValueType(name));
		}
		else {
			auto [found, current] = systemScript->GetValue(key);
			if (found && ToString(current) == name) systemScript->SetValue(key, ValueType(string()));
		}
	}

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	PostImplement postImplement(
		framework ? framework->GetMap() : nullptr,
		framework ? framework->GetPopulace() : nullptr,
		framework ? framework->GetSociety() : nullptr,
		story,
		framework ? framework->GetIndustry() : nullptr,
		framework ? framework->GetTraffic() : nullptr,
		framework ? framework->GetPlayer() : nullptr);

	BroadcastToScript(event, target, postImplement);
}

string UForeverStoryFrameworkComponent::FindScriptKey(const Container* script) const {
	if (!script) return string();
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	if (!framework) return string();
	for (auto& [key, candidate] : ScriptSaveCodec::CollectScripts(framework->GetMap(), framework->GetPopulace(),
		framework->GetSociety(), framework->GetTraffic(), story)) {
		if (candidate == script) return key;
	}
	return string();
}

Script* UForeverStoryFrameworkComponent::FindScriptByKey(const string& key) const {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	if (!framework || key.empty()) return nullptr;
	for (auto& [candidateKey, script] : ScriptSaveCodec::CollectScripts(framework->GetMap(), framework->GetPopulace(),
		framework->GetSociety(), framework->GetTraffic(), story)) {
		if (candidateKey == key) return script;
	}
	return nullptr;
}

const Container* UForeverStoryFrameworkComponent::ResolveTimerOwner(const string& category, const string& label, const Container* self) const {
	if (category.empty()) return self;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	if (category == "global" || category == "main") return story ? story->GetMainScript() : nullptr;
	if (category == "citizen") {
		Populace* populace = framework ? framework->GetPopulace() : nullptr;
		Citizen* citizen = populace ? populace->FindCitizenByName(label) : nullptr;
		return citizen && citizen->GetScheduler() ? citizen->GetScheduler()->GetScript() : nullptr;
	}
	if (category == "vehicle") {
		Traffic* traffic = framework ? framework->GetTraffic() : nullptr;
		Vehicle* vehicle = traffic ? traffic->FindVehicleByName(label) : nullptr;
		return vehicle ? vehicle->GetScript() : nullptr;
	}
	if (category == "door") {
		Map* map = framework ? framework->GetMap() : nullptr;
		Door* door = map ? map->FindDoorByName(label) : nullptr;
		return door ? door->GetScript() : nullptr;
	}
	if (category == "elevator") {
		Map* map = framework ? framework->GetMap() : nullptr;
		Cabin* cabin = map ? map->FindCabinByName(label) : nullptr;
		return cabin ? cabin->GetScript() : nullptr;
	}
	// 建筑/房间/园区：label写名字(实体脚本的$$self.name)或地址($$self.address)都行。
	if (category == "zone") {
		Map* map = framework ? framework->GetMap() : nullptr;
		Zone* zone = map ? map->GetZone(label) : nullptr;
		if (!zone && map) zone = map->LocateZone(label);
		return zone ? zone->GetScript() : nullptr;
	}
	if (category == "building") {
		Map* map = framework ? framework->GetMap() : nullptr;
		Building* building = map ? map->GetBuilding(label) : nullptr;
		if (!building && map) building = map->LocateBuilding(label);
		return building ? building->GetScript() : nullptr;
	}
	if (category == "room") {
		Map* map = framework ? framework->GetMap() : nullptr;
		Room* room = map ? map->LocateRoom(label) : nullptr;
		if (!room && map) {
			for (auto& [buildingName, building] : map->GetBuildings()) {
				if (!building) continue;
				for (Room* candidate : building->GetRooms()) {
					if (candidate && candidate->GetName() == label) { room = candidate; break; }
				}
				if (room) break;
			}
		}
		return room ? room->GetScript() : nullptr;
	}
	UE_LOG(LogTemp, Warning, TEXT("create_timer: unknown category \"%s\" (global/citizen/vehicle/door/elevator/zone/building/room, or omit for the script itself)."),
		UTF8_TO_TCHAR(category.c_str()));
	return nullptr;
}

void UForeverStoryFrameworkComponent::ApplyCreateTimer(const CreateTimerChange* change, const ScriptContext& context) {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Player* player = framework ? framework->GetPlayer() : nullptr;
	if (!story || !player || !player->GetTime()) return;

	string name = ToString(EvaluateExpression(change->GetName(), context));
	string timeText = ToString(EvaluateExpression(change->GetTime(), context));
	string label = ToString(EvaluateExpression(change->GetLabel(), context));

	string ownerKey = FindScriptKey(ResolveTimerOwner(change->GetCategory(), label, context.self));
	if (ownerKey.empty()) {
		UE_LOG(LogTemp, Warning, TEXT("create_timer \"%s\": target script not found (category=\"%s\", label=\"%s\")."),
			UTF8_TO_TCHAR(name.c_str()), UTF8_TO_TCHAR(change->GetCategory().c_str()), UTF8_TO_TCHAR(label.c_str()));
		return;
	}

	// time两种写法：带日期(如"2024-05-01 08:00:00")是绝对时刻；只有时间("08:00:00")是下一次到这个
	// 钟点——今天还没过就是今天，已经过了(含正好等于现在)就是明天，和老工程一致。
	Time parsed;
	try {
		parsed = Time(timeText);
	}
	catch (const ExceptionBase&) {
		UE_LOG(LogTemp, Warning, TEXT("create_timer \"%s\": cannot parse time \"%s\"."),
			UTF8_TO_TCHAR(name.c_str()), UTF8_TO_TCHAR(timeText.c_str()));
		return;
	}
	const Time& now = *player->GetTime();
	Time target = parsed;
	if (!parsed.IsValid()) {
		target = now;
		target.SetTime(parsed.GetHour(), parsed.GetMinute(), parsed.GetSecond(), parsed.GetMillisecond());
		if (target <= now) target.AddDays(1);
	}
	story->CreateTimer(name, target, ownerKey);
}

void UForeverStoryFrameworkComponent::ApplyRemoveTimer(const RemoveTimerChange* change, const ScriptContext& context) {
	if (!story) return;
	// 只删执行这个change的脚本自己名下的计时器(不同脚本可以有同名计时器)。
	string ownerKey = FindScriptKey(context.self);
	if (ownerKey.empty()) return;
	story->RemoveTimer(ToString(EvaluateExpression(change->GetName(), context)), ownerKey);
}

void UForeverStoryFrameworkComponent::FireExpiredTimers() {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Player* player = framework ? framework->GetPlayer() : nullptr;
	if (!story || !player || !player->GetTime()) return;

	vector<StoryTimer> expired = story->PopExpiredTimers(*player->GetTime());
	if (expired.empty()) return;

	PostImplement postImplement(
		framework->GetMap(), framework->GetPopulace(), framework->GetSociety(), story,
		framework->GetIndustry(), framework->GetTraffic(), player);

	for (const StoryTimer& timer : expired) {
		// 只发给计时器的归属脚本，不广播。归属对象已经不在了就丢弃。
		Script* script = FindScriptByKey(timer.ownerKey);
		if (!script) continue;
		TimeUpEvent event(timer.name);
		ScriptContext context;
		context.self = script;
		context.system = story->GetSystemScript();
		context.global = story->GetGlobalScript();
		context.local = &event;
		ProcessScriptActions(framework, script->MatchEvent(&event, context, &postImplement), context);
	}
}

void UForeverStoryFrameworkComponent::ShowCurrentBackground() {
	AForeverPlayerController* playerController = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	UBackgroundSpeakingWidget* widget = playerController ? playerController->GetBackgroundSpeakingWidget() : nullptr;

	if (backgroundQueue.Num() == 0) {
		if (widget) widget->HideLine();
		return;
	}

	// 不碰输入模式/鼠标/MeetOption——后台对话不抢操控。控件没配置时照样计时推进队列。
	const FPendingItem& item = backgroundQueue[0];
	if (widget) widget->ShowLine(item.speaker, item.content);
	// 世界计时器跟随时间缩放：暂停(时间缩放为0)期间计时冻结。
	if (UWorld* world = GetWorld()) {
		world->GetTimerManager().SetTimer(backgroundTimer, this, &UForeverStoryFrameworkComponent::AdvanceBackground,
			item.background, false);
	}
}

void UForeverStoryFrameworkComponent::AdvanceBackground() {
	if (backgroundQueue.Num() == 0) return;

	std::string finishedLabel = backgroundQueue[0].label;
	backgroundQueue.RemoveAt(0);
	// 先显示下一条再广播：广播命中的milestone弹出的新对话会走ShowDialog->ShowCurrentItem，可能
	// 往backgroundQueue里追加后台台词，这时后台已经在播(或确实空闲)，不会重复启动计时。
	ShowCurrentBackground();
	if (!finishedLabel.empty()) BroadcastSpeakingFinish(finishedLabel);
}

void UForeverStoryFrameworkComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	if (UWorld* world = GetWorld()) world->GetTimerManager().ClearTimer(backgroundTimer);
	backgroundQueue.Reset();
	Super::EndPlay(EndPlayReason);
}

void UForeverStoryFrameworkComponent::BroadcastSpeakingFinish(const string& label) {
	if (!story) return;

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	PostImplement postImplement(
		framework ? framework->GetMap() : nullptr,
		framework ? framework->GetPopulace() : nullptr,
		framework ? framework->GetSociety() : nullptr,
		story,
		framework ? framework->GetIndustry() : nullptr,
		framework ? framework->GetTraffic() : nullptr,
		framework ? framework->GetPlayer() : nullptr);

	SpeakingFinishEvent event(label);
	BroadcastToAllScripts(event, postImplement, /*includeMain=*/true);
}

void UForeverStoryFrameworkComponent::SelectDialogOption(int32 index) {
	if (pendingQueue.Num() == 0 || !pendingQueue[0].bBranch) return;

	// 值拷贝一份再RemoveAt——selected是对这份拷贝里的Option取引用，RemoveAt(0)之后原数组
	// 元素的生命周期就跟这次调用无关了。
	FPendingItem current = pendingQueue[0];
	if (!current.options.IsValidIndex(index)) return;

	pendingQueue.RemoveAt(0);

	const Option& selected = current.options[index];

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	if (framework) {
		for (const Change* change : selected.GetChanges()) {
			framework->ApplyChange(change, current.context);
		}
	}

	// 选中后追加的对话应该紧接着这个分支之后立刻播放，插到队头而不是排到队尾；多个嵌套
	// Dialog之间要保持相对顺序，insertCursor随每次插入新增的条目数往后走。
	int32 insertCursor = 0;
	for (Dialog* nested : selected.GetDialogs()) {
		int32 before = pendingQueue.Num();
		EnqueueDialog(nested, current.context, insertCursor);
		insertCursor += pendingQueue.Num() - before;
	}

	ShowCurrentItem();
}

void UForeverStoryFrameworkComponent::ShowCurrentItem() {
	// 队首的后台台词放行：它能排到队首，说明它前面的前台条目都播完了(后台等前台)；转走后马上
	// 显示下一条前台条目(前台不等后台)。
	bool backgroundWasIdle = backgroundQueue.Num() == 0;
	while (pendingQueue.Num() > 0 && !pendingQueue[0].bBranch && pendingQueue[0].background > 0.f) {
		backgroundQueue.Add(pendingQueue[0]);
		pendingQueue.RemoveAt(0);
	}
	if (backgroundWasIdle && backgroundQueue.Num() > 0) ShowCurrentBackground();

	AForeverPlayerController* playerController = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (!playerController) return;

	USectionSpeakingWidget* speakingWidget = playerController->GetSectionSpeakingWidget();
	USectionOptionWidget* optionWidget = playerController->GetSectionOptionWidget();
	UMeetOptionWidget* meetOptionWidget = playerController->GetMeetOptionWidget();

	if (pendingQueue.Num() == 0) {
		if (speakingWidget) speakingWidget->HideLine();
		if (optionWidget) optionWidget->HideOptions();
		// 对话结束，重新露出MeetOption（Visible是AddToViewport()之后的默认可见状态）——
		// 见下面"对话展示期间"分支里把它藏起来的说明。
		if (meetOptionWidget) meetOptionWidget->SetVisibility(ESlateVisibility::Visible);

		// 对话播完，恢复正常的游戏内输入。光把bShowMouseCursor/SetInputMode改回去还不够——
		// UIOnly期间点SectionButton/ClickCatcher这次鼠标点击本身会把Slate的键盘/鼠标焦点
		// 转移到那个Button上，SetInputMode(GameOnly)并不会主动抢回来，停留在UI焦点上的话
		// 即使鼠标已经隐藏，Look这个Enhanced Input Action也收不到鼠标移动事件（对话关闭后
		// 鼠标不消失、也没法转视角）。必须再调用一次SetFocusToGameViewport()把
		// 焦点抢回游戏视口。
		playerController->bShowMouseCursor = false;
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(playerController);
		UWidgetBlueprintLibrary::SetFocusToGameViewport();
		return;
	}

	// 同时把MeetOption藏起来——对话（不管是台词还是分支）展示期间，"靠近市民能问什么"这个
	// 列表不应该跟剧情对话同屏抢注意力/抢点击，见老蓝图SectionButton.SelectOption分支里
	// "选完把MeetOption.SetVisibility(Visible)调回来"那段逻辑（.dump/SelectionButton.txt）。
	// 用SetVisibility而不是加一个MeetOptionWidget自己的Show/Hide方法——UUserWidget的
	// SetVisibility本来就是public UFUNCTION，不需要再包一层。
	if (meetOptionWidget) meetOptionWidget->SetVisibility(ESlateVisibility::Collapsed);

	const FPendingItem& item = pendingQueue[0];
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());

	UWidget* widgetToFocus = nullptr;
	if (item.bBranch) {
		if (speakingWidget) speakingWidget->HideLine();
		if (optionWidget) optionWidget->ShowOptions(framework, item.optionTexts);
		widgetToFocus = optionWidget;
	}
	else {
		if (optionWidget) optionWidget->HideOptions();
		if (speakingWidget) speakingWidget->ShowLine(framework, item.speaker, item.content);
		widgetToFocus = speakingWidget;
	}

	// 对话展示期间切UIOnly（仿照老工程SectionButton点击后调SetInputMode_GameOnly、隐含
	// 之前是"游戏动作完全不响应"这个状态，见.dump/SelectionButton.txt）——UIOnly下Pawn的
	// Enhanced Input完全收不到任何输入，Jump/Move/Look/MeetOption的F/滚轮全都天然不会
	// 误触发，不需要像"空格键同时绑Jump和推进对话"那样一个个手动判断"对话是不是在展示"
	// （之前踩过这个坑：GameAndUI下Jump还是会响应，按空格推进对话的同时角色会跳一下）。
	// ClickCatcher/SectionButton的鼠标点击在UIOnly下依然正常工作，跟GameAndUI没有区别；
	// 唯一的代价是SectionSpeaking的空格键推进不能再走Enhanced Input（Pawn收不到输入），
	// 改成SectionSpeaking自己的NativeOnKeyDown原生响应键盘事件，所以这里必须把
	// widgetToFocus指定成当前实际显示的那个Widget，它才能拿到键盘焦点，见
	// USectionSpeakingWidget.h类注释。不判断"是不是刚从空队列进入"，每次都设一遍——
	// 重复设置同一个输入模式/显示鼠标状态没有副作用，比额外维护一个"上一次状态"标记简单。
	playerController->bShowMouseCursor = true;
	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(playerController, widgetToFocus, EMouseLockMode::DoNotLock, false);
}
