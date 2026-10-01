#include "UI/InventoryWidget.h"
#include "UI/AssetButtonWidget.h"

#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

#include "Framework/ForeverFrameworkActor.h"
#include "Framework/ForeverAssetFrameworkComponent.h"

#include "player/player.h"
#include "player/asset.h"
#include "map/room.h"

Player* UForeverInventoryWidget::GetPlayer() const {
	if (AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()))) {
		return framework->GetPlayer();
	}
	return nullptr;
}

FString UForeverInventoryWidget::BodyPartToPath(const FString& part) {
	if (part == TEXT("LeftHand")) return TEXT("left");
	if (part == TEXT("RightHand")) return TEXT("right");
	if (part == TEXT("BackPack")) return TEXT("back");
	if (part == TEXT("LeftShoulder")) return TEXT("leftShoulder");
	if (part == TEXT("RightShoulder")) return TEXT("rightShoulder");
	return FString();
}

Asset* UForeverInventoryWidget::ResolveBodyContainer() const {
	return bodyContainerStack.Num() > 0 ? bodyContainerStack.Last() : nullptr;
}

Asset* UForeverInventoryWidget::ResolveRoomContainer() const {
	return roomContainerStack.Num() > 0 ? roomContainerStack.Last() : nullptr;
}

FString UForeverInventoryWidget::BuildBreadcrumb(const FString& root, const TArray<Asset*>& stack) {
	FString text = root;
	for (Asset* container : stack) {
		text += TEXT(" > ") + FString(UTF8_TO_TCHAR(container->GetType().c_str()));
	}
	return text;
}

void UForeverInventoryWidget::NativeConstruct() {
	Super::NativeConstruct();

	if (LeftHandButton) LeftHandButton->OnClicked.AddDynamic(this, &UForeverInventoryWidget::HandleClickLeftHand);
	if (RightHandButton) RightHandButton->OnClicked.AddDynamic(this, &UForeverInventoryWidget::HandleClickRightHand);
	if (BackPackButton) BackPackButton->OnClicked.AddDynamic(this, &UForeverInventoryWidget::HandleClickBackPack);
	if (LeftShoulderButton) LeftShoulderButton->OnClicked.AddDynamic(this, &UForeverInventoryWidget::HandleClickLeftShoulder);
	if (RightShoulderButton) RightShoulderButton->OnClicked.AddDynamic(this, &UForeverInventoryWidget::HandleClickRightShoulder);
	if (UseButton) UseButton->OnClicked.AddDynamic(this, &UForeverInventoryWidget::HandleUse);
	if (DropButton) DropButton->OnClicked.AddDynamic(this, &UForeverInventoryWidget::HandleDrop);
	if (PickButton) PickButton->OnClicked.AddDynamic(this, &UForeverInventoryWidget::HandlePick);
	if (ResetButton) ResetButton->OnClicked.AddDynamic(this, &UForeverInventoryWidget::HandleReset);
	if (CloseButton) CloseButton->OnClicked.AddDynamic(this, &UForeverInventoryWidget::HandleClose);

	SetVisibility(ESlateVisibility::Collapsed); // 默认不显示，没按B就不弹出
}

void UForeverInventoryWidget::TogglePanel() {
	if (GetVisibility() == ESlateVisibility::Visible) ClosePanel();
	else InitPanel();
}

void UForeverInventoryWidget::InitPanel() {
	SetVisibility(ESlateVisibility::Visible);

	currentBodyPart = TEXT("BackPack");
	bodyContainerStack.Empty();
	roomContainerStack.Empty();
	ClearSelect();
	RefreshBody();
	RefreshRoom();

	if (APlayerController* playerController = GetOwningPlayer()) {
		playerController->bShowMouseCursor = true;
		UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(playerController, this, EMouseLockMode::DoNotLock, false);
		SetKeyboardFocus();
	}
}

void UForeverInventoryWidget::ClosePanel() {
	SetVisibility(ESlateVisibility::Collapsed);

	if (APlayerController* playerController = GetOwningPlayer()) {
		playerController->bShowMouseCursor = false;
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(playerController);
		UWidgetBlueprintLibrary::SetFocusToGameViewport();
	}
}

void UForeverInventoryWidget::ClickBody(const FString& part) {
	currentBodyPart = part;
	bodyContainerStack.Empty();
	ClearSelect();
	RefreshBody();
}

void UForeverInventoryWidget::ResetRoom() {
	roomContainerStack.Empty();
	ClearSelect();
	RefreshRoom();
}

