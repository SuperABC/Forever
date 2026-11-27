#include "UI/DeathWidget.h"

#include "Framework/CoreGuard.h"
#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverDeathFrameworkComponent.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

void UDeathWidget::NativeConstruct() {
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);
	SetIsFocusable(true);
	if (ContinueButton) ContinueButton->OnClicked.AddDynamic(this, &UDeathWidget::HandleContinueClicked);
	if (ExitButton) ExitButton->OnClicked.AddDynamic(this, &UDeathWidget::HandleExitClicked);
}

void UDeathWidget::BeginDeath() {
	// 慢镜头期间只是盖在画面上，不拦截鼠标(此时也没有可点的东西)。
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetButtonsVisible(false);
	SetProgress(0.f);
}

void UDeathWidget::SetProgress(float progress) {
	progress = FMath::Clamp(progress, 0.f, 1.f);
	if (DimImage) DimImage->SetRenderOpacity(progress * dimOpacity);
	if (TitleText) TitleText->SetRenderOpacity(progress);
}

void UDeathWidget::ShowMenu() {
	SetVisibility(ESlateVisibility::Visible);
	SetButtonsVisible(true);
	if (APlayerController* playerController = GetOwningPlayer()) {
		playerController->bShowMouseCursor = true;
		UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(playerController, this, EMouseLockMode::DoNotLock, false);
		SetKeyboardFocus();
	}
}

void UDeathWidget::HideMenu() {
	SetButtonsVisible(false);
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (APlayerController* playerController = GetOwningPlayer()) {
		playerController->bShowMouseCursor = false;
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(playerController);
		UWidgetBlueprintLibrary::SetFocusToGameViewport();
	}
}

void UDeathWidget::EndDeath() {
	SetButtonsVisible(false);
	SetVisibility(ESlateVisibility::Collapsed);
}

void UDeathWidget::SetButtonsVisible(bool bVisible) {
	const ESlateVisibility visibility = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (ContinueButton) ContinueButton->SetVisibility(visibility);
	if (ExitButton) ExitButton->SetVisibility(visibility);
}

FReply UDeathWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) {
	// 死亡界面只能点按钮；其余按键吞掉，不让Esc/Tab等漏到别处。
	return FReply::Handled();
}

UForeverDeathFrameworkComponent* UDeathWidget::FindDeathFramework() const {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()));
	return framework ? framework->GetDeathFramework() : nullptr;
}

void UDeathWidget::HandleContinueClicked() {
	CORE_GUARD_BEGIN("UDeathWidget::HandleContinueClicked")
	if (UForeverDeathFrameworkComponent* death = FindDeathFramework()) death->Continue();
	CORE_GUARD_END
}

void UDeathWidget::HandleExitClicked() {
	CORE_GUARD_BEGIN("UDeathWidget::HandleExitClicked")
	if (UForeverDeathFrameworkComponent* death = FindDeathFramework()) death->ExitToMenu();
	CORE_GUARD_END
}
