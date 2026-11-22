# MixamoRetarget

把 Mixamo 的人形动画重定向到项目里的 Mannequin(`SKM_Manny_Simple`)。纯编辑器手工流程，没有脚本。游泳动画(`Swimming`/`Treading_Water`)就是这么做的。

## 已有资产(可复用，以后重定向其他动画不用重做 2~4 步)

都在 `Content/Asset/Animation/X_Bot/`：

- `X_Bot`/`X_Bot_Skeleton`：从 Mixamo 下载的 X Bot(**With Skin**)，骨骼名是 `mixamorig:` 风格。
- `IK_XBot`：X Bot 的 IK Rig(源)。
- `IK_Mannequin`：Manny 的 IK Rig(目标，运行在 `SKM_Manny_Simple` 上)。
- `RTG_NewRetargeter`：源 `IK_XBot` → 目标 `IK_Mannequin`，**目标姿势已对齐成 T 字**。

重定向好的动画放在 `Content/Asset/Animation/Swim/`。

## 流程

### 1. 从 Mixamo 下载

- 网址要带 www：`https://www.mixamo.com`(裸域名 `mixamo.com` 没有 DNS 记录，浏览器报 `DNS_PROBE_FINISHED_NXDOMAIN`)。Adobe 账号免费。
- 搜动画(如 `swim`/`tread`/`float`)，循环的游泳类动作如有 **In Place** 勾上(位移交给移动组件)。
- Download：Format **FBX Binary**，Frames per Second **30**，Keyframe Reduction **none**。
  - **第一次**要选 **With Skin**(需要网格+骨架才能在 UE 里生成 Mixamo 骨架)。已有 `X_Bot_Skeleton` 之后的动画选 **Without Skin**。

### 2. 导入 FBX(Interchange 对话框)

- 带网格那次：「仅导入动画」不勾，「骨架」留空，「导入骨骼网格体」勾，「导入动画」勾。
- 之后的纯动画：「动画」标签下「仅导入动画」勾、「骨架」选 `X_Bot_Skeleton`、「导入动画」「导入骨骼轨道」勾，其余默认。「骨架」没选时「导入」按钮是灰的。
- 不要把 Mixamo 动画直接导到 Mannequin 的骨架上：骨骼名对不上，动画轨道是空的。

### 3. 建两个 IK Rig(已做，`IK_XBot`/`IK_Mannequin`)

内容浏览器右键 → 动画 → 重定向 → **IK绑定**。

- 工具栏点 **自动创建重定向链**(识别出 Spine/Arms/Legs/Head 等)。
- 在骨盆骨骼上右键 → **设置骨盆**(Mannequin 是 `pelvis` 不是 `root`；X Bot 是 Hips)。
- 保存。

### 4. 建 IK 重定向器并对齐姿势(已做，`RTG_NewRetargeter`)

右键 → 动画 → 重定向 → **IK重定向器**，源选 `IK_XBot`，目标选 `IK_Mannequin`。

- X Bot 默认 **T 字**，Manny 默认 **A 字**，必须把目标姿势对齐，否则手臂歪。
- 进入**编辑姿势**模式后「自动对齐」菜单才可用(灰色=没进编辑模式)：选 **对齐所有骨骼**，对齐方法保持**方向**。
- 日志里「骨骼链 Root 无法标准化，链过短」「缺失源根骨骼」可忽略(`root` 只有一根骨骼、Mixamo 没有 root)。
- 切到 **Running Retarget**，在资产浏览器里双击动画预览，确认手臂/腿/整体高度正常。

### 5. 生成 Manny 版动画

在内容浏览器里选中要转的动画(X Bot 骨架的)，右键 → 重定向 → **复制并重定向动画资产**，重定向器选 `RTG_NewRetargeter`；或者在重定向器的资产浏览器里选动画点 **导出选定动画**。

生成后打开确认：预览是 Manny、骨架是 Manny 的、「启用根运动」关、循环动画的「循环」开。

## 以后重定向新动画

只需要：下载 **Without Skin** FBX → 按第 2 步导入到 `X_Bot_Skeleton` → 第 5 步(重定向器已经配好，不用重做 3、4)。

## 备注

- 动画的位移由 `UCharacterMovementComponent` 控制，所以游泳/行走这类动画都应是原地的，不要在重定向器里加根运动。
- 如果换了别的角色骨架(不是 Manny)，要重做 3、4(目标 IK Rig + 新重定向器)。