void UForeverInventoryWidget::ClearSelect() {
	if (UAssetButtonWidget* row = selectedBody.Get()) row->SetRowSelected(false);
	if (UAssetButtonWidget* row = selectedRoom.Get()) row->SetRowSelected(false);
	selectedBody = nullptr;
	selectedRoom = nullptr;
}

void UForeverInventoryWidget::SelectRow(UAssetButtonWidget* row) {
	if (!row) return;

	if (row->IsRoomSide()) {
		if (UAssetButtonWidget* prev = selectedRoom.Get()) prev->SetRowSelected(false);
		selectedRoom = row;
		lastSelectionIsRoom = true;
	} else {
		if (UAssetButtonWidget* prev = selectedBody.Get()) prev->SetRowSelected(false);
		selectedBody = row;
		lastSelectionIsRoom = false;
	}
	row->SetRowSelected(true);
}

void UForeverInventoryWidget::RefreshBody() {
	if (!BodyList || !AssetButtonClass) return;

	BodyList->ClearChildren();
	selectedBody = nullptr;

	Player* player = GetPlayer();
	if (!player) return;

	if (Asset* container = ResolveBodyContainer()) {
		// ResolveBodyContainer()只在bodyContainerStack非空(已经双击进入过某个容器)时才
		// 返回非空，这里必然要加"返回上一级"占位行。
		UAssetButtonWidget* backRow = CreateWidget<UAssetButtonWidget>(this, AssetButtonClass);
		if (backRow) {
			backRow->SetupAsBack(this, false);
			BodyList->AddChild(backRow);
		}
		for (const auto& [name, asset] : container->GetContents()) {
			UAssetButtonWidget* row = CreateWidget<UAssetButtonWidget>(this, AssetButtonClass);
			if (!row) continue;
			row->Setup(this, asset, false);
			BodyList->AddChild(row);
		}
	} else {
		// 根视图：永远只显示这个槽位当前那一个Asset(0或1行)，不自动展开它的内容，哪怕它
		// 本身是容器(比如BackPack槽位的PlayerBag)——5个槽位在这一点上完全对称，见类注释。
		Asset* slotAsset = nullptr;
		if (currentBodyPart == TEXT("LeftHand")) slotAsset = player->GetLeftHand();
		else if (currentBodyPart == TEXT("RightHand")) slotAsset = player->GetRightHand();
		else if (currentBodyPart == TEXT("BackPack")) slotAsset = player->GetBackPack();
		else if (currentBodyPart == TEXT("LeftShoulder")) slotAsset = player->GetLeftShoulder();
		else if (currentBodyPart == TEXT("RightShoulder")) slotAsset = player->GetRightShoulder();

		if (slotAsset) {
			UAssetButtonWidget* row = CreateWidget<UAssetButtonWidget>(this, AssetButtonClass);
			if (row) {
				row->Setup(this, slotAsset, false);
				BodyList->AddChild(row);
			}
		}
	}

	if (BodyLabel) BodyLabel->SetText(FText::FromString(BuildBreadcrumb(currentBodyPart, bodyContainerStack)));
}

void UForeverInventoryWidget::RefreshRoom() {
	if (!RoomList || !AssetButtonClass) return;
	RoomList->ClearChildren();
	selectedRoom = nullptr;

	Player* player = GetPlayer();
	Room* room = player ? player->GetCurrentRoom() : nullptr;

	if (RoomLabel) {
		FString root = room ? UTF8_TO_TCHAR(room->GetAddress().c_str()) : TEXT("不在房间中");
		RoomLabel->SetText(FText::FromString(BuildBreadcrumb(root, roomContainerStack)));
	}

	if (!player) return;

	if (Asset* container = ResolveRoomContainer()) {
		// ResolveRoomContainer()只在roomContainerStack非空时才返回非空，这里必然要加
		// "返回上一级"占位行(房间根列表本身不是容器，没有对应的"根视图退出"概念)。
		UAssetButtonWidget* backRow = CreateWidget<UAssetButtonWidget>(this, AssetButtonClass);
		if (backRow) {
			backRow->SetupAsBack(this, true);
			RoomList->AddChild(backRow);
		}
		for (const auto& [name, asset] : container->GetContents()) {
			UAssetButtonWidget* row = CreateWidget<UAssetButtonWidget>(this, AssetButtonClass);
			if (!row) continue;
			row->Setup(this, asset, true);
			RoomList->AddChild(row);
		}
		return;
	}

	if (!room) return;

	for (const auto& [name, asset] : room->GetAssets()) {
		UAssetButtonWidget* row = CreateWidget<UAssetButtonWidget>(this, AssetButtonClass);
		if (!row) continue;
		row->Setup(this, asset, true);
		RoomList->AddChild(row);
	}
}

