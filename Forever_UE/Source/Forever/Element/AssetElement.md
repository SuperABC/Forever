# AssetElement.h / AssetElement.cpp

## 职责
`AAssetElement`：物品/资产在世界里的可视化表示。由`UForeverAssetFrameworkComponent::SpawnWorldAsset()`在Drop时`SpawnActor(Deferred)`+`Init(asset)`，`DestroyWorldAsset()`(Pick时)销毁；**不持有`Asset`所有权**(`asset`是非owning裸指针，真正所有权在`Room::assets`或某个容器的`contents`里，`EndPlay`置空)。

## 关键设计
- 网格：默认`/Game/Asset/Common/Mesh/Cube.Cube`占位(与`ABuildingElement`同一份)；`AssetMod::meshPath`非空时用`LoadAssetMesh`加载(加载失败回退引擎球体并打Warning)，投掷预览模型也用这个函数以保证和掉落物同一份网格。缩放不在`Init`里设，由生成方经`SpawnActorDeferred`的Transform传入(物理体在BeginPlay时就按这个缩放创建)。
- 物理：Movable + `PhysicsActor`碰撞预设 + 模拟物理 + 重力，玩家角色可以直接推动；`SetAngularDamping(3)`防止球体(投掷物)落地后无限滚动，不加线性阻尼以免飞行弧线偏离预览曲线。
- `Launch(velocity, thrower)`：投掷初速度；`thrower`的碰撞被忽略(起点在投掷者胶囊体附近，否则会被顶开)；开CCD防止小而快的球体穿过薄墙/地面。


## 弹选项的触发盒
资产有脚本(`asset->GetScript()`非空)才在`Init`建触发盒(世界120x120x80cm，随根缩放反算；与`AVehicleElement::BuildProximityBox`同一思路)，玩家的`AForeverCharacter`进入时把`asset->GetOptions()`快照推给MeetOption；`RefreshOptions()`在`add_option/remove_option`后刷新；`EndPlay`撤掉屏幕上的选项(被捡走时引擎不一定补发End)。没有脚本不建盒。Overlap回调只读名字+选项快照(`assetNameOnly`/`cachedOptions`)，不解引用`asset`；只有玩家当前控制的`AForeverCharacter`(人物)进盒才弹选项，开车经过不算。

## 依赖关系
- 依赖：`Asset`/`AssetMod`(`Source/Dependence/player/asset_mod.h`)、`AForeverPlayerController::GetMeetOptionWidget`、`AForeverFrameworkActor`。
- 被谁依赖：`UForeverAssetFrameworkComponent`、`UForeverThrowableComponent`(`LoadAssetMesh`/`Launch`)。

## 待办/后续阶段
- 无。
