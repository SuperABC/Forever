# ForeverHUD.h / .cpp

## 职责

瞄准时在屏幕中心画一个准心——用户明确说是"临时"的，先用`AHUD`原生`DrawLine`画四条短线
拼成一个"+"，不新建`UUserWidget`/不需要在编辑器里配一份对应的Widget Blueprint资产。这次
武器系统一路都是先用`DrawDebugLine`/`AddOnScreenDebugMessage`这类零资产依赖的原生调用
验证手感（子弹轨迹、命中提示、弹药显示），准心延续同一个思路。

## 关键设计

- `DrawHUD()`里`Cast<AForeverCharacter>(GetOwningPawn())`拿到当前被这个HUD所属
  PlayerController占有的Pawn——不管这个Pawn是玩家自己的初始角色还是`ChangeControlChange`
  切换过去的`ACitizenElement`（两者都继承`AForeverCharacter`），只要`IsAiming()`为true
  就画。
- `AForeverCharacter::IsAiming()`是新增的公开getter，直接转发`bIsAiming`（瞄准功能本身
  见`ForeverWeaponComponent.md`"瞄准"一节），这次没有单独的"是否显示准心"状态——准心
  显示和瞄准状态是同一件事，没有必要拆成两个变量。
- 用`Canvas->SizeX/SizeY`算屏幕中心，中心留一个`gap`(6px)半径的空当不画满，避免准心
  正中心的交叉点糊成一团，四条短线各自往外延伸`armLength`(8px)，参数都是目测给的，
  具体尺寸/颜色需要在PIE里再调，直接改这两个数字/`FLinearColor`即可。

## 依赖关系

- `AForeverGameMode`的`HUDClass = AForeverHUD::StaticClass()`——这次第一次给这个项目设置
  `HUDClass`，此前一直是引擎默认`AHUD`(不画任何东西)。
- 依赖`AForeverCharacter::IsAiming()`。

## 待办/后续阶段

- 换成真正的准心贴图资产/根据武器`baseSpread`动态改变准心张开程度——这次只有一个固定
  尺寸的"+"，不跟随散射角度变化。
