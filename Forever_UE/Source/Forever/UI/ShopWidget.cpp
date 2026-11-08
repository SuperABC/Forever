#include "UI/ShopWidget.h"

#include "Framework/ForeverFrameworkActor.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "UI/ShopRowWidget.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#include <algorithm>
#include <functional>

#include "society/economy.h"
#include "society/society.h"
#include "society/organization.h"
#include "industry/industry.h"
#include "player/player.h"
#include "society/ledger.h"
#include "populace/citizen.h"

namespace {
	// 屏幕左上角调试信息的key，避开已占用的9000/9102/200/201/202。
	constexpr int32 kShopMessageKey = 9200;
}

void UShopWidget::NativeConstruct() {
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);
	SetIsFocusable(true);

	if (TotalPriceText) normalColor = TotalPriceText->GetColorAndOpacity();
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

	if (ShopNameText) {
		OrganizationMod* mod = organization ? organization->GetMod() : nullptr;
		ShopNameText->SetText(FText::FromString(mod ? UTF8_TO_TCHAR(mod->GetName()) : TEXT("")));
	}

	items.clear();
	rows.Reset();
	if (RowScrollBox) RowScrollBox->ClearChildren();

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()));
	Industry* industry = framework ? framework->GetIndustry() : nullptr;
	Player* player = framework ? framework->GetPlayer() : nullptr;
	if (industry && player && organization && ShopRowClass && RowScrollBox) {
		items = Economy::ListShopItems(*industry, *player, organization);
		for (const Economy::ShopItem& item : items) {
			UShopRowWidget* row = CreateWidget<UShopRowWidget>(this, ShopRowClass);
			row->SetupRow(UTF8_TO_TCHAR(item.productType.c_str()), item.price, item.stock, [this]() { Refresh(); });
			RowScrollBox->AddChild(row);
			rows.Add(row);
		}
	}
	Refresh();

	if (IsOpen()) return;

	SetVisibility(ESlateVisibility::Visible);
	previousDilation = UGameplayStatics::GetGlobalTimeDilation(this);
	UGameplayStatics::SetGlobalTimeDilation(this, 0.f);
	if (APlayerController* playerController = GetOwningPlayer()) {
		playerController->bShowMouseCursor = true;
		UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(playerController, this, EMouseLockMode::DoNotLock, false);
		SetKeyboardFocus();
	}
}

void UShopWidget::CloseShop() {
	if (!IsOpen()) return;

	SetVisibility(ESlateVisibility::Collapsed);
	UGameplayStatics::SetGlobalTimeDilation(this, previousDilation);
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

bool UShopWidget::Refresh() {
	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()));
	Society* society = framework ? framework->GetSociety() : nullptr;
	Player* player = framework ? framework->GetPlayer() : nullptr;
	Citizen* buyer = framework ? framework->GetControlledCitizen() : nullptr;

	// 每件一个size，和价格、个数一起汇总。
	std::vector<float> sizes;
	int64 totalPrice = 0;
	float totalVolume = 0.f;
	for (int32 i = 0; i < rows.Num() && i < static_cast<int32>(items.size()); ++i) {
		const int32 quantity = rows[i]->GetQuantity();
		totalPrice += items[i].price * quantity;
		totalVolume += items[i].size * quantity;
		for (int32 k = 0; k < quantity; ++k) sizes.push_back(items[i].size);
	}
	const int32 totalCount = static_cast<int32>(sizes.size());

	Player::CarryPlan plan;
	if (player) plan = player->PlanCarry(sizes);
	const int64 balance = (society && buyer) ? society->GetLedger().Balance(Party::Of(buyer)) : 0;
	const bool affordable = totalPrice <= balance;

	// 体积上限=所有容器剩余空间+手上能多拿的最大几件(空手数那么多件)；没有容器时体积不受限，只受空手个数限制。
	std::vector<float> sorted = sizes;
	std::sort(sorted.begin(), sorted.end(), std::greater<float>());
	float volumeLimit = plan.containerSpace;
	for (int32 i = 0; i < plan.freeHands && i < static_cast<int32>(sorted.size()); ++i) volumeLimit += sorted[i];

	const bool countOver = !plan.feasible && !plan.hasContainer;
	const bool volumeOver = !plan.feasible && plan.hasContainer;
	const FSlateColor red(FLinearColor::Red);
	auto setText = [&](UTextBlock* block, const FString& text, bool over) {
		if (!block) return;
		block->SetText(FText::FromString(text));
		block->SetColorAndOpacity(over ? red : normalColor);
	};

	setText(TotalCountText, plan.hasContainer ? FString::Printf(TEXT("个数：%d"), totalCount)
		: FString::Printf(TEXT("个数：%d / %d"), totalCount, plan.freeHands), countOver);
	setText(TotalVolumeText, plan.hasContainer ? FString::Printf(TEXT("体积：%.2f / %.2f"), totalVolume, volumeLimit)
		: FString::Printf(TEXT("体积：%.2f"), totalVolume), volumeOver);
	setText(TotalPriceText, FString::Printf(TEXT("总价：%.2f元 / 余额：%.2f元"), totalPrice / 100.0, balance / 100.0), !affordable);

	return totalCount > 0 && plan.feasible && affordable;
}

void UShopWidget::HandleBuyClicked() {
	// 数量/放得下/付得起任一不满足就什么都不做(对应的数字已经标红)。
	if (!Refresh()) return;

	AForeverFrameworkActor* framework = Cast<AForeverFrameworkActor>(
		UGameplayStatics::GetActorOfClass(GetWorld(), AForeverFrameworkActor::StaticClass()));
	Society* society = framework ? framework->GetSociety() : nullptr;
	Industry* industry = framework ? framework->GetIndustry() : nullptr;
	Player* player = framework ? framework->GetPlayer() : nullptr;
	Citizen* buyer = framework ? framework->GetControlledCitizen() : nullptr;
	if (!society || !industry || !player || !buyer || !organization) {
		if (GEngine) GEngine->AddOnScreenDebugMessage(kShopMessageKey, 3.f, FColor::Red, TEXT("购买失败：当前没有操控市民或商店无效。"));
		return;
	}

	std::vector<std::pair<std::string, int>> cart;
	for (int32 i = 0; i < rows.Num() && i < static_cast<int32>(items.size()); ++i) {
		if (rows[i]->GetQuantity() > 0) cart.emplace_back(items[i].productType, rows[i]->GetQuantity());
	}
	if (Economy::PlayerPurchaseCart(*society, *industry, *player, organization, buyer, cart) == Economy::PurchaseResult::Success) {
		CloseShop();
	}
}
