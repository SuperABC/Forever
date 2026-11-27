#include "Framework/ForeverDeathFrameworkComponent.h"

#include "Framework/CoreGuard.h"
#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverPopulaceFrameworkComponent.h"
#include "Framework/ForeverStoryFrameworkComponent.h"
#include "Element/CitizenElement.h"
#include "Player/ForeverPlayerController.h"
#include "UI/DeathWidget.h"
#include "UI/FadeSubsystem.h"
#include "UI/PausePanelWidget.h"

#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"

#include "map/map.h"
#include "player/player.h"
#include "populace/citizen.h"
#include "populace/populace.h"
#include "story/event.h"

UForeverDeathFrameworkComponent::UForeverDeathFrameworkComponent() {
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
}

AForeverFrameworkActor* UForeverDeathFrameworkComponent::GetFramework() const {
	return Cast<AForeverFrameworkActor>(GetOwner());
}

UDeathWidget* UForeverDeathFrameworkComponent::GetWidget() const {
	AForeverPlayerController* controller = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	return controller ? controller->GetDeathWidget() : nullptr;
}

bool UForeverDeathFrameworkComponent::HandlePlayerDeath(ACitizenElement* element, Citizen* citizen, const std::string& killer,
	const std::string& weapon, const std::string& part, const std::string& location) {
	AForeverFrameworkActor* framework = GetFramework();
	if (state != EState::None || !citizen || !framework || citizen != framework->GetPlayerCitizen()) return false;

	state = EState::Dying;
	elapsed = 0.f;
	deadCitizen = citizen;
	deadName = citizen->GetName();
	corpse = element;
	bCorpseRemoved = false;

	AForeverPlayerController* controller = Cast<AForeverPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	if (controller) {
		// 先关掉开着的面板(投掷物列表关的时候会把它的慢放还原)，再上死亡慢镜头。
		controller->CloseGameplayPanels();
		// 不再响应移动/开火/切枪等Pawn上的按键(Controller上的全局热键由它自己按IsPlayerDead屏蔽)。
		if (APawn* pawn = controller->GetPawn()) pawn->DisableInput(controller);
		// 玩家开着车死的：死者是藏起来的那个市民，镜头留在车上，只有慢镜头和渐暗。
		if (element && controller->GetPawn() == element) element->BeginDeathPresentation();
	}
	UGameplayStatics::SetGlobalTimeDilation(this, slowMotionDilation);
	if (UDeathWidget* widget = GetWidget()) widget->BeginDeath();
	else UE_LOG(LogTemp, Warning, TEXT("UForeverDeathFrameworkComponent: deathWidgetClass未设置，死亡界面不会显示，慢镜头结束后直接继续游戏。"));

	if (UForeverStoryFrameworkComponent* storyFramework = framework->GetStoryFramework()) {
		PlayerDeceaseEvent event(deadName, killer, weapon, part, location);
		storyFramework->BroadcastToEveryone(event);
	}
	return true;
}

void UForeverDeathFrameworkComponent::TickComponent(float deltaTime, ELevelTick tickType, FActorComponentTickFunction* thisTickFunction) {
	Super::TickComponent(deltaTime, tickType, thisTickFunction);
	CORE_GUARD_BEGIN("UForeverDeathFrameworkComponent::TickComponent")
	switch (state) {
	case EState::Dying: {
		// 用真实时间：世界被慢放了。
		elapsed += FApp::GetDeltaTime();
		const float progress = slowMotionDuration > KINDA_SMALL_NUMBER ? elapsed / slowMotionDuration : 1.f;
		if (UDeathWidget* widget = GetWidget()) widget->SetProgress(progress);
		if (progress >= 1.f) EnterMenu();
		break;
	}
	case EState::Reviving: {
		// 跳时间的黑屏完全盖住以后才撤尸体、收起死亡界面，免得看见尸体消失/镜头跳走。
		UGameInstance* gameInstance = GetOwner() ? GetOwner()->GetGameInstance() : nullptr;
		UFadeSubsystem* fade = gameInstance ? gameInstance->GetSubsystem<UFadeSubsystem>() : nullptr;
		if (!bCorpseRemoved && fade && fade->IsHeldBlack()) {
			RemoveCorpse();
			if (UDeathWidget* widget = GetWidget()) widget->EndDeath();
		}
		break;
	}
	default:
		break;
	}
	CORE_GUARD_END
}

void UForeverDeathFrameworkComponent::EnterMenu() {
	state = EState::Menu;
	// 世界停住(同暂停界面)：脚本计时器、市民、交通都不再走，尸体也定格。
	UGameplayStatics::SetGlobalTimeDilation(this, 0.f);
	if (UDeathWidget* widget = GetWidget()) widget->ShowMenu();
	else Continue(); // 没有界面可点：直接继续游戏，不让玩家卡死
}

