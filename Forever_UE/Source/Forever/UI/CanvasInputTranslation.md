# CanvasInputTranslation.h / CanvasInputTranslation.cpp

## 职责

`FKey`到Core `Canvas`键码/鼠标按钮常量的**翻译表**：`TranslateKey`、`TranslateMouseButton`。

## 关键设计

- `PuzzleWidget`和`PhoneWidget`都要把UE键鼠事件转发进`Dependence/player/canvas.h`的`Canvas`，需要完全一样的映射，所以抽出来共享，避免复制两份。
- 键码精确对应`KEY_LEFT/KEY_UP`等常量和字母/数字的ASCII直通，不是老工程那种任意哈希值；查不到的键返回0(`Canvas::PushKey`会忽略0)，鼠标按钮查不到返回-1。

## 依赖关系

- 依赖：`Dependence/player/canvas.h`。
- 被谁依赖：`PhoneWidget.cpp`、`PuzzleWidget.cpp`。

## 待办/后续阶段

- 无。
