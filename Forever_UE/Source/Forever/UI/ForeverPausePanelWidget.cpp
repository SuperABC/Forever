#include "UI/ForeverPausePanelWidget.h"

#include "Input/ForeverKeyBindingSubsystem.h"
#include "Mod/ForeverSaveSubsystem.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace {
	// 低于这个时间缩放就视为世界已经被别的系统停住(见OpenPanel)。
	constexpr float kPausedDilationThreshold = 0.01f;
}

void UForeverPausePanelWidget::NativeConstruct() {
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);
	SetIsFocusable(true);

	if (SaveButton) SaveButton->OnClicked.AddDynamic(this, &UForeverPausePanelWidget::HandleSaveClicked);
	if (LoadButton) LoadButton->OnClicked.AddDynamic(this, &UForeverPausePanelWidget::HandleLoadClicked);
	if (CloseButton) CloseButton->OnClicked.AddDynamic(this, &UForeverPausePanelWidget::HandleCloseClicked);
	if (MessageText) MessageText->SetVisibility(ESlateVisibility::Collapsed);
}

FReply UForeverPausePanelWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) {
	const FKey key = InKeyEvent.GetKey();
	// 开关键跟Controller上的TogglePause同一个(按配置)，Esc固定也能关。
	FKey toggleKey = EKeys::Tab;
	if (UGameInstance* gameInstance = GetGameInstance()) {
		if (UForeverKeyBindingSubsystem* keyBindings = gameInstance->GetSubsystem<UForeverKeyBindingSubsystem>()) {
			toggleKey = keyBindings->GetKey(TEXT("TogglePause"));
		}
	}
	if (key == toggleKey || key == EKeys::Escape) {
		ClosePanel();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void UForeverPausePanelWidget::TogglePanel() {
	if (IsOpen()) ClosePanel();
	else OpenPanel();
}

void UForeverPausePanelWidget::OpenPanel() {
	if (IsOpen()) return;
	// 暂停用全局时间缩放。建筑LOD冻结(ForeverBuildingFrameworkComponent)和谜题(PuzzleWidget)
	// 也用时间缩放=0，并且结束时会改回1——它们已经把时间停住的时候不允许打开暂停界面，
	// 否则它们结束时会把这里的暂停意外解除，关暂停界面时又会把它们的冻结解除。
	// UE会把时间缩放夹到WorldSettings的最小值(默认0.0001)，不是精确的0，所以按阈值判断。
	if (UGameplayStatics::GetGlobalTimeDilation(this) < kPausedDilationThreshold) return;

	SetVisibility(ESlateVisibility::Visible);
	if (MessageText) MessageText->SetVisibility(ESlateVisibility::Collapsed);
	UGameplayStatics::SetGlobalTimeDilation(this, 0.f);

	if (APlayerController* playerController = GetOwningPlayer()) {
		playerController->bShowMouseCursor = true;
		UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(playerController, this, EMouseLockMode::DoNotLock, false);
		SetKeyboardFocus();
	}
}

void UForeverPausePanelWidget::ClosePanel() {
	if (!IsOpen()) return;

	SetVisibility(ESlateVisibility::Collapsed);
	UGameplayStatics::SetGlobalTimeDilation(this, 1.f);

	if (APlayerController* playerController = GetOwningPlayer()) {
		playerController->bShowMouseCursor = false;
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(playerController);
		UWidgetBlueprintLibrary::SetFocusToGameViewport();
	}
}

void UForeverPausePanelWidget::HandleSaveClicked() {
	UGameInstance* gameInstance = GetGameInstance();
	UForeverSaveSubsystem* saveSubsystem = gameInstance ? gameInstance->GetSubsystem<UForeverSaveSubsystem>() : nullptr;
	if (!saveSubsystem) return;

	FString error;
	bool ok = saveSubsystem->SaveWithDialog(error);
	RefocusSelf();
	if (ok) ShowMessage(TEXT("已保存"), false);
	else if (!error.IsEmpty()) ShowMessage(error, true);
	// 取消对话框：什么都不提示。
}

void UForeverPausePanelWidget::HandleLoadClicked() {
	UGameInstance* gameInstance = GetGameInstance();
	UForeverSaveSubsystem* saveSubsystem = gameInstance ? gameInstance->GetSubsystem<UForeverSaveSubsystem>() : nullptr;
	if (!saveSubsystem) return;

	// 成功时RequestLoad已经发起OpenLevel，这个关卡(连同暂停状态和这个widget)随之销毁，
	// 新关卡的AForeverPlayerController::BeginPlay会把输入模式重置成Game Only，不用这里收尾。
	FString error;
	bool ok = saveSubsystem->LoadWithDialog(error);
	if (ok) return;
	RefocusSelf();
	if (!error.IsEmpty()) ShowMessage(error, true);
}

void UForeverPausePanelWidget::HandleCloseClicked() {
	ClosePanel();
}

void UForeverPausePanelWidget::ShowMessage(const FString& message, bool bError) {
	if (bError) UE_LOG(LogTemp, Warning, TEXT("ForeverPausePanel: %s"), *message);
	if (MessageText) {
		MessageText->SetText(FText::FromString(message));
		MessageText->SetColorAndOpacity(FSlateColor(bError ? FLinearColor(1.f, 0.3f, 0.3f) : FLinearColor::White));
		MessageText->SetVisibility(ESlateVisibility::Visible);
	}
	else if (GEngine) {
		GEngine->AddOnScreenDebugMessage(202, 5.f, bError ? FColor::Red : FColor::Green, message);
	}
}

void UForeverPausePanelWidget::RefocusSelf() {
	if (!IsOpen()) return;
	if (APlayerController* playerController = GetOwningPlayer()) {
		UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(playerController, this, EMouseLockMode::DoNotLock, false);
	}
	SetKeyboardFocus();
}
