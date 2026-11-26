# FurnitureCollision.h / FurnitureCollision.cpp

## 职责

家具的阻挡碰撞。家具网格(`Tools/Furniture/make_furniture.py`生成)本身不带碰撞体，阻挡用若干个盒子拼出来。
房间家具(`ABuildingElement::BuildRoomFurniture`的碰撞ISM)和固定物品的场景实体(`AAssetElement::SetupFixture`)共用这一份。

## 关键设计

- **`GetBoxes(meshKey, boundsMin, boundsMax, outBoxes)`**：按网格名(只看最后一个`/`之后的前缀)把网格局部包围盒拆成几块，
  人能走到床边、坐垫前，相机也不会穿进床头板和靠背。尺寸直接取自`make_furniture.py`里拼这件家具的长方体(米)，
  再用这一类家具固定不变的那个设计尺寸反推"每设计米合多少局部单位"，不依赖网格的导入单位。

| 家具(网格名前缀) | 盒子 |
|---|---|
| 床`Bed_` | 床头板(背面厚0.07、到顶)；床身(到0.60) |
| 沙发`Sofa`、扶手椅`Armchair` | 靠背(背面厚0.20、到顶)；坐垫(两扶手之间、到0.50)；左右扶手(各宽0.15、到0.62) |
| 椅子`Chair`、餐椅`DiningChair` | 靠背(背面厚0.04、到顶)；座面和椅腿(到0.47) |
| 电视柜`TvStand` | 柜体(到0.45)；电视屏(柜顶中间的薄板，到顶) |
| 马桶`Toilet` | 水箱(背面厚0.18、到顶)；座(到0.44) |
| 淋浴`Shower` | 只有背面中间的立管和花洒臂；底盘不挡，人能走进去 |
| 其余(衣柜、桌子、茶几、床头柜、冰箱、橱柜、灶台、水槽、吊柜、洗脸盆、浴缸、洗衣机、书架、绿植、落地灯、鞋柜、斗柜) | 一个等于包围盒的盒子 |

- **`kMinBlockingHeight`(12cm)**：整件家具(乘完缩放)矮于它就不挡路(地毯)，由调用方按整件判断。
- **`ApplyBlockingResponse(component)`**：`ECC_WorldStatic`，只Block `ECC_Pawn`、`ECC_PhysicsBody`、`ECC_Visibility`、`ECC_Camera`，其余忽略。
  Visibility是为了脚部IK，Camera是为了弹簧臂，原因见`BuildingElement.md`"房间家具"。
- 新增一种需要分块的家具：在`GetBoxes`里加一个前缀分支，照着`make_furniture.py`里的长方体填尺寸。

## 依赖关系

- 依赖：引擎的`UPrimitiveComponent`。不依赖Core。
- 被谁依赖：`ABuildingElement`(房间家具)、`AAssetElement`(固定物品)。
