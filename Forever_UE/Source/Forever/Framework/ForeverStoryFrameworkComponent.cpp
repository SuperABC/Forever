#include "Framework/ForeverStoryFrameworkComponent.h"

#include "Framework/CoreGuard.h"

#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverPopulaceFrameworkComponent.h"
#include "Framework/ForeverBuildingFrameworkComponent.h"
#include "Framework/ForeverTrafficFrameworkComponent.h"
#include "Framework/ForeverAssetFrameworkComponent.h"
#include "Element/AssetElement.h"
#include "Element/CitizenElement.h"
#include "Player/ForeverPlayerController.h"
#include "Player/ForeverWeaponComponent.h"
#include "Player/ForeverProjectileBridgeComponent.h"
#include "Player/ForeverDamage.h"
#include "GameFramework/Pawn.h"
#include "UI/SectionSpeakingWidget.h"
#include "UI/BackgroundSpeakingWidget.h"
#include "UI/SectionOptionWidget.h"
#include "UI/MeetOptionWidget.h"
#include "UI/PuzzleWidget.h"
#include "UI/ShopWidget.h"

#include "Engine/Engine.h"
#include "Misc/PackageName.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

#include <regex>

#include "story/story.h"
#include "story/script.h"
#include "story/dialog.h"
#include "story/change.h"
#include "story/event.h"
#include "society/society.h"
#include "society/organization.h"
#include "society/job.h"
#include "society/economy.h"
#include "populace/populace.h"
#include "populace/citizen.h"
#include "populace/scheduler.h"
#include "traffic/traffic.h"
#include "traffic/vehicle.h"
#include "player/asset.h"
#include "player/weapon.h"
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
	// 所有广播入口共用的"Dialog交给ShowDialog展示/Change转发给ApplyChange"处理。不进头文件：
	// ScriptAction是std::variant，没法前置声明，进头文件要多#include一个story/script_mod.h。
	void ProcessScriptActions(AForeverFrameworkActor* framework, const vector<ScriptAction>& actions,
		const ScriptContext& context) {
		for (const auto& action : actions) {
			if (auto dialogPtr = get_if<const Dialog*>(&action)) {
				// 交给ShowDialog统一展示/排队。
				// 一段对话也是一个可以整体跳过的操作：对话里的数据错误(表达式写错)只丢掉这一段。
				if (framework && framework->GetStoryFramework()) {
					ErrorReport report;
					if (!GuardData([&]() {
						ErrorScope dialogScope(kErrorScopeScript, "dialog");
						framework->GetStoryFramework()->ShowDialog(*dialogPtr, context);
					}, report)) {
						UE_LOG(LogTemp, Warning, TEXT("一段对话无法显示，已跳过：%s"), UTF8_TO_TCHAR(report.ToLine().c_str()));
					}
				}
			}
			else if (auto changePtr = get_if<const Change*>(&action)) {
				// 统一交给AForeverFrameworkActor::ApplyChange消费，见ForeverFrameworkActor.md"统一的Change消费入口"。
				if (framework) framework->ApplyChange(*changePtr, context);
			}
		}
	}
}

