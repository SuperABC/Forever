#include "UI/ShopWidget.h"

#include "Framework/ForeverFrameworkActor.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#include "society/economy.h"
#include "society/society.h"
#include "industry/industry.h"
#include "player/player.h"
#include "populace/citizen.h"

namespace {
	// 屏幕左上角调试信息的key，避开已占用的9000/9102/200/201/202。
	constexpr int32 kShopMessageKey = 9200;

	// 目前商店只卖这一样东西，以后界面列出组织仓库里的商品时按选中的商品来。
	constexpr const char* kBurgerAssetId = "asset_burger";
}

void UShopWidget::NativeConstruct() {
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);
	SetIsFocusable(true);

	if (BuyButton) BuyButton->OnClicked.AddDynamic(this, &UShopWidget::HandleBuyClicked);
	if (CloseButton) CloseButton->OnClicked.AddDynamic(this, &UShopWidget::HandleCloseClicked);
}

FReply UShopWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) {
	if (InKeyEvent.GetKey() == EKeys::Escape) {
		CloseShop();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void UShopWidget::OpenShop(Organization* inOrganization) {
	organization = inOrganization;
	if (IsOpen()) return;

	SetVisibility(ESlateVisibility::Visible);
	if (APlayerController* playerController = GetOwningPlayer()) {
		playerController->bShowMouseCursor = true;
		UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(playerController, this, EMouseLockMode::DoNotLock, false);
		SetKeyboardFocus();
	}
}

void UShopWidget::CloseShop() {
	if (!IsOpen()) return;

	SetVisibility(ESlateVisibility::Collapsed);
	organization = nullptr;
	if (APlayerController* playerController = GetOwningPlayer()) {
		playerController->bShowMouseCursor = false;
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(playerController);
		UWidgetBlueprintLibrary::SetFocusToGameViewport();
	}
}

void UShopWidget::HandleCloseClicked() {
	CloseShop();
}

void UShopWidget::HandleBuyClicked() {
	auto say = [](const TCHAR* text, const FColor& color) {
		if (GEngine) GEngine->AddOnScreenDebugMessage(kShopMessageKey, 3.f, color, text);
	};

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()));
	Society* society = framework ? framework->GetSociety() : nullptr;
	Industry* industry = framework ? framework->GetIndustry() : nullptr;
	Player* player = framework ? framework->GetPlayer() : nullptr;
	Citizen* buyer = framework ? framework->GetControlledCitizen() : nullptr;
	if (!society || !industry || !player || !buyer || !organization) {
		say(TEXT("购买失败：当前没有操控市民或商店无效。"), FColor::Red);
		return;
	}

	switch (Economy::PlayerPurchase(*society, *industry, *player, organization, buyer, kBurgerAssetId)) {
	case Economy::PurchaseResult::Success:
		say(TEXT("购买成功：汉堡已放入背包。"), FColor::Green);
		break;
	case Economy::PurchaseResult::NoMoney:
		say(TEXT("购买失败：钱不够。"), FColor::Yellow);
		break;
	case Economy::PurchaseResult::NoSpace:
		say(TEXT("购买失败：背包里放不下(没有背包或已满)，没有扣钱。"), FColor::Yellow);
		break;
	default:
		say(TEXT("购买失败：商品无效。"), FColor::Red);
		break;
	}
}
