# LampDecalComponent.h / LampDecalComponent.cpp

## 职责

`ULampDecalComponent`(继承`UDecalComponent`)：灯的**光斑Decal**，远处代替真实光源。可选组件，哪个灯蓝图需要就添加。

## 关键设计

- **不写死朝向**：灯不全是路灯，方向/位置/大小用组件自己的Transform和`DecalSize`，路灯朝下，墙灯可以侧向，一盏灯可以加多个。淡出沿用`UDecalComponent`自带的`FadeScreenSize`。
- **由`ALampElement`驱动**：`SetLampActive(active, brightness)`——`SetVisibility(active)`；显示时用懒创建的动态材质实例(`CreateDynamicMaterialInstance`)把`intensityScale * brightness`写进材质参数`intensityParameterName`(默认`Brightness`)。
  没选光斑材质时`CreateDynamicMaterialInstance`返回空，直接跳过，不报错。
- **光斑材质由用户在编辑器里建**：材质域Deferred Decal，混合模式Translucent/Additive，径向渐变贴图(中心亮、边缘透明)×颜色×`Brightness`标量参数，输出到Emissive和Opacity。

## 依赖关系

- 依赖：UE `UDecalComponent`。
- 被谁依赖：`Element/LampElement`(`RefreshLampEffects`)。

## 待办/后续阶段

- 无。
