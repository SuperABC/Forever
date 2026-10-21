#include "UI/ForeverThrowListWidget.h"

#include "Input/ForeverKeyBindingSubsystem.h"
#include "Player/ForeverCharacter.h"
#include "Player/ForeverThrowableComponent.h"
#include "UI/ForeverThrowListButtonWidget.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace {
	// 打开列表期间的世界时间缩放。
	constexpr float kSlowDilation = 0.1f;
	// 低于这个时间缩放就视为世界已经被别的系统停住，不打开(同ForeverPausePanelWidget)。
	constexpr float kThrowListPausedDilationThreshold = 0.01f;
	// 点击按钮后最多等多久3键的松开(真实秒)，见NativeTick。
	constexpr double kMaxAwaitReleaseSeconds = 10.0;
}

void UForeverThrowListWidget::NativeConstruct() {
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);
	SetIsFocusable(true);
}

bool UForeverThrowListWidget::Open(const TArray<FThrowListItem>& items, const FString& activeType) {
	if (IsOpen()) return true;
	// 世界已经被别的系统停住时不打开：否则关闭时会把它们的冻结意外解除。
	if (UGameplayStatics::GetGlobalTimeDilation(this) < kThrowListPausedDilationThreshold) return false;
	if (!ListContainer || !buttonClass) {
		UE_LOG(LogTemp, Warning, TEXT("UForeverThrowListWidget: ListContainer或buttonClass未设置，无法打开列表。"));
		return false;
	}

	ListContainer->ClearChildren();
	if (EmptyText) EmptyText->SetVisibility(items.Num() == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	UForeverThrowListButtonWidget* activeRow = nullptr;
	for (const FThrowListItem& item : items) {
		UForeverThrowListButtonWidget* row = CreateWidget<UForeverThrowListButtonWidget>(this, buttonClass);
		if (!row) continue;
		const bool bActive = item.type == activeType;
		row->onSelected.BindUObject(this, &UForeverThrowListWidget::Select);
		row->Setup(item.type, item.count, bActive);
		if (UScrollBoxSlot* slot = Cast<UScrollBoxSlot>(ListContainer->AddChild(row))) {
			slot->SetPadding(FMargin(0.f, rowVerticalPadding, 0.f, rowVerticalPadding));
		}
		if (bActive) activeRow = row;
	}
	// 行很多时让当前激活的那一行滚动到可见位置。
	if (activeRow) ListContainer->ScrollWidgetIntoView(activeRow, false);

	previousDilation = UGameplayStatics::GetGlobalTimeDilation(this);
	UGameplayStatics::SetGlobalTimeDilation(this, kSlowDilation);
	bOpen = true;
	SetRenderOpacity(1.f);
	SetVisibility(ESlateVisibility::Visible);

	if (APlayerController* playerController = GetOwningPlayer()) {
		playerController->bShowMouseCursor = true;
		UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(playerController, this, EMouseLockMode::DoNotLock, false);
		SetKeyboardFocus();
	}
	return true;
}

void UForeverThrowListWidget::RestoreTimeDilation() {
	if (!bOpen) return;
	bOpen = false;

	// 只在时间缩放还是我们设的值时还原；中途被别的系统改走(比如谜题冻结了世界)就不动它。
	if (FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(this), kSlowDilation, 0.005f)) {
		UGameplayStatics::SetGlobalTimeDilation(this, previousDilation);
	}
}

void UForeverThrowListWidget::Close() {
	if (!IsOpen()) return;
	RestoreTimeDilation();
	bAwaitingRelease = false;
	SetRenderOpacity(1.f);
	SetVisibility(ESlateVisibility::Collapsed);

	if (APlayerController* playerController = GetOwningPlayer()) {
		playerController->bShowMouseCursor = false;
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(playerController);
		UWidgetBlueprintLibrary::SetFocusToGameViewport();
	}
}

void UForeverThrowListWidget::Select(const FString& type) {
	if (AForeverCharacter* character = Cast<AForeverCharacter>(GetOwningPlayerPawn())) {
		if (UForeverThrowableComponent* throwable = character->GetThrowableComponent()) {
			throwable->ActivateByType(type);
		}
	}

	// 列表看不见了、世界恢复正常速度，但输入仍留在UI Only，等3键真正松开再切回游戏，见头文件说明。
	// 不能设成Collapsed：Collapsed的控件拿不到键盘焦点。透明度0+HitTestInvisible既看不见也点不到，
	// 焦点还在。
	RestoreTimeDilation();
	bAwaitingRelease = true;
	awaitStartRealTime = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
	SetRenderOpacity(0.f);
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (APlayerController* playerController = GetOwningPlayer()) {
		playerController->bShowMouseCursor = false;
	}
	SetKeyboardFocus();
}

void UForeverThrowListWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime) {
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 保险：万一一直收不到3键的松开(焦点丢了之类)，等太久就强行切回游戏，免得玩家卡在UI Only里。
	if (bAwaitingRelease && GetWorld() && GetWorld()->GetRealTimeSeconds() - awaitStartRealTime > kMaxAwaitReleaseSeconds) {
		Close();
	}
}

FReply UForeverThrowListWidget::NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) {
	FKey throwKey = EKeys::Three;
	if (UGameInstance* gameInstance = GetGameInstance()) {
		if (UForeverKeyBindingSubsystem* keyBindings = gameInstance->GetSubsystem<UForeverKeyBindingSubsystem>()) {
			throwKey = keyBindings->GetKey(TEXT("ThrowItem"));
		}
	}
	if (InKeyEvent.GetKey() == throwKey) {
		// 列表还在(没点任何按钮，只关闭，什么都不激活)，或者点完按钮后在等松开(这时才切回游戏)。
		Close();
		return FReply::Handled();
	}
	return Super::NativeOnKeyUp(InGeometry, InKeyEvent);
}
