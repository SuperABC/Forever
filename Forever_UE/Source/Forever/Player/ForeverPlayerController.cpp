#include "ForeverPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

#include "Input/ForeverKeyBindingSubsystem.h"
#include "UI/MeetOptionWidget.h"
#include "UI/SectionSpeakingWidget.h"
#include "UI/BackgroundSpeakingWidget.h"
#include "UI/SectionOptionWidget.h"
#include "UI/PuzzleWidget.h"
#include "UI/ShopWidget.h"
#include "UI/PhoneWidget.h"
#include "UI/InventoryWidget.h"
#include "UI/MapWidget.h"
#include "UI/PausePanelWidget.h"
#include "UI/ThrowListWidget.h"
#include "UI/FadeSubsystem.h"
#include "UI/FadeWidget.h"
#include "Player/ForeverIdentifyComponent.h"
#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverTrafficFrameworkComponent.h"
#include "Kismet/GameplayStatics.h"
#include "map/map.h"
#include "map/building.h"
#include "map/room.h"

AForeverPlayerController::AForeverPlayerController() {
	identifyComponent = CreateDefaultSubobject<UForeverIdentifyComponent>(TEXT("IdentifyComponent"));
}

void AForeverPlayerController::OnPossess(APawn* inPawn) {
	Super::OnPossess(inPawn);
	// 切换操控（上下车、换人）后导航线要按新Pawn重新规划；没有活动导航时RefreshGuide什么都不做。
	if (AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()))) {
		if (UForeverTrafficFrameworkComponent* traffic = framework->GetTrafficFramework()) traffic->RefreshGuide();
	}
}

void AForeverPlayerController::TestNavigate() {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()));
	Map* map = framework ? framework->GetMap() : nullptr;
	UForeverTrafficFrameworkComponent* traffic = framework ? framework->GetTrafficFramework() : nullptr;
	if (!map || !traffic) return;

	// 所有停车场和住宅房间合成一个列表，等概率取一个。
	TArray<Room*> candidates;
	for (const auto& [name, building] : map->GetBuildings()) {
		if (!building) continue;
		for (Room* room : building->GetRooms()) {
			if (room && (room->IsParking() || room->IsResidential())) candidates.Add(room);
		}
	}
	if (candidates.Num() == 0) {
		UE_LOG(LogTemp, Warning, TEXT("TestNavigate: 没有停车场或住宅房间"));
		return;
	}
	Room* target = candidates[FMath::RandRange(0, candidates.Num() - 1)];
	UE_LOG(LogTemp, Log, TEXT("TestNavigate: 导航到 %s（%s，%d 个候选）"), UTF8_TO_TCHAR(target->GetAddress().c_str()),
		UTF8_TO_TCHAR(target->GetType().c_str()), candidates.Num());
	traffic->StartGuide(target);
}

void AForeverPlayerController::BeginPlay() {
	Super::BeginPlay();
	InitializeOnce();
}

