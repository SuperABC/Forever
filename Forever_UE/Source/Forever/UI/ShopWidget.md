# ShopWidget.h / ShopWidget.cpp

## 职责
商店界面。玩家操控的市民和店员对话选"购买汉堡"后，`open_shop` Change经`UForeverStoryFrameworkComponent::ApplyOpenShop`调`OpenShop(Organization*)`打开，列出该组织名下所有仓库的商品(`Economy::ListShopItems`)，每种商品一行(`ShopRowWidget`)。

## 蓝图
`WBP_Shop`(父类ShopWidget)，必须有：
- `ShopNameText`(TextBlock，可选)：显示组织名(`OrganizationMod::GetName()`)；
- `RowScrollBox`(ScrollBox)：放商品行；
- `TotalCountText`、`TotalVolumeText`、`TotalPriceText`(TextBlock)：底部汇总；
- `BuyButton`(Button)；可选`CloseButton`。
默认值里设`Shop Row Class`=`WBP_ShopRow`；在`BP_ForeverPlayerController`里设Shop Widget Class。

## 汇总与标红
每次数量变化`Refresh()`：个数、体积(Σ数量×size)、总价(分)。携带可行性来自`Player::PlanCarry`(先装容器，再最大/第二大拿手上)：
- 没有任何容器：显示`个数：n / 空手数`，不可行时个数标红；
- 有容器：个数不限，显示`体积：x / 上限`(上限=容器剩余空间+手上能多拿的最大几件)，不可行时体积标红；
- 总价超过账本余额(不含存款/国债)时总价标红。
`BuyButton`：三项有任一不满足或购物车为空则什么都不做；否则`Economy::PlayerPurchaseCart`，成功后关闭界面。Esc或CloseButton关闭。输入模式切换同PausePanelWidget(UI Only+鼠标)。打开时记下当前全局时间膨胀并置0暂停世界，关闭时还原。