void UForeverInventoryWidget::HandleClickLeftHand() { ClickBody(TEXT("LeftHand")); }
void UForeverInventoryWidget::HandleClickRightHand() { ClickBody(TEXT("RightHand")); }
void UForeverInventoryWidget::HandleClickBackPack() { ClickBody(TEXT("BackPack")); }
void UForeverInventoryWidget::HandleClickLeftShoulder() { ClickBody(TEXT("LeftShoulder")); }
void UForeverInventoryWidget::HandleClickRightShoulder() { ClickBody(TEXT("RightShoulder")); }
void UForeverInventoryWidget::HandleReset() { ResetRoom(); }
void UForeverInventoryWidget::HandleClose() { ClosePanel(); }

void UForeverInventoryWidget::HandleUse() {
	UAssetButtonWidget* row = selectedBody.Get();
	if (!row) return;
	Asset* asset = row->GetAsset();
	Player* player = GetPlayer();
	if (!asset || !player) return;

	if (asset->Use()) {
		if (Asset* container = ResolveBodyContainer()) {
			container->RemoveContent(asset->GetName());
		} else {
			player->RemoveByPath(TCHAR_TO_UTF8(*BodyPartToPath(currentBodyPart)));
		}
		player->DestroyAsset(asset);
	}

	RefreshBody();
}

void UForeverInventoryWidget::HandleDrop() {
	UAssetButtonWidget* row = selectedBody.Get();
	if (!row) return;
	Asset* asset = row->GetAsset();
	Player* player = GetPlayer();
	if (!asset || !player) return;

	Room* room = player->GetCurrentRoom();
	if (!room) return; // 不在任何房间里，没法丢到地上

	if (Asset* container = ResolveBodyContainer()) {
		container->RemoveContent(asset->GetName());
	} else {
		player->RemoveByPath(TCHAR_TO_UTF8(*BodyPartToPath(currentBodyPart)));
	}

	player->AddByPath("room", asset);

	if (AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()))) {
		if (UForeverAssetFrameworkComponent* assetFramework = framework->GetAssetFramework()) {
			APawn* pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
			FVector location = pawn ? pawn->GetActorLocation() : FVector::ZeroVector;
			assetFramework->SpawnWorldAsset(asset, location);
		}
	}

	RefreshBody();
	if (!ResolveRoomContainer()) RefreshRoom(); // 当前在看房间根列表时才需要刷新
}

void UForeverInventoryWidget::HandlePick() {
	UAssetButtonWidget* row = selectedRoom.Get();
	if (!row) return;
	Asset* asset = row->GetAsset();
	Player* player = GetPlayer();
	if (!asset || !player) return;

	if (Asset* container = ResolveRoomContainer()) {
		container->RemoveContent(asset->GetName());
	} else if (Room* room = player->GetCurrentRoom()) {
		room->RemoveAsset(asset->GetName());
	}

	if (AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()))) {
		if (UForeverAssetFrameworkComponent* assetFramework = framework->GetAssetFramework()) {
			assetFramework->DestroyWorldAsset(asset);
		}
	}

	bool added = false;
	if (Asset* container = ResolveBodyContainer()) {
		added = container->AddContent(asset);
	} else {
		added = player->AddByPath(TCHAR_TO_UTF8(*BodyPartToPath(currentBodyPart)), asset);
	}

	if (!added) {
		// 放不进去(槽位规则不满足，比如武器想放进手/容量不够)——原样放回房间。
		if (Asset* container = ResolveRoomContainer()) container->AddContent(asset);
		else player->AddByPath("room", asset);
	}

	RefreshRoom();
	RefreshBody();
}

void UForeverInventoryWidget::EnterRow(UAssetButtonWidget* row) {
	if (!row) return;
	Asset* asset = row->GetAsset();
	if (!asset || !asset->IsContainer()) return;

	if (row->IsRoomSide()) {
		roomContainerStack.Add(asset);
		ClearSelect();
		RefreshRoom();
	} else {
		bodyContainerStack.Add(asset);
		ClearSelect();
		RefreshBody();
	}
}

void UForeverInventoryWidget::ExitRow(UAssetButtonWidget* row) {
	if (!row || !row->IsBackRow()) return;

	if (row->IsRoomSide()) {
		if (roomContainerStack.Num() > 0) roomContainerStack.Pop();
		ClearSelect();
		RefreshRoom();
	} else {
		if (bodyContainerStack.Num() > 0) bodyContainerStack.Pop();
		ClearSelect();
		RefreshBody();
	}
}