void AForeverPlayerController::InitializeOnce() {
	// 打包版里FrameworkActor的BeginPlay(含game_start广播)先于本Controller的BeginPlay，所以这里做成幂等函数，两边都调(见md)。
	if (bInitialized) return;
	bInitialized = true;

	// 菜单关卡留下的UI Only+鼠标指针状态挂在ViewportClient/LocalPlayer上，不会随旧Controller销毁还原，必须显式重置。
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;

	// 全局热键的动态Context常驻加在Controller上，Pawn侧不重复加(见md)。
	if (UEnhancedInputLocalPlayerSubsystem* subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer())) {
		if (UGameInstance* gameInstance = GetGameInstance()) {
			if (UForeverKeyBindingSubsystem* keyBindings = gameInstance->GetSubsystem<UForeverKeyBindingSubsystem>()) {
				subsystem->AddMappingContext(keyBindings->GetBindingContext(), 0);
			}
		}
	}

	if (meetOptionWidgetClass == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("AForeverPlayerController: meetOptionWidgetClass未设置,跳过MeetOption创建。"));
	}
	else if ((meetOptionWidget = CreateWidget<UMeetOptionWidget>(this, meetOptionWidgetClass)) != nullptr) {
		meetOptionWidget->AddToViewport();
	}

	if (sectionSpeakingWidgetClass == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("AForeverPlayerController: sectionSpeakingWidgetClass未设置,跳过SectionSpeaking创建。"));
	}
	else if ((sectionSpeakingWidget = CreateWidget<USectionSpeakingWidget>(this, sectionSpeakingWidgetClass)) != nullptr) {
		sectionSpeakingWidget->AddToViewport();
		sectionSpeakingWidget->HideLine();
	}

	if (backgroundSpeakingWidgetClass == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("AForeverPlayerController: backgroundSpeakingWidgetClass未设置,跳过BackgroundSpeaking创建。"));
	}
	else if ((backgroundSpeakingWidget = CreateWidget<UBackgroundSpeakingWidget>(this, backgroundSpeakingWidgetClass)) != nullptr) {
		backgroundSpeakingWidget->AddToViewport();
		backgroundSpeakingWidget->HideLine();
	}

	if (sectionOptionWidgetClass == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("AForeverPlayerController: sectionOptionWidgetClass未设置,跳过SectionOption创建。"));
	}
	else if ((sectionOptionWidget = CreateWidget<USectionOptionWidget>(this, sectionOptionWidgetClass)) != nullptr) {
		sectionOptionWidget->AddToViewport();
		sectionOptionWidget->HideOptions();
	}

	if (puzzleWidgetClass == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("AForeverPlayerController: puzzleWidgetClass未设置,跳过PuzzleWidget创建。"));
	}
	else if ((puzzleWidget = CreateWidget<UPuzzleWidget>(this, puzzleWidgetClass)) != nullptr) {
		puzzleWidget->AddToViewport();
	}

	if (shopWidgetClass == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("AForeverPlayerController: shopWidgetClass未设置,跳过ShopWidget创建。"));
	}
	else if ((shopWidget = CreateWidget<UShopWidget>(this, shopWidgetClass)) != nullptr) {
		shopWidget->AddToViewport();
	}

	if (phoneWidgetClass == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("AForeverPlayerController: phoneWidgetClass未设置,跳过PhoneWidget创建。"));
	}
	else if ((phoneWidget = CreateWidget<UPhoneWidget>(this, phoneWidgetClass)) != nullptr) {
		phoneWidget->AddToViewport();
	}

	if (inventoryWidgetClass == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("AForeverPlayerController: inventoryWidgetClass未设置,跳过InventoryWidget创建。"));
	}
	else if ((inventoryWidget = CreateWidget<UInventoryWidget>(this, inventoryWidgetClass)) != nullptr) {
		inventoryWidget->AddToViewport();
	}

	if (mapWidgetClass == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("AForeverPlayerController: mapWidgetClass未设置,跳过MapWidget创建。"));
	}
	else if ((mapWidget = CreateWidget<UMapWidget>(this, mapWidgetClass)) != nullptr) {
		mapWidget->AddToViewport();
	}

	if (pauseWidgetClass == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("AForeverPlayerController: pauseWidgetClass未设置,跳过PausePanel创建。"));
	}
	else if ((pauseWidget = CreateWidget<UPausePanelWidget>(this, pauseWidgetClass)) != nullptr) {
		// ZOrder比其它常驻Widget高，暂停界面盖在最上面。
		pauseWidget->AddToViewport(10);
	}

	if (throwListWidgetClass == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("AForeverPlayerController: throwListWidgetClass未设置,跳过ThrowList创建(长按3不会弹出列表)。"));
	}
	else if ((throwListWidget = CreateWidget<UThrowListWidget>(this, throwListWidgetClass)) != nullptr) {
		throwListWidget->AddToViewport(5);
	}

	// 菜单按下开始/加载时已经变黑并盖到这里；这里登记类并保证黑着(编辑器直接PIE时补一个)。
	if (UGameInstance* gameInstance = GetGameInstance()) {
		if (UFadeSubsystem* fade = gameInstance->GetSubsystem<UFadeSubsystem>()) {
			if (fadeWidgetClass == nullptr) {
				UE_LOG(LogTemp, Warning, TEXT("AForeverPlayerController: fadeWidgetClass未设置,可能没有开局黑屏。"));
			}
			fade->RegisterWidgetClass(fadeWidgetClass);
			fade->EnsureBlack();
		}
	}
}

void AForeverPlayerController::SetupInputComponent() {
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* enhancedInput = Cast<UEnhancedInputComponent>(InputComponent)) {
		if (UGameInstance* gameInstance = GetGameInstance()) {
			if (UForeverKeyBindingSubsystem* keyBindings = gameInstance->GetSubsystem<UForeverKeyBindingSubsystem>()) {
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("TogglePhone")), ETriggerEvent::Started, this, &AForeverPlayerController::TogglePhone);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("ToggleInventory")), ETriggerEvent::Started, this, &AForeverPlayerController::ToggleInventory);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("ToggleMap")), ETriggerEvent::Started, this, &AForeverPlayerController::ToggleMap);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("CloseMap")), ETriggerEvent::Started, this, &AForeverPlayerController::HandleCloseMap);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("TogglePause")), ETriggerEvent::Started, this, &AForeverPlayerController::TogglePause);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("TestNavigate")), ETriggerEvent::Started, this, &AForeverPlayerController::TestNavigate);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("Identify")), ETriggerEvent::Started, this, &AForeverPlayerController::EnterIdentify);
				enhancedInput->BindAction(keyBindings->GetAction(TEXT("Identify")), ETriggerEvent::Completed, this, &AForeverPlayerController::ExitIdentify);
			}
		}
	}
}

void AForeverPlayerController::EnterIdentify() {
	if (identifyComponent) identifyComponent->Enter();
}

void AForeverPlayerController::ExitIdentify() {
	if (identifyComponent) identifyComponent->Exit();
}

void AForeverPlayerController::TogglePhone() {
	if (phoneWidget) phoneWidget->TogglePhone();
}

void AForeverPlayerController::ToggleInventory() {
	if (inventoryWidget) inventoryWidget->TogglePanel();
}

void AForeverPlayerController::ToggleMap() {
	if (mapWidget) mapWidget->ToggleMap();
}

void AForeverPlayerController::HandleCloseMap() {
	if (mapWidget) mapWidget->CloseMap();
}

void AForeverPlayerController::TogglePause() {
	if (!pauseWidget) return;
	if (pauseWidget->IsOpen()) {
		pauseWidget->ClosePanel();
		return;
	}

	auto isShown = [](UUserWidget* widget) { return widget && widget->IsVisible(); };
	if (isShown(mapWidget) || isShown(phoneWidget) || isShown(inventoryWidget) || isShown(puzzleWidget) || isShown(shopWidget)
		|| isShown(sectionSpeakingWidget) || isShown(sectionOptionWidget) || isShown(throwListWidget)) {
		return;
	}
	pauseWidget->OpenPanel();
}
