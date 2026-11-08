# ShopRowWidget.h / ShopRowWidget.cpp

## 职责
商店列表的一行：`TypeText`(商品类型id)、`PriceText`(单价)、`StockText`(库存)，右边`QuantityBox`(EditableTextBox)和`MinusButton`/`PlusButton`。数量可以点按钮调，也可以直接在输入框里写，始终夹在[0, 库存]内，非数字/空串当0。数量变化时通过`SetupRow`传入的回调通知`ShopWidget::Refresh()`。

## 蓝图
`WBP_ShopRow`(父类ShopRowWidget)，6个控件都必须有且同名。
