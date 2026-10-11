# ForeverHUD

## 职责
瞄准时在屏幕中心画一个准心。用`AHUD`原生`DrawLine`画四条短线拼成"+",不依赖`UUserWidget`/Widget Blueprint资产,是临时效果。

## 关键设计
- `DrawHUD()`里`Cast<AForeverCharacter>(GetOwningPawn())`取当前被该HUD所属PlayerController占有的Pawn(玩家初始角色或`ChangeControlChange`切换过去的`ACitizenElement`,两者都继承`AForeverCharacter`),`IsAiming()`为true时画。
- `AForeverCharacter::IsAiming()`是公开getter,转发`bIsAiming`(瞄准功能见`ForeverWeaponComponent.md`"瞄准"一节)——准心显示和瞄准状态是同一个变量,不单独维护"是否显示准心"状态。
- 用`Canvas->SizeX/SizeY`算屏幕中心,中心留`gap`(6px)半径空当避免交叉点糊成一团,四条短线向外延伸`armLength`(8px),参数为目测值,可直接改这两个数字/`FLinearColor`调整。

## 依赖关系
- `AForeverGameMode`的`HUDClass = AForeverHUD::StaticClass()`。
- 依赖`AForeverCharacter::IsAiming()`。

## 待办/后续阶段
- 换成真正的准心贴图资产,并根据武器`baseSpread`动态改变张开程度(目前是固定尺寸的"+")。
