#include "UI/InventoryWidget.h"
#include "UI/AssetButtonWidget.h"

#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "InputCoreTypes.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Framework/Application/SlateApplication.h"

#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverAssetFrameworkComponent.h"
#include "Player/ForeverCharacter.h"
#include "Player/ForeverThrowableComponent.h"

#include "player/player.h"
#include "player/asset.h"
#include "map/room.h"

namespace {

	// "附近"列表的范围：玩家周围10米(Unreal单位是厘米)。
	constexpr float kNearbyRadiusCm = 1000.f;

	constexpr EInvZone kSlotZones[] = {
		EInvZone::LeftShoulder, EInvZone::RightShoulder, EInvZone::LeftHand, EInvZone::RightHand, EInvZone::Back,
	};

	// 进入过的容器栈可能因为拖拽移走/销毁物品而失效：从栈底开始逐层确认还在上一层里，
	// 不在了就截断到这里(确认之前不解引用栈里的指针)。
	void TrimStack(TArray<Asset*>& stack, TFunctionRef<bool(Asset*)> inBase) {
		for (int32 i = 0; i < stack.Num(); ++i) {
			bool found = false;
			if (i == 0) {
				found = inBase(stack[0]);
			} else {
				for (const auto& [name, content] : stack[i - 1]->GetContents()) {
					if (content == stack[i]) { found = true; break; }
				}
			}
			if (!found) {
				stack.SetNum(i);
				return;
			}
		}
	}

	void SortAssets(TArray<Asset*>& assets) {
		assets.Sort([](const Asset& a, const Asset& b) {
			if (a.GetType() != b.GetType()) return a.GetType() < b.GetType();
			return a.GetName() < b.GetName();
		});
	}

}

Player* UInventoryWidget::GetPlayer() const {
	if (AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()))) {
		return framework->GetPlayer();
	}
	return nullptr;
}

UForeverAssetFrameworkComponent* UInventoryWidget::GetAssetFramework() const {
	if (AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()))) {
		return framework->GetAssetFramework();
	}
	return nullptr;
}

FString UInventoryWidget::SlotName(EInvZone zone) {
	switch (zone) {
	case EInvZone::LeftShoulder: return TEXT("LeftShoulder");
	case EInvZone::RightShoulder: return TEXT("RightShoulder");
	case EInvZone::LeftHand: return TEXT("LeftHand");
	case EInvZone::RightHand: return TEXT("RightHand");
	case EInvZone::Back: return TEXT("BackPack");
	default: return FString();
	}
}

FString UInventoryWidget::DescribeAsset(const Asset* asset, int amount) {
	FString label = UTF8_TO_TCHAR(asset->GetType().c_str());
	if (amount != 1) label += FString::Printf(TEXT(" x%d"), amount);
	if (asset->IsContainer()) label += TEXT(" [容器]");
	return label;
}

UButton* UInventoryWidget::SlotButton(EInvZone zone) const {
	switch (zone) {
	case EInvZone::LeftShoulder: return LeftShoulderButton;
	case EInvZone::RightShoulder: return RightShoulderButton;
	case EInvZone::LeftHand: return LeftHandButton;
	case EInvZone::RightHand: return RightHandButton;
	case EInvZone::Back: return BackPackButton;
	default: return nullptr;
	}
}

Asset* UInventoryWidget::GetSlotAsset(EInvZone zone) const {
	Player* player = GetPlayer();
	if (!player || !InventoryTransfer::IsSlot(zone)) return nullptr;
	return player->GetByPath(TCHAR_TO_UTF8(*InventoryTransfer::SlotPath(zone)));
}

bool UInventoryWidget::FindSlotAt(const FVector2D& screenPosition, EInvZone& outZone) const {
	for (EInvZone zone : kSlotZones) {
		UButton* button = SlotButton(zone);
		if (button && button->IsVisible() && button->GetCachedGeometry().IsUnderLocation(screenPosition)) {
			outZone = zone;
			return true;
		}
	}
	return false;
}

