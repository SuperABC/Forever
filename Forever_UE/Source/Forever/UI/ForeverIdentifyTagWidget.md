# ForeverIdentifyTagWidget

## 职责
识别模式下挂在人物头上的文字标签控件(放进世界空间的 `UWidgetComponent`，见 `Player/ForeverIdentifyComponent.md`)：任意多行文字，白色加黑色描边，最下面一行贴着控件底边。

## 关键设计
- **行靠下排列**：根是铺满的 `UVerticalBox`，第一个子项是 `USpacer`(Fill 占满剩余空间)，把后面所有行挤到控件底部——所以行数增加时往上长，最后一行始终在底边。配合 `UWidgetComponent` 的 `Pivot(0.5, 1.0)`，底边中点对齐头顶，最后一行就在头上方一点。`drawSize` 的高度要留够最多行数。
- **描边**：`FSlateFontInfo::OutlineSettings`(`outlineSize`/黑色)，字号 `fontSize`，都是 `EditDefaultsOnly`。描边宽度单位是像素，描边向字形两侧各伸出这么多，太大会盖住白色的芯(曾经因为白字发黑误以为3太厚；白字亮度正常后3是合适的)，默认3。`OutlineSize`是整数，最小1，没法设0.2；想让描边相对字形更细，用`UForeverIdentifyComponent::resolutionScale`把字画得更大(描边仍是1像素，相对粗细 = 1/(字号×倍数))。文字白色、水平居中。
- **控件树在 C++ 的 `Initialize()` 里搭**，没有 WBP 也能用；`UForeverIdentifyComponent::tagWidgetClass` 可以换成自己做的 WBP(根控件已存在就不再搭，但 WBP 要自己提供 `SetLines` 用到的 `linesBox`，目前没有 BindWidget 约定，真要换 WBP 时再补)。
- 内容只在 `SetLines` 时设置一次，不会自己更新。

## 依赖关系
- 被谁依赖：`UForeverIdentifyComponent`(创建标签时调 `SetLines`)。
