# AssetElement.h / AssetElement.cpp

## 职责
`AAssetElement`：物品/资产在世界里的可视化表示。由`UForeverAssetFrameworkComponent::SpawnWorldAsset()`在Drop时`SpawnActor(Deferred)`+`Init(asset)`，`DestroyWorldAsset()`(Pick时)销毁；**不持有`Asset`所有权**(`asset`是非owning裸指针，真正所有权在`Room::assets`或某个容器的`contents`里，`EndPlay`置空)。

## 关键设计
- 网格：默认`/Game/Asset/Common/Mesh/Cube.Cube`占位(与`ABuildingElement`同一份)；`AssetMod::meshPath`非空时用`LoadAssetMesh`加载(加载失败回退引擎球体并打Warning)，投掷预览模型也用这个函数以保证和掉落物同一份网格。缩放不在`Init`里设，由生成方经`SpawnActorDeferred`的Transform传入(物理体在BeginPlay时就按这个缩放创建)。
- 物理：Movable + `PhysicsActor`碰撞预设 + 模拟物理 + 重力，玩家角色可以直接推动；`SetAngularDamping(3)`防止球体(投掷物)落地后无限滚动，不加线性阻尼以免飞行弧线偏离预览曲线。
- `Launch(velocity, thrower)`：投掷初速度；`thrower`的碰撞被忽略(起点在投掷者胶囊体附近，否则会被顶开)；开CCD防止小而快的球体穿过薄墙/地面。

- **固定物品**(`Asset::IsFixture()`，没有`AssetMod`)：显示和阻挡跟房间家具一起由`ABuildingElement::BuildRoomFurniture`建进ISM(阻挡同样用`FurnitureCollision::GetBoxes`的分块)，这个Actor只剩弹选项的触发盒：`Init`里走`SetupFixture`——关物理和重力、网格隐藏且`NoCollision`(只用来给触发盒量包围盒)。只在玩家进了它所在的房间、而且它有脚本时才生成(`UForeverAssetFrameworkComponent::OnPlayerRoomChanged`)。三轴缩放由生成方写进Transform(`Asset::GetMeshScale3`)。
- **流式生成**：可移动物品的实体由`UForeverAssetFrameworkComponent`按玩家距离生成/销毁，不只在丢弃时。`BeginPlay`里主动`UpdateOverlaps`一次，玩家已经站在触发盒里时也能立刻弹选项。

## 弹选项的触发盒
资产有脚本(`asset->GetScript()`非空)才在`Init`建触发盒(普通物品是世界120x120x80cm，随根缩放反算；固定物品个头大，取网格包围盒、世界里水平各外扩80cm、上下各外扩20cm；与`AVehicleElement::BuildProximityBox`同一思路)，玩家的`AForeverCharacter`进入时把`asset->GetOptions()`快照推给MeetOption；`RefreshOptions()`在`add_option/remove_option`后刷新；`SetOptionsAllowed(false)`时进了盒也不弹(物品组件按"玩家是不是在这件物品所在的房间"设置，室外物品总是允许)，切换时玩家正在盒里就立刻显示/撤掉；`EndPlay`撤掉屏幕上的选项(被捡走时引擎不一定补发End)。没有脚本不建盒。Overlap回调只读名字+选项快照(`assetNameOnly`/`cachedOptions`)，不解引用`asset`；只有玩家当前控制的`AForeverCharacter`(人物)进盒才弹选项，开车经过不算。

## 依赖关系
- 依赖：`Asset`/`AssetMod`(`Source/Dependence/player/asset_mod.h`)、`AForeverPlayerController::GetMeetOptionWidget`、`AForeverFrameworkActor`。
- 被谁依赖：`UForeverAssetFrameworkComponent`、`UForeverThrowableComponent`(`LoadAssetMesh`/`Launch`)。