Asset* UInventoryWidget::ResolveBodyContainer() const {
	if (bodyContainerStack.Num() > 0) return bodyContainerStack.Last();
	Asset* slotAsset = GetSlotAsset(currentSlot);
	return (slotAsset && slotAsset->IsContainer()) ? slotAsset : nullptr;
}

Asset* UInventoryWidget::ResolveRoomContainer() const {
	return roomContainerStack.Num() > 0 ? roomContainerStack.Last() : nullptr;
}

Asset* UInventoryWidget::ResolveNearbyContainer() const {
	return nearbyContainerStack.Num() > 0 ? nearbyContainerStack.Last() : nullptr;
}

FString UInventoryWidget::BuildBreadcrumb(const FString& root, const TArray<Asset*>& stack) {
	FString text = root;
	for (Asset* container : stack) {
		text += TEXT(" > ") + FString(UTF8_TO_TCHAR(container->GetType().c_str()));
	}
	return text;
}

void UInventoryWidget::NativeConstruct() {
	Super::NativeConstruct();

	// 槽位按钮只负责外观：鼠标事件让它直接落到整个面板上，由面板按位置判断点中/拖中了哪个槽位
	// (SButton会吞掉按下事件，拖拽检测和松手落点都到不了这一层)。
	for (EInvZone zone : kSlotZones) {
		if (UButton* button = SlotButton(zone)) {
			slotStyles[static_cast<int32>(zone)].Capture(button); // 先记下WBP里配置的样式
			button->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
	if (UseButton) UseButton->OnClicked.AddDynamic(this, &UInventoryWidget::HandleUse);
	if (CloseButton) CloseButton->OnClicked.AddDynamic(this, &UInventoryWidget::HandleClose);

	SetVisibility(ESlateVisibility::Collapsed); // 默认不显示，没按B就不弹出
}

void UInventoryWidget::TogglePanel() {
	if (GetVisibility() == ESlateVisibility::Visible) ClosePanel();
	else InitPanel();
}

void UInventoryWidget::InitPanel() {
	SetVisibility(ESlateVisibility::Visible);

	currentSlot = EInvZone::Back;
	bodyContainerStack.Empty();
	roomContainerStack.Empty();
	nearbyContainerStack.Empty();
	bSlotPressed = false;
	ClearSelect();
	if (MessageText) MessageText->SetText(FText::GetEmpty());
	RefreshAll();

	if (APlayerController* playerController = GetOwningPlayer()) {
		playerController->bShowMouseCursor = true;
		UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(playerController, this, EMouseLockMode::DoNotLock, false);
		SetKeyboardFocus();
	}
}

void UInventoryWidget::ClosePanel() {
	SetVisibility(ESlateVisibility::Collapsed);

	if (APlayerController* playerController = GetOwningPlayer()) {
		playerController->bShowMouseCursor = false;
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(playerController);
		UWidgetBlueprintLibrary::SetFocusToGameViewport();
	}
}

void UInventoryWidget::ClickSlot(EInvZone zone) {
	currentSlot = zone;
	bodyContainerStack.Empty();
	ClearSelect();
	RefreshBody();
}

void UInventoryWidget::ClearSelect() {
	if (UAssetButtonWidget* row = selectedBody.Get()) row->SetRowSelected(false);
	if (UAssetButtonWidget* row = selectedOther.Get()) row->SetRowSelected(false);
	selectedBody = nullptr;
	selectedOther = nullptr;
}

void UInventoryWidget::SelectRow(UAssetButtonWidget* row) {
	if (!row || row->IsBackRow()) return;

	if (row->GetZone() == EInvZone::Container) {
		if (UAssetButtonWidget* prev = selectedBody.Get()) prev->SetRowSelected(false);
		selectedBody = row;
	} else {
		if (UAssetButtonWidget* prev = selectedOther.Get()) prev->SetRowSelected(false);
		selectedOther = row;
	}
	row->SetRowSelected(true);
}

void UInventoryWidget::ShowMessage(const FString& text) {
	if (MessageText) {
		MessageText->SetText(FText::FromString(text));
	} else if (GEngine) {
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Yellow, text);
	}
}

void UInventoryWidget::RefreshAll() {
	Player* player = GetPlayer();
	if (player) {
		// 拖拽可能把进入过的容器移走或销毁(合并)，先把失效的栈截断，再刷新各列表。
		Asset* slotAsset = GetSlotAsset(currentSlot);
		TrimStack(bodyContainerStack, [slotAsset](Asset* a) { return slotAsset && slotAsset->IsContainer() && [&] {
			for (const auto& [name, content] : slotAsset->GetContents()) if (content == a) return true;
			return false;
		}(); });
		TrimStack(roomContainerStack, [player](Asset* a) {
			Room* room = player->GetCurrentRoom();
			if (!room) return false;
			for (const auto& [name, content] : room->GetAssets()) if (content == a) return true;
			return false;
		});
		TrimStack(nearbyContainerStack, [player](Asset* a) {
			for (const auto& [name, entry] : player->GetWorldAssets()) if (entry.asset == a) return true;
			return false;
		});
	}

	RefreshSlots();
	RefreshBody();
	RefreshRoom();
	RefreshNearby();
}

void UInventoryWidget::RefreshSlots() {
	Player* player = GetPlayer();
	for (EInvZone zone : kSlotZones) {
		UButton* button = SlotButton(zone);
		UTextBlock* text = button ? Cast<UTextBlock>(button->GetChildAt(0)) : nullptr;
		if (!text) continue;

		int32 index = static_cast<int32>(zone);
		if (!bSlotLabelsCaptured) slotBaseLabels[index] = text->GetText().ToString();

		// 槽位按钮尺寸有限，有物品时只显示物品内容，空槽位才显示WBP里原来的部位标签。
		FString label = slotBaseLabels[index];
		if (Asset* asset = player ? GetSlotAsset(zone) : nullptr) {
			label = DescribeAsset(asset, asset->GetCount());
		}
		text->SetText(FText::FromString(label));
	}
	bSlotLabelsCaptured = true;
}

void UInventoryWidget::FillList(UScrollBox* list, TArray<TObjectPtr<UAssetButtonWidget>>& pool, EInvZone zone,
	Asset* parent, bool bBack, const TArray<Asset*>& assets) {
	if (!list || !AssetButtonClass) return;

	// 行对象复用：刷新只重新Setup，不反复CreateWidget(附近的物品可能有很多)。
	int32 needed = assets.Num() + (bBack ? 1 : 0);
	while (pool.Num() < needed) {
		UAssetButtonWidget* row = CreateWidget<UAssetButtonWidget>(this, AssetButtonClass);
		if (!row) break;
		list->AddChild(row);
		pool.Add(row);
	}

	int32 index = 0;
	if (bBack && index < pool.Num()) {
		pool[index]->SetupAsBack(this, zone);
		pool[index]->SetVisibility(ESlateVisibility::Visible);
		++index;
	}
	for (Asset* asset : assets) {
		if (index >= pool.Num()) break;
		pool[index]->Setup(this, asset, zone, parent);
		pool[index]->SetVisibility(ESlateVisibility::Visible);
		++index;
	}
	for (; index < pool.Num(); ++index) pool[index]->SetVisibility(ESlateVisibility::Collapsed);
}

void UInventoryWidget::RefreshBody() {
	// 背包没有变化事件，所有放入/取出/使用/丢弃之后都会走到这里——顺便刷新投掷物缓存，
	// 激活的投掷物被拿走/用完时也能及时切换。
	if (AForeverCharacter* character = Cast<AForeverCharacter>(UGameplayStatics::GetPlayerPawn(GetWorld(), 0))) {
		if (UForeverThrowableComponent* throwable = character->GetThrowableComponent()) throwable->Refresh();
	}

	selectedBody = nullptr;

	TArray<Asset*> assets;
	Asset* container = ResolveBodyContainer();
	if (container) {
		for (const auto& [name, asset] : container->GetContents()) assets.Add(asset);
		SortAssets(assets);
	}
	FillList(BodyList, bodyPool, EInvZone::Container, container, bodyContainerStack.Num() > 0, assets);

	if (BodyLabel) BodyLabel->SetText(FText::FromString(BuildBreadcrumb(SlotName(currentSlot), bodyContainerStack)));
}

void UInventoryWidget::RefreshRoom() {
	selectedOther = nullptr;

	Player* player = GetPlayer();
	Room* room = player ? player->GetCurrentRoom() : nullptr;

	// 不在房间内整块隐藏。
	ESlateVisibility roomVisibility = room ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (RoomList) RoomList->SetVisibility(roomVisibility);
	if (RoomLabel) RoomLabel->SetVisibility(roomVisibility);
	if (!room) {
		roomContainerStack.Empty();
		return;
	}

	if (RoomLabel) {
		FString root = UTF8_TO_TCHAR(room->GetAddress().c_str());
		RoomLabel->SetText(FText::FromString(BuildBreadcrumb(root, roomContainerStack)));
	}

	TArray<Asset*> assets;
	Asset* container = ResolveRoomContainer();
	if (container) {
		for (const auto& [name, asset] : container->GetContents()) assets.Add(asset);
	} else {
		for (const auto& [name, asset] : room->GetAssets()) assets.Add(asset);
	}
	SortAssets(assets);
	FillList(RoomList, roomPool, EInvZone::Room, container, container != nullptr, assets);
}

void UInventoryWidget::RefreshNearby() {
	if (!NearbyList) return;

	Player* player = GetPlayer();
	if (!player) return;

	TArray<Asset*> assets;
	Asset* container = ResolveNearbyContainer();
	if (container) {
		for (const auto& [name, asset] : container->GetContents()) assets.Add(asset);
		SortAssets(assets);
	} else if (UForeverAssetFrameworkComponent* assetFramework = GetAssetFramework()) {
		// 已按距离从近到远排好，不再按名字重排。
		APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		assetFramework->CollectNearby(pawn ? pawn->GetActorLocation() : FVector::ZeroVector, kNearbyRadiusCm, assets);
	}
	FillList(NearbyList, nearbyPool, EInvZone::Nearby, container, container != nullptr, assets);

	if (NearbyLabel) NearbyLabel->SetText(FText::FromString(BuildBreadcrumb(TEXT("附近"), nearbyContainerStack)));
}

void UInventoryWidget::HandleClose() { ClosePanel(); }

void UInventoryWidget::HandleUse() {
	Player* player = GetPlayer();
	if (!player) return;

	Asset* asset = nullptr;
	Asset* container = nullptr;
	if (UAssetButtonWidget* row = selectedBody.Get()) {
		asset = row->GetAsset();
		container = ResolveBodyContainer();
	} else {
		asset = GetSlotAsset(currentSlot);
	}
	if (!asset) return;

	if (asset->Use()) {
		if (container) container->RemoveContent(asset->GetName());
		else player->RemoveByPath(TCHAR_TO_UTF8(*InventoryTransfer::SlotPath(currentSlot)));
		player->DestroyAsset(asset);
	}

	ClearSelect();
	RefreshAll();
}

void UInventoryWidget::EnterRow(UAssetButtonWidget* row) {
	if (!row) return;
	Asset* asset = row->GetAsset();
	if (!asset || !asset->IsContainer()) return;

	ClearSelect();
	switch (row->GetZone()) {
	case EInvZone::Container: bodyContainerStack.Add(asset); RefreshBody(); break;
	case EInvZone::Room: roomContainerStack.Add(asset); RefreshRoom(); break;
	case EInvZone::Nearby: nearbyContainerStack.Add(asset); RefreshNearby(); break;
	default: break;
	}
}

void UInventoryWidget::ExitRow(UAssetButtonWidget* row) {
	if (!row || !row->IsBackRow()) return;

	ClearSelect();
	switch (row->GetZone()) {
	case EInvZone::Container: if (bodyContainerStack.Num() > 0) bodyContainerStack.Pop(); RefreshBody(); break;
	case EInvZone::Room: if (roomContainerStack.Num() > 0) roomContainerStack.Pop(); RefreshRoom(); break;
	case EInvZone::Nearby: if (nearbyContainerStack.Num() > 0) nearbyContainerStack.Pop(); RefreshNearby(); break;
	default: break;
	}
}

FReply UInventoryWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) {
	EInvZone zone = EInvZone::Back;
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && FindSlotAt(InMouseEvent.GetScreenSpacePosition(), zone)) {
		bPressActive = true;
		bSlotPressed = true;
		pressedSlot = zone;
		pressedRow = nullptr;
		pressSource.zone = zone;
		pressSource.asset = GetSlotAsset(zone);
		pressSource.parent = nullptr;
		slotPressPos = InMouseEvent.GetScreenSpacePosition();
		bSlotDragging = false;
		UpdateSlotTints();
		// 拖拽不走UMG的DragDropOperation：它的拖拽框会从"发起拖拽的控件左上角"插值滑向鼠标
		// (150ms)，槽位是整个面板发起的，框就会从屏幕左上角飞过来。这里自己捕获鼠标，超过
		// 拖动阈值后生成一个贴着鼠标的框，松手时在NativeOnMouseButtonUp里结算。列表行同理
		// (见BeginRowPress)，两者用同一套逻辑。
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UInventoryWidget::BeginRowPress(UAssetButtonWidget* row, const FPointerEvent& InMouseEvent) {
	if (!row) return FReply::Unhandled();
	bPressActive = true;
	bSlotPressed = false;
	pressedRow = row;
	pressSource.zone = row->GetZone();
	pressSource.asset = row->GetAsset(); // 返回行为nullptr，不能拖
	pressSource.parent = row->GetParentContainer();
	slotPressPos = InMouseEvent.GetScreenSpacePosition();
	bSlotDragging = false;
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UInventoryWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) {
	if (bPressActive && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton) {
		bPressActive = false;
		bool bWasSlot = bSlotPressed;
		bSlotPressed = false;
		FVector2D position = InMouseEvent.GetScreenSpacePosition();
		UAssetButtonWidget* row = pressedRow.Get();
		pressedRow = nullptr;
		if (row) row->FinishPress(position); // 先还原行的按下样式，下面的刷新可能复用这一行

		if (bSlotDragging) {
			bSlotDragging = false;
			DestroyGhost();
			if (pressSource.asset) PerformDrop(pressSource, bSlotDragAll, position);
		} else if (bWasSlot) {
			EInvZone zone = EInvZone::Back;
			if (FindSlotAt(position, zone) && zone == pressedSlot) ClickSlot(zone);
		} else if (row && row->IsUnderScreenPosition(position)) {
			row->Click();
		}
		bSlotHovered = FindSlotAt(position, hoveredSlot);
		UpdateSlotTints();
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UInventoryWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) {
	FVector2D position = InMouseEvent.GetScreenSpacePosition();

	if (bPressActive) {
		if (!bSlotDragging && FVector2D::Distance(position, slotPressPos) >= FSlateApplication::Get().GetDragTriggerDistance()) {
			// 空槽位/返回行(asset为空)不能拖；Ctrl在拖拽开始的这一刻取。
			if (pressSource.asset) {
				bSlotDragging = true;
				bSlotDragAll = InMouseEvent.IsControlDown();
				CreateGhost(pressSource.asset, bSlotDragAll);
			}
		}
		if (bSlotDragging) UpdateGhost(position);
		return FReply::Handled();
	}

	EInvZone zone = EInvZone::Back;
	bool bHover = FindSlotAt(position, zone);
	if (bHover != bSlotHovered || (bHover && zone != hoveredSlot)) {
		bSlotHovered = bHover;
		hoveredSlot = zone;
		UpdateSlotTints();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UInventoryWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent) {
	Super::NativeOnMouseLeave(InMouseEvent);
	bSlotHovered = false;
	UpdateSlotTints();
}

void UInventoryWidget::NativeDestruct() {
	DestroyGhost();
	Super::NativeDestruct();
}

void UInventoryWidget::UpdateSlotTints() {
	// 槽位按钮是HitTestInvisible，原生悬停/按下样式不再生效；这里按状态把按钮样式的Normal换成
	// 它自己样式里的Hovered/Pressed，效果和默认响应一致。
	for (EInvZone zone : kSlotZones) {
		UButton* button = SlotButton(zone);
		if (!button) continue;
		FInvButtonStyleState::EState state = FInvButtonStyleState::EState::Normal;
		if (bSlotPressed && zone == pressedSlot) state = FInvButtonStyleState::EState::Pressed;
		else if (!bSlotPressed && bSlotHovered && zone == hoveredSlot) state = FInvButtonStyleState::EState::Hovered;
		slotStyles[static_cast<int32>(zone)].Apply(button, state);
	}
}

void UInventoryWidget::CreateGhost(Asset* asset, bool bAll) {
	DestroyGhost();
	if (!AssetButtonClass || !asset) return;

	dragGhost = CreateWidget<UAssetButtonWidget>(GetOwningPlayer(), AssetButtonClass);
	if (!dragGhost) return;
	int amount = bAll ? asset->GetCount() : FMath::Min(1, asset->GetCount());
	dragGhost->SetLabel(FString::Printf(TEXT("%s x%d"), UTF8_TO_TCHAR(asset->GetType().c_str()), amount));
	dragGhost->SetVisibility(ESlateVisibility::HitTestInvisible);
	dragGhost->AddToViewport(1000);
	dragGhost->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));
}

void UInventoryWidget::UpdateGhost(const FVector2D& screenPosition) {
	if (!dragGhost) return;
	FVector2D pixelPosition, viewportPosition;
	USlateBlueprintLibrary::AbsoluteToViewport(this, screenPosition, pixelPosition, viewportPosition);
	dragGhost->SetPositionInViewport(viewportPosition, false);
}

void UInventoryWidget::DestroyGhost() {
	if (dragGhost) {
		dragGhost->RemoveFromParent();
		dragGhost = nullptr;
	}
}

void UInventoryWidget::PerformDrop(const FInvSource& source, bool bAll, const FVector2D& screenPosition) {
	FInvDest dest;
	if (!ResolveDropTarget(screenPosition, dest)) return; // 落在空白处：什么都不做

	Player* player = GetPlayer();
	APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	FVector location = pawn ? pawn->GetActorLocation() : FVector::ZeroVector;

	// 松手这一刻才校验并真正移动。
	FString error;
	if (InventoryTransfer::Transfer(player, GetAssetFramework(), location, source, dest, bAll, error)) {
		ClearSelect();
		if (MessageText) MessageText->SetText(FText::GetEmpty());
		RefreshAll();
	} else if (!error.IsEmpty()) {
		ShowMessage(error);
	}
}

bool UInventoryWidget::ResolveDropTarget(const FVector2D& screenPosition, FInvDest& outDest) const {
	EInvZone slot;
	if (FindSlotAt(screenPosition, slot)) {
		outDest.zone = slot;
		outDest.container = nullptr;
		return true;
	}

	auto hit = [&](UWidget* widget) {
		return widget && widget->IsVisible() && widget->GetCachedGeometry().IsUnderLocation(screenPosition);
	};
	if (hit(BodyList)) {
		outDest.zone = EInvZone::Container;
		outDest.container = ResolveBodyContainer();
		return true;
	}
	if (hit(RoomList)) {
		outDest.zone = EInvZone::Room;
		outDest.container = ResolveRoomContainer();
		return true;
	}
	if (hit(NearbyList)) {
		outDest.zone = EInvZone::Nearby;
		outDest.container = ResolveNearbyContainer();
		return true;
	}
	return false;
}

