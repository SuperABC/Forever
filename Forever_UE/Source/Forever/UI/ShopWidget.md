# ShopWidget.h / ShopWidget.cpp

## 职责
商店界面。玩家操控的市民和店员对话选"购买汉堡"后，`open_shop` Change经`UForeverStoryFrameworkComponent::ApplyOpenShop`调`OpenShop(Organization*)`打开，并记住店员所属的组织(以后界面列该组织仓库里所有商品，这次只有一个按钮)。

## 蓝图
`WBP_Shop`(父类ShopWidget)：必须有名为`BuyButton`的Button，可选`CloseButton`。在`BP_ForeverPlayerController`里设Shop Widget Class。

## 购买
`BuyButton`固定买一个汉堡，调`Economy::PlayerPurchase`，钱够且背包放得下才扣钱；结果用`AddOnScreenDebugMessage(9200, ...)`打在屏幕左上角。Esc或CloseButton关闭。输入模式切换同PausePanelWidget(UI Only+鼠标)，不改时间缩放。