void UForeverStoryFrameworkComponent::Init(Story* inStory) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::Init")
	story = inStory;
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::UpdateSystemTime() {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::UpdateSystemTime")
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
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::BroadcastGameStart() {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::BroadcastGameStart")
	if (!story || !GEngine) return;

	// 现场构造一个PostImplement，供WrapScript通过Post()查询Core状态(如"random citizen"，见Core/common/implement.md)，
	// 生命周期只覆盖这次广播。
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
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::BroadcastToScript(Event& event, Script* script, PostImplement& postImplement) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::BroadcastToScript")
	if (!story || !script) return;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	ScriptContext context;
	context.self = script;
	context.system = story->GetSystemScript();
	context.global = story->GetGlobalScript();
	context.local = &event;
	ProcessScriptActions(framework, script->MatchEvent(&event, context, &postImplement), context);
	CORE_GUARD_END
}

namespace {
	// global_message的category：接收方脚本类别的位掩码。空=全部。
	enum ScriptKind : uint32 {
		KindMain = 1 << 0, KindOrganization = 1 << 1, KindJob = 1 << 2, KindCitizen = 1 << 3, KindVehicle = 1 << 4,
		KindDoor = 1 << 5, KindElevator = 1 << 6, KindBuilding = 1 << 7, KindRoom = 1 << 8, KindZone = 1 << 9,
		KindAsset = 1 << 10, KindAll = 0xFFFFFFFFu
	};

	uint32 ParseScriptKinds(const string& category) {
		if (category.empty()) return KindAll;
		static const std::pair<const char*, uint32> kNames[] = {
			{ "main", KindMain }, { "organization", KindOrganization }, { "job", KindJob }, { "citizen", KindCitizen },
			{ "vehicle", KindVehicle }, { "door", KindDoor }, { "elevator", KindElevator }, { "building", KindBuilding },
			{ "room", KindRoom }, { "zone", KindZone }, { "asset", KindAsset } };
		uint32 mask = 0;
		size_t start = 0;
		while (start <= category.size()) {
			size_t comma = category.find(',', start);
			const string item = category.substr(start, comma == string::npos ? string::npos : comma - start);
			for (const auto& [name, bit] : kNames) if (item == name) mask |= bit;
			if (comma == string::npos) break;
			start = comma + 1;
		}
		return mask;
	}

	// 玩家身上(手/肩/背)、estate、地上(worldAssets)、全部房间里的资产，含容器内容，摊平成数组。
	void CollectAllAssets(AForeverFrameworkActor* framework, TArray<Asset*>& out) {
		auto add = [&](auto&& self, Asset* asset) -> void {
			if (!asset) return;
			out.Add(asset);
			for (auto& [childName, child] : asset->GetContents()) self(self, child);
		};
		if (Player* player = framework ? framework->GetPlayer() : nullptr) {
			for (Asset* slot : { player->GetLeftHand(), player->GetRightHand(), player->GetBackPack(),
				player->GetLeftShoulder(), player->GetRightShoulder() }) {
				add(add, slot);
			}
			for (auto& [name, world] : player->GetWorldAssets()) add(add, world.asset);
		}
		if (Map* map = framework ? framework->GetMap() : nullptr) {
			for (auto& [buildingName, building] : map->GetBuildings()) {
				if (!building) continue;
				for (Room* room : building->GetRooms()) {
					if (!room) continue;
					for (auto& [assetName, asset] : room->GetAssets()) add(add, asset);
				}
			}
		}
	}
}

void UForeverStoryFrameworkComponent::BroadcastToAllScripts(Event& event, PostImplement& postImplement, bool includeMain, const string& category) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::BroadcastToAllScripts")
	if (!story) return;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());

	auto broadcastOne = [&](Script* script) {
		BroadcastToScript(event, script, postImplement);
		};

	// 只遍历category指定的那几类脚本(几千个岗位/日程/房间脚本全遍历一遍很慢)；空=全部。
	const uint32 kinds = ParseScriptKinds(category);

	if (includeMain && (kinds & KindMain)) broadcastOne(story->GetMainScript());

	if ((kinds & (KindOrganization | KindJob)) != 0) {
		if (Society* society = framework ? framework->GetSociety() : nullptr) {
			for (Organization* org : society->GetOrganizations()) {
				if (kinds & KindOrganization) broadcastOne(org->GetScript());
				if (kinds & KindJob) {
					for (Job* job : org->GetJobs()) {
						broadcastOne(job->GetScript());
					}
				}
			}
		}
	}

	// 每个市民独占持有一份Scheduler，Scheduler自己的Script(schedule_empty.script)。
	if (Populace* populace = (kinds & KindCitizen) ? (framework ? framework->GetPopulace() : nullptr) : nullptr) {
		for (Citizen* citizen : populace->GetCitizens()) {
			if (citizen && citizen->GetScheduler()) {
				broadcastOne(citizen->GetScheduler()->GetScript());
			}
		}
	}

	// 每辆车独占持有一份Script(vehicle_basic.script)。
	if (Traffic* traffic = (kinds & KindVehicle) ? (framework ? framework->GetTraffic() : nullptr) : nullptr) {
		for (auto& [name, vehicle] : traffic->GetVehicles()) {
			if (vehicle) broadcastOne(vehicle->GetScript());
		}
	}

	// 只有spec.name非空的门才会有Script(见Core/map/door.md)，过滤掉纯自动门；电梯同理(spec指定了
	// 里程碑文件才有Script)。
	const bool wantBuildings = (kinds & (KindElevator | KindBuilding | KindRoom)) != 0;
	if (Map* map = (kinds & (KindDoor | KindElevator | KindBuilding | KindRoom | KindZone)) ? (framework ? framework->GetMap() : nullptr) : nullptr) {
		if (kinds & KindDoor) {
			for (Door* door : map->GetAllDoors()) {
				if (door && door->GetScript()) broadcastOne(door->GetScript());
			}
		}
		for (auto& [name, building] : map->GetBuildings()) {
			if (!wantBuildings) break;
			if (!building) continue;
			if (kinds & KindElevator) {
				for (Cabin* cabin : building->GetCabinEntities()) {
					if (cabin && cabin->GetScript()) broadcastOne(cabin->GetScript());
				}
			}
			// 实体脚本(建筑/房间/园区)：收game_start、计时器等普通广播；它们的进出事件不走这里，
			// 只由BroadcastPlayerLocation发给被进出的那一个。
			if (kinds & KindBuilding) broadcastOne(building->GetScript());
			if (kinds & KindRoom) {
				for (Room* room : building->GetRooms()) {
					if (room) broadcastOne(room->GetScript());
				}
			}
		}
		if (kinds & KindZone) {
			for (auto& [name, zone] : map->GetZones()) {
				if (zone) broadcastOne(zone->GetScript());
			}
		}
	}

	// 资产(玩家身上/地上/房间里，含容器内容)带的脚本。game_start同时置scriptStarted，之后新建的资产
	// 在StartAssetScript里补发。
	TArray<Asset*> assets;
	if (kinds & KindAsset) CollectAllAssets(framework, assets);
	bool bGameStart = dynamic_cast<GameStartEvent*>(&event) != nullptr;
	for (Asset* asset : assets) {
		if (!asset->GetScript()) continue;
		if (bGameStart) {
			if (asset->IsScriptStarted()) continue;
			asset->SetScriptStarted(true);
		}
		broadcastOne(asset->GetScript());
	}
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::StartAssetScript(Asset* asset) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::StartAssetScript")
	if (!story || !asset) return;
	if (asset->GetScript() && !asset->IsScriptStarted()) {
		asset->SetScriptStarted(true);
		AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
		PostImplement postImplement(
			framework ? framework->GetMap() : nullptr,
			framework ? framework->GetPopulace() : nullptr,
			framework ? framework->GetSociety() : nullptr,
			story,
			framework ? framework->GetIndustry() : nullptr,
			framework ? framework->GetTraffic() : nullptr,
			framework ? framework->GetPlayer() : nullptr);
		GameStartEvent event;
		BroadcastToScript(event, asset->GetScript(), postImplement);
	}
	// 容器里的东西各有各的脚本。先拷贝名字列表：脚本里的Change可能改动容器。
	TArray<Asset*> children;
	for (auto& [childName, child] : asset->GetContents()) children.Add(child);
	for (Asset* child : children) StartAssetScript(child);
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::NotifyAssetEvent(Asset* root, bool bGet, const string& result, Script* sender) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::NotifyAssetEvent")
	if (!story || !root) return;
	if (assetEventDepth >= kMaxAssetEventDepth) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverStoryFrameworkComponent::NotifyAssetEvent: nested asset event depth exceeded %d, dropped (do two scripts keep triggering each other?)"),
			kMaxAssetEventDepth);
		return;
	}

	// 先把整棵树摊平成快照再发：事件处理里的Change可能改动容器，边遍历边发不安全。
	struct FNode { string type; string name; Script* ownScript; };
	TArray<FNode> nodes;
	auto flatten = [&](auto&& self, Asset* asset) -> void {
		nodes.Add({ asset->GetType(), asset->GetName(), asset->GetScript() });
		for (auto& [childName, child] : asset->GetContents()) self(self, child);
	};
	flatten(flatten, root);

	assetEventDepth++;
	for (const FNode& node : nodes) {
		// 测试用：屏幕左上角显示。
		if (GEngine) {
			FString text = FString::Printf(TEXT("%s %s%s"), bGet ? TEXT("获取") : TEXT("失去"),
				UTF8_TO_TCHAR(node.name.c_str()), result == "success" ? TEXT("") : TEXT(" 失败"));
			GEngine->AddOnScreenDebugMessage(-1, 5.f, result == "success" ? FColor::Green : FColor::Red, text);
		}
		NotifyAssetResult(bGet, node.type, result, sender ? sender : node.ownScript);
	}
	assetEventDepth--;
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::NotifyThrowAsset(Asset* asset, const string& thrower) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::NotifyThrowAsset")
	if (!story || !asset || !asset->GetScript()) return;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	PostImplement postImplement(
		framework ? framework->GetMap() : nullptr,
		framework ? framework->GetPopulace() : nullptr,
		framework ? framework->GetSociety() : nullptr,
		story,
		framework ? framework->GetIndustry() : nullptr,
		framework ? framework->GetTraffic() : nullptr,
		framework ? framework->GetPlayer() : nullptr);
	ThrowAssetEvent event(asset->GetType(), thrower);
	BroadcastToScript(event, asset->GetScript(), postImplement);
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::NotifyUseAsset(Asset* asset, const string& user) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::NotifyUseAsset")
	if (!story || !asset || !asset->GetScript()) return;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	PostImplement postImplement(
		framework ? framework->GetMap() : nullptr,
		framework ? framework->GetPopulace() : nullptr,
		framework ? framework->GetSociety() : nullptr,
		story,
		framework ? framework->GetIndustry() : nullptr,
		framework ? framework->GetTraffic() : nullptr,
		framework ? framework->GetPlayer() : nullptr);
	UseAssetEvent event(asset->GetName(), asset->GetType(), user);
	BroadcastToScript(event, asset->GetScript(), postImplement);
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::NotifyWeaponFire(Weapon* weapon, const FWeaponFireInfo& info) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::NotifyWeaponFire")
	if (!story || !weapon || !weapon->GetScript()) return;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	PostImplement postImplement(
		framework ? framework->GetMap() : nullptr,
		framework ? framework->GetPopulace() : nullptr,
		framework ? framework->GetSociety() : nullptr,
		story,
		framework ? framework->GetIndustry() : nullptr,
		framework ? framework->GetTraffic() : nullptr,
		framework ? framework->GetPlayer() : nullptr);
	WeaponFireEvent event(weapon->GetType());
	event.SetField("shooter", ValueType(info.shooter));
	event.SetField("sufferer", ValueType(info.sufferer));
	event.SetField("part", ValueType(info.part));
	event.SetField("distance", ValueType(static_cast<double>(info.distance)));
	event.SetField("pellet", ValueType(static_cast<double>(info.pellet)));
	event.SetVec3("asset", info.asset.X, info.asset.Y, info.asset.Z);
	event.SetVec3("fire", info.fire.X, info.fire.Y, info.fire.Z);
	event.SetVec3("hit", info.hit.X, info.hit.Y, info.hit.Z);
	event.SetVec3("direction", info.direction.X, info.direction.Y, info.direction.Z);
	event.SetVec3("normal", info.normal.X, info.normal.Y, info.normal.Z);
	BroadcastToScript(event, weapon->GetScript(), postImplement);
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::NotifyProjectileHit(const string& ownerKey, const FVector& hit, const FVector& normal,
	const string& sufferer) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::NotifyProjectileHit")
	if (!story || ownerKey.empty()) return;
	Script* owner = FindScriptByKey(ownerKey);
	if (!owner) return;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	PostImplement postImplement(
		framework ? framework->GetMap() : nullptr,
		framework ? framework->GetPopulace() : nullptr,
		framework ? framework->GetSociety() : nullptr,
		story,
		framework ? framework->GetIndustry() : nullptr,
		framework ? framework->GetTraffic() : nullptr,
		framework ? framework->GetPlayer() : nullptr);
	ProjectileHitEvent event;
	event.SetField("sufferer", ValueType(sufferer));
	event.SetVec3("hit", hit.X, hit.Y, hit.Z);
	event.SetVec3("normal", normal.X, normal.Y, normal.Z);
	BroadcastToScript(event, owner, postImplement);
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::NotifyAssetResult(bool bGet, const string& type, const string& result, Script* sender) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::NotifyAssetResult")
	if (!story || !sender) return;
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	PostImplement postImplement(
		framework ? framework->GetMap() : nullptr,
		framework ? framework->GetPopulace() : nullptr,
		framework ? framework->GetSociety() : nullptr,
		story,
		framework ? framework->GetIndustry() : nullptr,
		framework ? framework->GetTraffic() : nullptr,
		framework ? framework->GetPlayer() : nullptr);
	if (bGet) {
		GetAssetEvent event(type, result);
		BroadcastToScript(event, sender, postImplement);
	}
	else {
		LoseAssetEvent event(type, result);
		BroadcastToScript(event, sender, postImplement);
	}
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::BroadcastGlobalMessage(const string& message, const string& category) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::BroadcastGlobalMessage")
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
	BroadcastToAllScripts(event, postImplement, /*includeMain=*/true, category);
	globalMessageDepth--;
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::BroadcastWeatherChange(const string& weather) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::BroadcastWeatherChange")
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

	WeatherChangeEvent event(weather);
	BroadcastToAllScripts(event, postImplement, /*includeMain=*/true);
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::OptionDialog(const FString& name, const FString& option) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::OptionDialog")
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
	// 岗位的Job脚本：工作中的市民在Job脚本里加选项(见job_factory_worker.script/job_shop_saler.script)，
	// 点击事件也要发给它。
	if (target && target->GetJob()) {
		matchOne(target->GetJob()->GetScript());
	}

	// name也可能是一辆车(见vehicle_basic.script的"上车"选项)，额外匹配它自己的Script，和其它分支并列。
	Traffic* traffic = framework ? framework->GetTraffic() : nullptr;
	Vehicle* targetVehicle = traffic ? traffic->FindVehicleByName(nameUtf8) : nullptr;
	if (targetVehicle && targetVehicle->GetScript()) {
		matchOne(targetVehicle->GetScript());
	}

	// name也可能是一扇门的交互名(见door_shop_entrance.script的"敲门"选项)，额外匹配它的Script。
	Map* map = framework ? framework->GetMap() : nullptr;
	Door* targetDoor = map ? map->FindDoorByName(nameUtf8) : nullptr;
	if (targetDoor && targetDoor->GetScript()) {
		matchOne(targetDoor->GetScript());
	}

	// 资产：name是场景里一个带脚本的资产的名字(靠近它弹出的选项)。
	if (UForeverAssetFrameworkComponent* assetFramework = framework ? framework->GetAssetFramework() : nullptr) {
		if (AAssetElement* element = assetFramework->FindElementByName(name)) {
			if (Asset* asset = element->GetAsset()) matchOne(asset->GetScript());
		}
	}

	// 电梯：name是电梯的交互名("<building名> 电梯<n>")。
	Cabin* targetCabin = map ? map->FindCabinByName(nameUtf8) : nullptr;
	if (targetCabin && targetCabin->GetScript()) {
		matchOne(targetCabin->GetScript());
	}
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::BroadcastNpcArrive(Citizen* citizen, Room* room) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::BroadcastNpcArrive")
	if (!room) return;
	BroadcastNpcArrive(citizen, room->GetAddress());
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::BroadcastNpcArrive(Citizen* citizen, const string& address) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::BroadcastNpcArrive")
	if (!story || !GEngine || !citizen || address.empty()) return;
	if (npcArriveDepth >= kMaxNpcArriveDepth) {
		UE_LOG(LogTemp, Warning, TEXT("npc_arrive nesting too deep, dropped."));
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

	NpcArriveEvent event(citizen->GetName(), address);
	npcArriveDepth++;
	if (citizen->GetScheduler()) BroadcastToScript(event, citizen->GetScheduler()->GetScript(), postImplement);
	if (citizen->GetJob()) BroadcastToScript(event, citizen->GetJob()->GetScript(), postImplement);
	npcArriveDepth--;
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::BroadcastMeeting(Citizen* citizen, bool bStart, const string& party, const string& address) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::BroadcastMeeting")
	if (!story || !GEngine || !citizen || !citizen->GetScheduler()) return;

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	PostImplement postImplement(
		framework ? framework->GetMap() : nullptr,
		framework ? framework->GetPopulace() : nullptr,
		framework ? framework->GetSociety() : nullptr,
		story,
		framework ? framework->GetIndustry() : nullptr,
		framework ? framework->GetTraffic() : nullptr,
		framework ? framework->GetPlayer() : nullptr);

	// 家的地址随事件带给脚本($$local.home)：散会后npc_navigate回家用，脚本里没有"家"这个关键字。
	const string home = citizen->GetRoom() ? citizen->GetRoom()->GetAddress() : string();
	if (bStart) {
		MeetingStartEvent event(party, address, home);
		BroadcastToScript(event, citizen->GetScheduler()->GetScript(), postImplement);
	}
	else {
		MeetingEndEvent event(party, address, home);
		BroadcastToScript(event, citizen->GetScheduler()->GetScript(), postImplement);
	}
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::BroadcastExecuteTask(Citizen* citizen, const string& task, const string& destination) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::BroadcastExecuteTask")
	if (!story || !GEngine || !citizen || !citizen->GetJob()) return;

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	PostImplement postImplement(
		framework ? framework->GetMap() : nullptr,
		framework ? framework->GetPopulace() : nullptr,
		framework ? framework->GetSociety() : nullptr,
		story,
		framework ? framework->GetIndustry() : nullptr,
		framework ? framework->GetTraffic() : nullptr,
		framework ? framework->GetPlayer() : nullptr);

	// 只发给被派任务的员工自己的Job脚本。
	ExecuteTaskEvent event(task, destination);
	BroadcastToScript(event, citizen->GetJob()->GetScript(), postImplement);
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::BroadcastTaskComplete(Organization* organization, const string& task, const string& destination,
	const string& worker, const string& origin) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::BroadcastTaskComplete")
	if (!story || !GEngine || !organization || !organization->GetScript()) return;

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	PostImplement postImplement(
		framework ? framework->GetMap() : nullptr,
		framework ? framework->GetPopulace() : nullptr,
		framework ? framework->GetSociety() : nullptr,
		story,
		framework ? framework->GetIndustry() : nullptr,
		framework ? framework->GetTraffic() : nullptr,
		framework ? framework->GetPlayer() : nullptr);

	// 只发给派任务的组织自己的脚本。
	TaskCompleteEvent event(task, destination, worker, origin);
	BroadcastToScript(event, organization->GetScript(), postImplement);
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::BroadcastTaskFailed(Organization* organization, const string& task, const string& destination,
	const string& worker, const string& origin, const string& reason) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::BroadcastTaskFailed")
	if (!story || !GEngine || !organization || !organization->GetScript()) return;

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	PostImplement postImplement(
		framework ? framework->GetMap() : nullptr,
		framework ? framework->GetPopulace() : nullptr,
		framework ? framework->GetSociety() : nullptr,
		story,
		framework ? framework->GetIndustry() : nullptr,
		framework ? framework->GetTraffic() : nullptr,
		framework ? framework->GetPlayer() : nullptr);

	TaskFailedEvent event(task, destination, worker, origin, reason);
	BroadcastToScript(event, organization->GetScript(), postImplement);
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::BroadcastCitizenDecease(const FString& name, const FString& reason) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::BroadcastCitizenDecease")
	CitizenDeceaseEvent event(string(TCHAR_TO_UTF8(*name)), string(TCHAR_TO_UTF8(*reason)));
	BroadcastCitizenDecease(event);
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::BroadcastCitizenDecease(CitizenDeceaseEvent& event) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::BroadcastCitizenDecease")
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

	const string nameUtf8 = event.GetName();

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
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::BroadcastCitizenInjured(CitizenInjuredEvent& event) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::BroadcastCitizenInjured")
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

	// 和citizen_decease一样：主线脚本 + 受伤者自己的Scheduler脚本(按name找)。
	if (story->GetMainScript()) BroadcastToScript(event, story->GetMainScript(), postImplement);
	Populace* populace = framework ? framework->GetPopulace() : nullptr;
	Citizen* target = populace ? populace->FindCitizenByName(event.GetName()) : nullptr;
	if (target && target->GetScheduler() && target->GetScheduler()->GetScript()) {
		BroadcastToScript(event, target->GetScheduler()->GetScript(), postImplement);
	}
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::BroadcastFleeStart(const string& name, const string& from, const string& destination) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::BroadcastFleeStart")
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
	Populace* populace = framework ? framework->GetPopulace() : nullptr;
	Citizen* target = populace ? populace->FindCitizenByName(name) : nullptr;
	if (!target || !target->GetScheduler() || !target->GetScheduler()->GetScript()) return;
	FleeStartEvent event(name, from, destination);
	BroadcastToScript(event, target->GetScheduler()->GetScript(), postImplement);
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::BroadcastFleeEnd(const string& name, const string& from, const string& reason) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::BroadcastFleeEnd")
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
	Populace* populace = framework ? framework->GetPopulace() : nullptr;
	Citizen* target = populace ? populace->FindCitizenByName(name) : nullptr;
	if (!target || !target->GetScheduler() || !target->GetScheduler()->GetScript()) return;
	FleeEndEvent event(name, from, reason);
	BroadcastToScript(event, target->GetScheduler()->GetScript(), postImplement);
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::ApplyControlChange(const ChangeControlChange* change, const ScriptContext& context) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::ApplyControlChange")
	FString name = UTF8_TO_TCHAR(ToString(EvaluateExpression(change->GetName(), context)).data());

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	UForeverPopulaceFrameworkComponent* populaceFramework = framework ? framework->GetPopulaceFramework() : nullptr;
	if (!populaceFramework) return;

	ACitizenElement* target = populaceFramework->FindOrSpawnCitizenByName(name);
	if (!target) {
		debugf(DebugLevel::Warning, "ChangeControlChange target citizen not found: %s.\n", TCHAR_TO_UTF8(*name));
		return;
	}

	if (APlayerController* playerController = UGameplayStatics::GetPlayerController(GetWorld(), 0)) {
		playerController->Possess(target);
	}
	// 被玩家操控的市民才实例化动态属性(饱食度)，放手后保持实例化，见Citizen::ActivateStats。
	if (Citizen* controlled = target->GetCitizen()) controlled->ActivateStats();

	// 切换控制权时建筑近处LOD可能还没建完(如游戏刚开始)，玩家会先掉到没有细节的地面上：
	// 请求冻结世界直到LOD切换队列清空，见UForeverBuildingFrameworkComponent::RequestFreezeUntilLodSettled。
	if (framework && framework->GetBuildingFramework()) {
		framework->GetBuildingFramework()->RequestFreezeUntilLodSettled();
	}
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::ApplyStartPuzzle(const StartPuzzleChange* change, const ScriptContext& context) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::ApplyStartPuzzle")
	FString puzzleId = UTF8_TO_TCHAR(ToString(EvaluateExpression(change->GetPuzzle(), context)).data());
	// param是剧情脚本给小游戏的启动参数，空串表示脚本没传，不求值
	FString param;
	if (!change->GetParam().empty()) {
		param = UTF8_TO_TCHAR(ToString(EvaluateExpression(change->GetParam(), context)).data());
	}

	AForeverPlayerController* playerController = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (!playerController || !playerController->GetPuzzleWidget()) return;

	puzzleOwnerKey = FindScriptKey(context.self);
	playerController->GetPuzzleWidget()->StartPuzzle(puzzleId, param);
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::NotifyPuzzleFinished(PuzzleResultEvent& event) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::NotifyPuzzleFinished")
	if (!story || puzzleOwnerKey.empty()) return;
	Script* owner = FindScriptByKey(puzzleOwnerKey);
	puzzleOwnerKey.clear();
	if (!owner) return;

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	PostImplement postImplement(
		framework ? framework->GetMap() : nullptr,
		framework ? framework->GetPopulace() : nullptr,
		framework ? framework->GetSociety() : nullptr,
		story,
		framework ? framework->GetIndustry() : nullptr,
		framework ? framework->GetTraffic() : nullptr,
		framework ? framework->GetPlayer() : nullptr);
	BroadcastToScript(event, owner, postImplement);
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::ApplyOpenShop(const ScriptContext& context) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::ApplyOpenShop")
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Society* society = framework ? framework->GetSociety() : nullptr;
	if (!society) return;
	Organization* organization = society->FindOrganizationOfJobScript(context.self);
	if (!organization) {
		UE_LOG(LogTemp, Warning, TEXT("open_shop: the script is not a Job script, ignored."));
		return;
	}

	AForeverPlayerController* playerController = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	if (!playerController || !playerController->GetShopWidget()) return;
	playerController->GetShopWidget()->OpenShop(organization);
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::ApplySpawnActor(const SpawnActorChange* change, const ScriptContext& context) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::ApplySpawnActor")
	UWorld* world = GetWorld();
	if (!world || change->GetActor().empty()) return;

	FString path = UTF8_TO_TCHAR(ToString(EvaluateExpression(change->GetActor(), context)).c_str());
	if (path.IsEmpty()) return;
	// 路径只写包路径时补上蓝图生成类的名字(<名>_C)，同AForeverExplosionEffect::Spawn。
	if (!path.Contains(TEXT("."))) path += TEXT(".") + FPackageName::GetShortName(path) + TEXT("_C");
	UClass* actorClass = LoadClass<AActor>(nullptr, *path);
	if (!actorClass) {
		UE_LOG(LogTemp, Warning, TEXT("spawn_actor: failed to load actor class %s, skipped."), *path);
		return;
	}

	FVector location;
	if (change->GetPosition().empty() ||
		!FForeverDamage::ParseVec3(ToString(EvaluateExpression(change->GetPosition(), context)), location)) {
		UE_LOG(LogTemp, Warning, TEXT("spawn_actor: position missing or invalid, skipped."));
		return;
	}
	FRotator rotation = FRotator::ZeroRotator;
	if (!change->GetDirection().empty()) {
		FVector direction;
		if (FForeverDamage::ParseVec3(ToString(EvaluateExpression(change->GetDirection(), context)), direction) && !direction.IsNearlyZero()) {
			rotation = FRotationMatrix::MakeFromX(direction).Rotator();
		}
	}
	float scale = 1.f;
	if (!change->GetScale().empty()) scale = static_cast<float>(ToDouble(EvaluateExpression(change->GetScale(), context)));
	if (scale <= 0.f) scale = 1.f;

	const FTransform transform(rotation, location, FVector(scale));
	APawn* instigator = UGameplayStatics::GetPlayerPawn(world, 0);
	// 延迟生成：BeginPlay之前先把所有者脚本键/Instigator写进去，Actor的BeginPlay里就能用。
	AActor* actor = world->SpawnActorDeferred<AActor>(actorClass, transform, nullptr, instigator,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!actor) return;
	if (UForeverProjectileBridgeComponent* bridge = actor->FindComponentByClass<UForeverProjectileBridgeComponent>()) {
		bridge->OwnerScriptKey = UTF8_TO_TCHAR(FindScriptKey(context.self).c_str());
	}
	actor->FinishSpawning(transform);
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::ApplyPayWage(const PayWageChange* change, const ScriptContext& context) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::ApplyPayWage")
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Society* society = framework ? framework->GetSociety() : nullptr;
	Citizen* citizen = framework ? framework->GetControlledCitizen() : nullptr;
	if (!society || !citizen) return;
	Organization* organization = society->FindOrganizationOfJobScript(context.self);
	if (!organization) {
		UE_LOG(LogTemp, Warning, TEXT("pay_wage: the script is not a Job script, ignored."));
		return;
	}

	// 表达式写错(比如变量漏了$$前缀被当成字符串去做算术)会抛异常，脚本数据错误不能把游戏带崩。
	int64_t amount = 0;
	try {
		amount = static_cast<int64_t>(ToInt(EvaluateExpression(change->GetAmount(), context)));
	}
	catch (const std::exception& e) {
		UE_LOG(LogTemp, Warning, TEXT("pay_wage: amount expression failed (%s), ignored."), UTF8_TO_TCHAR(e.what()));
		return;
	}
	Economy::PayWage(*society, organization, citizen, amount);
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::ApplyEnterVehicle(const EnterVehicleChange* change, const ScriptContext& context) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::ApplyEnterVehicle")
	FString name = UTF8_TO_TCHAR(ToString(EvaluateExpression(change->GetVehicle(), context)).data());

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	UForeverTrafficFrameworkComponent* trafficFramework = framework ? framework->GetTrafficFramework() : nullptr;
	if (!trafficFramework) return;

	APlayerController* playerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!playerController) return;

	trafficFramework->ApplyEnterVehicle(name, playerController);
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::ApplyBoardVehicle(const BoardVehicleChange* change, const ScriptContext& context) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::ApplyBoardVehicle")
	FString name = UTF8_TO_TCHAR(ToString(EvaluateExpression(change->GetVehicle(), context)).data());

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	UForeverTrafficFrameworkComponent* trafficFramework = framework ? framework->GetTrafficFramework() : nullptr;
	APlayerController* playerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!trafficFramework || !playerController) return;

	trafficFramework->ApplyBoardVehicle(name, playerController);
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::ApplyLeaveVehicle(const LeaveVehicleChange* change, const ScriptContext& context) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::ApplyLeaveVehicle")
	FString name = UTF8_TO_TCHAR(ToString(EvaluateExpression(change->GetVehicle(), context)).data());

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	UForeverTrafficFrameworkComponent* trafficFramework = framework ? framework->GetTrafficFramework() : nullptr;
	APlayerController* playerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!trafficFramework || !playerController) return;

	trafficFramework->ApplyLeaveVehicle(name, playerController, /*bForce=*/false);
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::ApplyForceLeave(const ForceLeaveChange* change, const ScriptContext& context) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::ApplyForceLeave")
	FString name;
	if (!change->GetVehicle().empty()) {
		name = UTF8_TO_TCHAR(ToString(EvaluateExpression(change->GetVehicle(), context)).data());
	}

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	UForeverTrafficFrameworkComponent* trafficFramework = framework ? framework->GetTrafficFramework() : nullptr;
	APlayerController* playerController = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!trafficFramework || !playerController) return;

	trafficFramework->ApplyLeaveVehicle(name, playerController, /*bForce=*/true);
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::BroadcastToVehicle(Event& event, Vehicle* vehicle) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::BroadcastToVehicle")
	if (!story || !vehicle || !vehicle->GetScript()) return;

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	PostImplement postImplement(
		framework ? framework->GetMap() : nullptr,
		framework ? framework->GetPopulace() : nullptr,
		framework ? framework->GetSociety() : nullptr,
		story,
		framework ? framework->GetIndustry() : nullptr,
		framework ? framework->GetTraffic() : nullptr,
		framework ? framework->GetPlayer() : nullptr);

	BroadcastToScript(event, vehicle->GetScript(), postImplement);
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::EnqueueDialog(const Dialog* dialog, const ScriptContext& context, int32 insertIndex) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::EnqueueDialog")
	if (!dialog) return;

	// Dialog自己的condition：非空且求值为假就整段不展示(求值时机是入队的这一刻，所以同一个里程碑里
	// 排在前面的changes已经生效，见Script::MatchEvent先放changes再放dialogs)。
	if (!dialog->GetCondition().empty() && !EvaluateExpressionBool(dialog->GetCondition(), context)) return;

	// self/system指向长期存活的Script*/systemScript，照抄传入的context；local指向调用方栈上的Event，
	// 这次调用一结束就悬垂，强制清空。
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
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::ShowDialog(const Dialog* dialog, const ScriptContext& context) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::ShowDialog")
	bool wasEmpty = pendingQueue.Num() == 0;
	EnqueueDialog(dialog, context, pendingQueue.Num());
	if (wasEmpty) {
		ShowCurrentItem();
	}
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::AdvanceDialog() {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::AdvanceDialog")
	if (pendingQueue.Num() == 0) return;

	std::string finishedLabel = pendingQueue[0].bBranch ? std::string() : pendingQueue[0].label;
	pendingQueue.RemoveAt(0);
	// 先广播再显示下一项：命中的milestone如果又弹出对话，会通过ShowDialog排到队尾(队列空时
	// ShowDialog自己就显示)；下面的ShowCurrentItem重复显示同一项没有副作用。
	if (!finishedLabel.empty()) BroadcastSpeakingFinish(finishedLabel);
	ShowCurrentItem();
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::BroadcastPlayerLocation(Event& event, const string& kind, const string& name, bool entering,
	Script* target) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::BroadcastPlayerLocation")
	if (!story) return;

	if (Script* systemScript = story->GetSystemScript()) {
		string key = "player." + kind;
		if (entering) {
			systemScript->SetValue(key, ValueType(name));
		}
		else {
			const ValueType* current = systemScript->FindValue(key);
			if (current && ToString(*current) == name) systemScript->SetValue(key, ValueType(string()));
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
	CORE_GUARD_END
}

string UForeverStoryFrameworkComponent::FindScriptKey(const Container* script) const {
	if (!script) return string();
	// 脚本自己记着它的稳定键：第一次算出来存进去，之后(对自己建/删计时器、小游戏归属)不做任何查找。
	const Script* self = dynamic_cast<const Script*>(script);
	if (self && !self->GetStableKey().empty()) return self->GetStableKey();
	string key = FindScriptKeySlow(script);
	if (self && !key.empty()) self->SetStableKey(key);
	return key;
}

string UForeverStoryFrameworkComponent::FindScriptKeySlow(const Container* script) const {
	if (!script) return string();
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	if (!framework) return string();
	// 故事的三个固定脚本(main/system/global)直接认，不然下面会先扫全部岗位/市民、再落到CollectScripts(重建并排序全城脚本，
	// 实测每次create_timer/remove_timer 30ms+)。
	if (story) {
		if (script == story->GetMainScript()) return "story.main";
		if (script == story->GetSystemScript()) return "story.system";
		if (script == story->GetGlobalScript()) return "story.global";
	}
	// 命中缓存并核对通过就直接返回(门/房间/建筑/园区/载具/资产等没有直达路径的脚本靠它)。
	if (auto cached = scriptKeyCache.find(script); cached != scriptKeyCache.end()) {
		if (FindScriptByKey(cached->second) == script) return cached->second;
	}
	// 岗位/日程脚本数量大(几千)、timer创建和到时都要来回查，先走不分配的直接扫描，
	// 再退回CollectScripts(它每次都重建并排序全部脚本，几千个timer同时触发时会卡很久)。
	if (Society* society = framework->GetSociety()) {
		const vector<Organization*>& organizations = society->GetOrganizations();
		for (size_t i = 0; i < organizations.size(); i++) {
			Organization* organization = organizations[i];
			if (!organization) continue;
			if (organization->GetScript() == script) return "org:" + to_string(i);
			const vector<Job*>& jobs = organization->GetJobs();
			for (size_t k = 0; k < jobs.size(); k++) {
				if (jobs[k] && jobs[k]->GetScript() == script) return "job:" + to_string(i) + ":" + to_string(k);
			}
		}
	}
	if (Populace* populace = framework->GetPopulace()) {
		const vector<Citizen*>& citizens = populace->GetCitizens();
		for (size_t i = 0; i < citizens.size(); i++) {
			Scheduler* scheduler = citizens[i] ? citizens[i]->GetScheduler() : nullptr;
			if (scheduler && scheduler->GetScript() == script) return "scheduler:" + to_string(i);
		}
	}
	// 最后才整张重建，并把结果全部记进缓存，之后同类脚本都是命中。
	scriptKeyCache.clear();
	string found;
	for (auto& [key, candidate] : ScriptSaveCodec::CollectScripts(framework->GetMap(), framework->GetPopulace(),
		framework->GetSociety(), framework->GetTraffic(), story, framework->GetPlayer())) {
		scriptKeyCache[candidate] = key;
		if (candidate == script) found = key;
	}
	return found;
}

Script* UForeverStoryFrameworkComponent::FindScriptByKey(const string& key) const {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	if (!framework || key.empty()) return nullptr;
	if (story) {
		if (key == "story.main") return story->GetMainScript();
		if (key == "story.system") return story->GetSystemScript();
		if (key == "story.global") return story->GetGlobalScript();
	}
	// 同FindScriptKey：岗位/组织/日程键直接解析，不重建全部脚本(CollectScripts每次都重建并排序全城脚本，
	// 几百个组织的送货计时器同时到时，每个都走它就是每个几十毫秒)。
	if (key.compare(0, 4, "org:") == 0) {
		Society* society = framework->GetSociety();
		if (!society) return nullptr;
		const size_t i = static_cast<size_t>(atoll(key.c_str() + 4));
		const vector<Organization*>& organizations = society->GetOrganizations();
		return i < organizations.size() && organizations[i] ? organizations[i]->GetScript() : nullptr;
	}
	if (key.compare(0, 10, "scheduler:") == 0) {
		Populace* populace = framework->GetPopulace();
		if (!populace) return nullptr;
		const size_t i = static_cast<size_t>(atoll(key.c_str() + 10));
		const vector<Citizen*>& citizens = populace->GetCitizens();
		Scheduler* scheduler = i < citizens.size() && citizens[i] ? citizens[i]->GetScheduler() : nullptr;
		return scheduler ? scheduler->GetScript() : nullptr;
	}
	if (key.compare(0, 4, "job:") == 0) {
		size_t sep = key.find(':', 4);
		if (sep == string::npos) return nullptr;
		Society* society = framework->GetSociety();
		if (!society) return nullptr;
		size_t i = static_cast<size_t>(atoll(key.c_str() + 4)), k = static_cast<size_t>(atoll(key.c_str() + sep + 1));
		const vector<Organization*>& organizations = society->GetOrganizations();
		if (i >= organizations.size() || !organizations[i]) return nullptr;
		const vector<Job*>& jobs = organizations[i]->GetJobs();
		return k < jobs.size() && jobs[k] ? jobs[k]->GetScript() : nullptr;
	}
	// 实体类脚本：按键里的名字/地址/id直接找对象。键的写法必须和CollectScripts一致。
	if (Map* map = framework->GetMap()) {
		if (key.compare(0, 5, "door:") == 0) {
			Door* door = map->FindDoorById(key.substr(5));
			return door ? door->GetScript() : nullptr;
		}
		if (key.compare(0, 6, "cabin:") == 0) {
			Cabin* cabin = map->FindCabinByName(key.substr(6));
			return cabin ? cabin->GetScript() : nullptr;
		}
		if (key.compare(0, 9, "building:") == 0) {
			Building* building = map->GetBuilding(key.substr(9));
			return building ? building->GetScript() : nullptr;
		}
		if (key.compare(0, 5, "zone:") == 0) {
			Zone* zone = map->GetZone(key.substr(5));
			return zone ? zone->GetScript() : nullptr;
		}
		if (key.compare(0, 5, "room:") == 0) {
			Room* room = map->LocateRoom(key.substr(5));
			return room && room->GetAddress() == key.substr(5) ? room->GetScript() : nullptr;
		}
	}
	if (key.compare(0, 8, "vehicle:") == 0) {
		Traffic* traffic = framework->GetTraffic();
		Vehicle* vehicle = traffic ? traffic->FindVehicleByName(key.substr(8)) : nullptr;
		return vehicle ? vehicle->GetScript() : nullptr;
	}
	if (key.compare(0, 7, "weapon:") == 0) {
		// 武器脚本：每个已启用的武器都有一份，由Player常驻持有(不随装备/换枪增删)，键里是武器id。
		Weapon* weapon = framework->GetPlayer() ? framework->GetPlayer()->FindWeapon(key.substr(7)) : nullptr;
		return weapon ? weapon->GetScript() : nullptr;
	}
	if (key.compare(0, 6, "asset:") == 0) {
		// 资产名全局唯一：先找玩家身上/地上的，再找房间里的(都是不分配的遍历)。
		const string name = key.substr(6);
		Asset* asset = framework->GetPlayer() ? framework->GetPlayer()->FindAssetByName(name) : nullptr;
		if (!asset && framework->GetMap()) asset = framework->GetMap()->FindAssetByName(name);
		return asset ? asset->GetScript() : nullptr;
	}
	for (auto& [candidateKey, script] : ScriptSaveCodec::CollectScripts(framework->GetMap(), framework->GetPopulace(),
		framework->GetSociety(), framework->GetTraffic(), story, framework->GetPlayer())) {
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

namespace {
	// create_timer的relative/real/next模式把time解析成“天数+钟点”：日期部分是相对天数(年月日减0年1月1日，
	// 日期不限范围，如1月40日=39天)。支持：
	//   "[Y年]M月D日[ H:M:S 或 H时M分S秒]"、"YYYY-MM-DD[ H:M:S]"、"H:M:S"(只有钟点，天数0)。
	// 不用Time类解析：Time会校验日期范围(1月40日非法)。
	struct TimerOffset {
		int days = 0;
		int hour = 0;
		int minute = 0;
		int second = 0;
		int millisecond = 0;
	};

	bool ParseTimerOffset(const string& text, TimerOffset& out) {
		smatch match;
		int year = 0, month = 1, day = 1;
		auto readTime = [&](int firstGroup) {
			if (match[firstGroup].matched) {
				out.hour = stoi(match[firstGroup]);
				out.minute = stoi(match[firstGroup + 1]);
				out.second = stoi(match[firstGroup + 2]);
			}
		};

		static const regex chinese(R"(^(?:(\d{1,4})年)?(\d{1,2})月(\d{1,4})日(?:\s*(\d{1,2})(?::|时)(\d{1,2})(?::|分)(\d{1,2})(?:秒)?)?$)");
		static const regex iso(R"(^(\d{1,4})-(\d{1,2})-(\d{1,4})(?:[T ](\d{1,2}):(\d{1,2}):(\d{1,2}))?$)");
		static const regex clockOnly(R"(^(\d{1,2}):(\d{1,2}):(\d{1,2})$)");

		if (regex_match(text, match, chinese)) {
			year = match[1].matched ? stoi(match[1]) : 0;
			month = stoi(match[2]);
			day = stoi(match[3]);
			readTime(4);
		}
		else if (regex_match(text, match, iso)) {
			year = stoi(match[1]);
			month = stoi(match[2]);
			day = stoi(match[3]);
			readTime(4);
		}
		else if (regex_match(text, match, clockOnly)) {
			readTime(1);
			return true;
		}
		else {
			return false;
		}

		if (month < 1 || month > 12 || day < 1) return false;
		// 年月日减0年1月1日：整年 + 整月 + (日-1)。
		int days = Time::DaysBetweenYears(0, year);
		for (int m = 1; m < month; m++) days += Time::DaysInMonth(year, m);
		out.days = days + (day - 1);
		return true;
	}

	enum class TimerResolve { Ok, Real, Past, NeedsDate, Unparsable, UnknownMode };

	// create_timer与jump_time共用：把mode+time解析成目标时刻。mode为real时不产生Time，返回Real并把秒数写进*realSeconds
	// (jump_time传nullptr)。
	TimerResolve ResolveTimerTarget(const string& mode, const string& timeText, const Time& now, Time& target, double* realSeconds) {
		if (mode == "absolute") {
			Time parsed;
			if (!Time::TryParse(timeText, parsed)) return TimerResolve::Unparsable;
			if (!parsed.IsValid()) return TimerResolve::NeedsDate;
			if (parsed <= now) return TimerResolve::Past;
			target = parsed;
			return TimerResolve::Ok;
		}

		TimerOffset offset;
		if (mode != "relative" && mode != "real" && mode != "next") return TimerResolve::UnknownMode;
		if (!ParseTimerOffset(timeText, offset)) return TimerResolve::Unparsable;

		if (mode == "relative") {
			target = now;
			target.AddDays(offset.days);
			target.AddHours(offset.hour);
			target.AddMinutes(offset.minute);
			target.AddSeconds(offset.second);
			target.AddMilliseconds(offset.millisecond);
			return TimerResolve::Ok;
		}
		if (mode == "real") {
			if (realSeconds) {
				*realSeconds = offset.days * 86400.0 + offset.hour * 3600.0 + offset.minute * 60.0 + offset.second
					+ offset.millisecond / 1000.0;
			}
			return TimerResolve::Real;
		}
		// next：日期是相对天数，时间是绝对钟点；钟点早于当前钟点就再晚一天。例：现在10点，1日8点=明天8点，
		// 2日8点=后天8点，1日11点=当天11点。
		int days = offset.days;
		auto timeOfDay = [](int h, int m, int s, int ms) { return ((h * 60 + m) * 60 + s) * 1000 + ms; };
		if (timeOfDay(offset.hour, offset.minute, offset.second, offset.millisecond)
			< timeOfDay(now.GetHour(), now.GetMinute(), now.GetSecond(), now.GetMillisecond())) {
			days += 1;
		}
		target = now;
		target.SetTime(offset.hour, offset.minute, offset.second, offset.millisecond);
		target.AddDays(days);
		return TimerResolve::Ok;
	}
}

void UForeverStoryFrameworkComponent::ApplyCreateTimer(const CreateTimerChange* change, const ScriptContext& context) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::ApplyCreateTimer")
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

	// mode决定time怎么解释，见CreateTimerChange::SetMode：absolute(默认)/relative/real/next。
	string mode = change->GetMode().empty() ? string("absolute") : ToString(EvaluateExpression(change->GetMode(), context));
	const Time& now = *player->GetTime();

	auto warn = [&](const char* reason) {
		UE_LOG(LogTemp, Warning, TEXT("create_timer \"%s\" (mode=%s, time=\"%s\"): %s"),
			UTF8_TO_TCHAR(name.c_str()), UTF8_TO_TCHAR(mode.c_str()), UTF8_TO_TCHAR(timeText.c_str()), UTF8_TO_TCHAR(reason));
	};

	Time target;
	double realSeconds = 0.0;
	switch (ResolveTimerTarget(mode, timeText, now, target, &realSeconds)) {
	case TimerResolve::Ok: story->CreateTimer(name, target, ownerKey); break;
	case TimerResolve::Real: story->CreateRealTimer(name, realSeconds, ownerKey); break;
	case TimerResolve::Past: warn("time is in the past, ignored."); break;
	case TimerResolve::NeedsDate: warn("absolute needs a full date; use mode \"next\" for a time of day."); break;
	case TimerResolve::Unparsable: warn("cannot parse time."); break;
	case TimerResolve::UnknownMode: warn("unknown mode (absolute/relative/real/next)."); break;
	}
	CORE_GUARD_END
}

bool UForeverStoryFrameworkComponent::ResolveJumpTarget(const JumpTimeChange* change, const ScriptContext& context, Time& outTarget) {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Player* player = framework ? framework->GetPlayer() : nullptr;
	if (!player || !player->GetTime()) return false;

	string timeText = ToString(EvaluateExpression(change->GetTime(), context));
	string mode = change->GetMode().empty() ? string("absolute") : ToString(EvaluateExpression(change->GetMode(), context));
	const Time& now = *player->GetTime();

	switch (ResolveTimerTarget(mode, timeText, now, outTarget, nullptr)) {
	case TimerResolve::Ok:
		// 只往前跳：目标不晚于现在(relative偏移为0、next恰好等于此刻等)同样什么都不做。
		return outTarget > now;
	case TimerResolve::Past:
		return false; // 过去的绝对时间：静默不执行
	case TimerResolve::Real:
		UE_LOG(LogTemp, Warning, TEXT("jump_time: mode \"real\" is not supported, ignored."));
		return false;
	case TimerResolve::NeedsDate:
		UE_LOG(LogTemp, Warning, TEXT("jump_time (time=\"%s\"): absolute needs a full date; use mode \"next\" for a time of day."),
			UTF8_TO_TCHAR(timeText.c_str()));
		return false;
	case TimerResolve::Unparsable:
		UE_LOG(LogTemp, Warning, TEXT("jump_time: cannot parse time \"%s\"."), UTF8_TO_TCHAR(timeText.c_str()));
		return false;
	case TimerResolve::UnknownMode:
		UE_LOG(LogTemp, Warning, TEXT("jump_time: unknown mode \"%s\" (absolute/relative/next)."), UTF8_TO_TCHAR(mode.c_str()));
		return false;
	}
	return false;
}

void UForeverStoryFrameworkComponent::ApplyRemoveTimer(const RemoveTimerChange* change, const ScriptContext& context) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::ApplyRemoveTimer")
	if (!story) return;
	// 只删执行这个change的脚本自己名下的计时器(不同脚本可以有同名计时器)。
	string ownerKey = FindScriptKey(context.self);
	if (ownerKey.empty()) return;
	story->RemoveTimer(ToString(EvaluateExpression(change->GetName(), context)), ownerKey);
	CORE_GUARD_END
}

// 每帧最多处理的到时计时器数；8点/9点这类几千个岗位脚本同时到时，多出的留到后面几帧。
static constexpr size_t kMaxStoryTimersPerFrame = 16;

void UForeverStoryFrameworkComponent::FireExpiredTimers(float deltaSeconds) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::FireExpiredTimers")
	if (!story) return;
	story->AdvanceRealTimers(deltaSeconds);
	FireTimerBatch(kMaxStoryTimersPerFrame);
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::DrainExpiredTimers(int32 maxIterations) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::DrainExpiredTimers")
	for (int32 i = 0; i < maxIterations; i++) {
		if (!FireTimerBatch(kMaxStoryTimersPerFrame)) return;
	}
	UE_LOG(LogTemp, Warning, TEXT("DrainExpiredTimers: still has due timers after %d batches (a timer re-arming at the same instant?)."), maxIterations);
	CORE_GUARD_END
}

bool UForeverStoryFrameworkComponent::FireTimerBatch(size_t maxCount) {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(GetOwner());
	Player* player = framework ? framework->GetPlayer() : nullptr;
	if (!story || !player || !player->GetTime()) return false;

	const double batchStart = FPlatformTime::Seconds();
	vector<StoryTimer> expired = story->PopExpiredTimers(*player->GetTime(), maxCount);
	if (expired.empty()) return false;
	const double popSeconds = FPlatformTime::Seconds() - batchStart;

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
		const double matchStart = FPlatformTime::Seconds();
		const vector<ScriptAction> actions = script->MatchEvent(&event, context, &postImplement);
		const double processStart = FPlatformTime::Seconds();
		ProcessScriptActions(framework, actions, context);
		const double timerEnd = FPlatformTime::Seconds();
		if (timerEnd - matchStart > 0.01) {
			UE_LOG(LogTemp, Warning, TEXT("慢计时器 %s (owner=%s): MatchEvent %.1f ms, 执行动作 %.1f ms, 动作数 %d"),
				UTF8_TO_TCHAR(timer.name.c_str()), UTF8_TO_TCHAR(timer.ownerKey.c_str()),
				(processStart - matchStart) * 1000.0, (timerEnd - processStart) * 1000.0, static_cast<int32>(actions.size()));
		}
	}
	// 卡顿排查：一批计时器超过10ms就打日志，区分是取到时计时器慢还是处理脚本动作慢。
	const double batchSeconds = FPlatformTime::Seconds() - batchStart;
	if (batchSeconds > 0.01) {
		UE_LOG(LogTemp, Warning, TEXT("FireTimerBatch: %d timers took %.1f ms (PopExpiredTimers %.1f ms)."),
			static_cast<int32>(expired.size()), batchSeconds * 1000.0, popSeconds * 1000.0);
	}
	return true;
}

