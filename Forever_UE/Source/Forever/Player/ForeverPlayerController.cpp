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
#include "UI/PhoneWidget.h"
#include "UI/InventoryWidget.h"
#include "UI/MapWidget.h"
#include "UI/ForeverPausePanelWidget.h"
#include "UI/ForeverThrowListWidget.h"
#include "UI/ForeverFadeSubsystem.h"
#include "UI/ForeverFadeWidget.h"
#include "Player/ForeverIdentifyComponent.h"

AForeverPlayerController::AForeverPlayerController() {
	identifyComponent = CreateDefaultSubobject<UForeverIdentifyComponent>(TEXT("IdentifyComponent"));
}

void AForeverPlayerController::BeginPlay() {
	Super::BeginPlay();
	InitializeOnce();
}

void AForeverPlayerController::InitializeOnce() {
	// 打包版里AForeverFrameworkActor::BeginPlay(含game_start广播)先于这个Controller的BeginPlay执行，
	// game_start里的start_puzzle/对话等要用这里创建的Widget、并设置UI Only输入——如果等到本Controller
	// 的BeginPlay才创建，Widget还是空的(start_puzzle被静默跳过)，而且下面的"输入模式重置成Game Only"
	// 还会盖掉谜题刚设的UI Only。所以初始化提到一个幂等函数里，FrameworkActor在生成世界之前先调一次，
	// BeginPlay里再调一次(已经初始化过就什么都不做)。
	if (bInitialized) return;
	bInitialized = true;

	// 显式把输入模式设回Game Only+隐藏鼠标指针——不能假设这是空白状态:游戏启动配置界面
	// (Menu关卡的AForeverMenuController::BeginPlay())会把输入模式设成UI Only+显示鼠标
	// 指针，这个状态挂在ViewportClient/LocalPlayer身上，不是挂在PlayerController实例上，
	// 不会因为旧的MenuController被OpenLevel销毁就自动还原——没有这一步，从配置界面点"开始
	// 游戏"进场之后，输入会一直停留在UI Only模式，鼠标键盘操作不到任何Pawn，表现上就是
	// "角色完全操控不了"。这里显式重置，不管上一个关卡/Controller留下什么状态都能保证
	// 这一局是正常的Game Only输入，PIE直接在World.umap上跑(不经过Menu)同样安全——本来
	// 就应该是这个状态。
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;

	// 手机(Phone)系统：P键要求任何时候都生效(包括开局还没占有任何真正Pawn、操控着
	// ADefaultPawn的那几帧，以及ChangeControlChange换人操控/换乘载具的瞬间)——这个动态
	// Context常驻加在Controller上而不是Pawn的PossessedBy里，就不用依赖"当前是不是正占有着
	// 一个真正的Pawn"，见ForeverPlayerController.md"旧MainController热键"一节。Pawn侧
	// (AForeverCharacter.cpp/VehicleElement.cpp)不重复加这份Context。
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

	if (phoneWidgetClass == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("AForeverPlayerController: phoneWidgetClass未设置,跳过PhoneWidget创建。"));
	}
	else if ((phoneWidget = CreateWidget<UPhoneWidget>(this, phoneWidgetClass)) != nullptr) {
		phoneWidget->AddToViewport();
	}

	if (inventoryWidgetClass == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("AForeverPlayerController: inventoryWidgetClass未设置,跳过InventoryWidget创建。"));
	}
	else if ((inventoryWidget = CreateWidget<UForeverInventoryWidget>(this, inventoryWidgetClass)) != nullptr) {
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
	else if ((pauseWidget = CreateWidget<UForeverPausePanelWidget>(this, pauseWidgetClass)) != nullptr) {
		// ZOrder比其它常驻Widget高，暂停界面盖在最上面。
		pauseWidget->AddToViewport(10);
	}

	if (throwListWidgetClass == nullptr) {
		UE_LOG(LogTemp, Warning, TEXT("AForeverPlayerController: throwListWidgetClass未设置,跳过ThrowList创建(长按3不会弹出列表)。"));
	}
	else if ((throwListWidget = CreateWidget<UForeverThrowListWidget>(this, throwListWidgetClass)) != nullptr) {
		throwListWidget->AddToViewport(5);
	}

	// 开局黑屏由GameInstance上的UForeverFadeSubsystem持有(跨关卡)：从菜单按下开始/加载按钮时已经
	// 变黑并一直盖到这里；这里登记类并保证黑着(编辑器里直接PIE在World.umap上时补一个)。
	if (UGameInstance* gameInstance = GetGameInstance()) {
		if (UForeverFadeSubsystem* fade = gameInstance->GetSubsystem<UForeverFadeSubsystem>()) {
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
	if (isShown(mapWidget) || isShown(phoneWidget) || isShown(inventoryWidget) || isShown(puzzleWidget)
		|| isShown(sectionSpeakingWidget) || isShown(sectionOptionWidget) || isShown(throwListWidget)) {
		return;
	}
	pauseWidget->OpenPanel();
}
