#include "ForeverPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

#include "Input/ForeverKeyBindingSubsystem.h"
#include "UI/MeetOptionWidget.h"
#include "UI/SectionSpeakingWidget.h"
#include "UI/SectionOptionWidget.h"
#include "UI/PuzzleWidget.h"
#include "UI/PhoneWidget.h"
#include "UI/InventoryWidget.h"
#include "UI/MapWidget.h"

void AForeverPlayerController::BeginPlay() {
	Super::BeginPlay();

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
	// Context原来只在Pawn的PossessedBy里加(见AForeverCharacter::PossessedBy)，Controller
	// 自己在这里常驻加一份，就不用依赖"当前是不是正占有着一个真正的Pawn"，见
	// MAINCONTROLLER_TODO.md"P开手机"这一行热键的既定设计。Pawn那边同样这两行现在删掉了
	// (不需要重复加)，见AForeverCharacter.cpp/VehicleElement.cpp。
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
			}
		}
	}
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