void UForeverStoryFrameworkComponent::ShowCurrentBackground() {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::ShowCurrentBackground")
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
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::AdvanceBackground() {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::AdvanceBackground")
	if (backgroundQueue.Num() == 0) return;

	std::string finishedLabel = backgroundQueue[0].label;
	backgroundQueue.RemoveAt(0);
	// 先显示下一条再广播：广播命中的milestone弹出的新对话会走ShowDialog->ShowCurrentItem，可能
	// 往backgroundQueue里追加后台台词，这时后台已经在播(或确实空闲)，不会重复启动计时。
	ShowCurrentBackground();
	if (!finishedLabel.empty()) BroadcastSpeakingFinish(finishedLabel);
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::BeginPlay() {
	Super::BeginPlay();
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::BeginPlay")
	// 弹射物蓝图(mod里)报告命中 -> projectile_hit事件发给所有者脚本。
	UForeverProjectileBridgeComponent::HitHandler().BindLambda(
		[this](const FString& ownerKey, const FVector& hit, const FVector& normal, const FString& sufferer) {
			NotifyProjectileHit(string(TCHAR_TO_UTF8(*ownerKey)), hit, normal, string(TCHAR_TO_UTF8(*sufferer)));
		});
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) {
	CORE_GUARD_ALWAYS_BEGIN("UForeverStoryFrameworkComponent::EndPlay")
	UForeverProjectileBridgeComponent::HitHandler().Unbind();
	if (UWorld* world = GetWorld()) world->GetTimerManager().ClearTimer(backgroundTimer);
	backgroundQueue.Reset();
	CORE_GUARD_END
	Super::EndPlay(EndPlayReason);
}

void UForeverStoryFrameworkComponent::BroadcastSpeakingFinish(const string& label) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::BroadcastSpeakingFinish")
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
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::SelectDialogOption(int32 index) {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::SelectDialogOption")
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
	CORE_GUARD_END
}

void UForeverStoryFrameworkComponent::ShowCurrentItem() {
	CORE_GUARD_BEGIN("UForeverStoryFrameworkComponent::ShowCurrentItem")
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
		// 对话结束，重新露出MeetOption（Visible是AddToViewport()之后的默认可见状态）。
		if (meetOptionWidget) meetOptionWidget->SetVisibility(ESlateVisibility::Visible);

		// 对话播完恢复游戏输入。光改SetInputMode不够：UIOnly期间点击会把Slate焦点转到Button上，
		// 焦点留在UI时Look收不到鼠标移动(鼠标不消失、没法转视角)，必须再SetFocusToGameViewport()抢回来。
		playerController->bShowMouseCursor = false;
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(playerController);
		UWidgetBlueprintLibrary::SetFocusToGameViewport();
		return;
	}

	// 对话展示期间把MeetOption藏起来，免得"靠近市民能问什么"的列表和剧情对话同屏抢注意力/抢点击。
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

	// 对话展示期间切UIOnly:Pawn的Enhanced Input收不到输入，Jump/Move/MeetOption不会误触发(GameAndUI下按空格推进对话角色会跳)。
	// 代价是SectionSpeaking的空格推进改走NativeOnKeyDown，所以widgetToFocus必须是当前显示的Widget才能拿到键盘焦点
	// (见UI/SectionSpeakingWidget.h)。每次都设一遍，重复设置无副作用。
	playerController->bShowMouseCursor = true;
	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(playerController, widgetToFocus, EMouseLockMode::DoNotLock, false);
	CORE_GUARD_END
}