void UForeverDeathFrameworkComponent::Continue() {
	if (state != EState::Menu) return;
	AForeverFrameworkActor* framework = GetFramework();
	if (!framework) return;
	state = EState::Reviving;

	if (UDeathWidget* widget = GetWidget()) widget->HideMenu();
	UGameplayStatics::SetGlobalTimeDilation(this, 1.f);

	// 跳reviveSkipDays天(期间经济/计时器/事件照常发生)，跳完的回调里复活。跳不了(比如正在跳别的时间)就直接复活。
	bool bSkipping = false;
	if (Player* player = framework->GetPlayer()) {
		Time target = *player->GetTime();
		target.AddDays(FMath::Max(0, reviveSkipDays));
		TWeakObjectPtr<UForeverDeathFrameworkComponent> weakThis(this);
		bSkipping = framework->BeginTimeSkip(target, [weakThis]() {
			if (weakThis.IsValid()) weakThis->Revive();
		});
	}
	if (!bSkipping) Revive();
}

void UForeverDeathFrameworkComponent::ExitToMenu() {
	if (state != EState::Menu) return;
	// 和暂停界面的"退出游戏"同一个：直接退出这一局回主菜单，不存档。
	UPausePanelWidget::ExitToMainMenu(this);
}

void UForeverDeathFrameworkComponent::RemoveCorpse() {
	if (bCorpseRemoved) return;
	bCorpseRemoved = true;
	AForeverFrameworkActor* framework = GetFramework();
	UForeverPopulaceFrameworkComponent* populaceFramework = framework ? framework->GetPopulaceFramework() : nullptr;
	// 就是平时市民死亡的那一步(撤Actor、取消任务和通勤)，只是推迟到了这里。
	if (populaceFramework && deadCitizen) populaceFramework->HandleCitizenDeath(deadCitizen);
	else if (ACitizenElement* element = corpse.Get()) element->Destroy();
	corpse.Reset();
}

void UForeverDeathFrameworkComponent::Revive() {
	if (state != EState::Reviving) return;
	RemoveCorpse();

	AForeverFrameworkActor* framework = GetFramework();
	Populace* populace = framework ? framework->GetPopulace() : nullptr;
	UForeverPopulaceFrameworkComponent* populaceFramework = framework ? framework->GetPopulaceFramework() : nullptr;
	UForeverStoryFrameworkComponent* storyFramework = framework ? framework->GetStoryFramework() : nullptr;
	Citizen* citizen = populace ? populace->FindCitizenByName(deadName) : nullptr;
	const FString name = UTF8_TO_TCHAR(deadName.c_str());

	ACitizenElement* element = nullptr;
	if (!citizen || citizen->IsDeparted() || !populaceFramework || !storyFramework) {
		UE_LOG(LogTemp, Warning, TEXT("死亡复活：找不到市民\"%s\"(或已离城)，没法复活。"), *name);
	}
	else {
		// 跳时间期间脚本可能已经把他复活并放到了别处(比如市民Scheduler脚本的npc_revive+npc_teleport)：先撤掉他现有的Actor，统一从复活点重新生成。
		populaceFramework->HandleCitizenDeath(citizen);
		if (citizen->IsDead()) citizen->Revive();
		citizen->EndWalk();
		citizen->SetCurrentRoom(nullptr);
		citizen->SetCurrentGreen(nullptr, -1);

		// 复活点：现在没有医院，随机一个路口的人行道转角(见TODO.md：有医院以后改成医院门口，没有医院就不能复活)。
		float x = 0.f, y = 0.f, z = 0.f;
		Map* map = framework->GetMap();
		if (map && map->PickJunctionRespawn(x, y, z)) {
			citizen->SetPosition(x, y, z);
		}
		else {
			// 地图上没有路口：退回到家里。
			UE_LOG(LogTemp, Warning, TEXT("死亡复活：地图上没有带人行道的路口，\"%s\"在家里复活。"), *name);
			citizen->ClearPosition();
			citizen->SetCurrentRoom(citizen->GetRoom());
		}
		element = storyFramework->PossessCitizenByName(name);
		if (!element) UE_LOG(LogTemp, Warning, TEXT("死亡复活：\"%s\"生成失败，没有重新操控。"), *name);
	}

	if (UDeathWidget* widget = GetWidget()) widget->EndDeath();
	state = EState::None;
	deadCitizen = nullptr;
	corpse.Reset();

	if (storyFramework && element) {
		PlayerReviveEvent event(deadName, framework->DescribeLocation(element->GetActorLocation()));
		storyFramework->BroadcastToEveryone(event);
	}
	deadName.clear();
}
