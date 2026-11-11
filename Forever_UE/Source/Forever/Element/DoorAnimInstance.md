# DoorAnimInstance.h

## 职责
`UDoorAnimInstance : UAnimInstance`：门动画蓝图的父类，只暴露两个只读变量，其余完全交给动画蓝图：
- `OpenProgress`：开合度，0=全关，1=全开，按`DoorSpec::openSeconds`线性变化（缓动自己在动画蓝图里加）。
- `OpenSide`：+1/-1，这次往组件本地+Y（墙法线）一侧还是另一侧开，永远是远离触发者的一侧；平开门的动画蓝图可用它决定转向，推拉门忽略。

变量由`UForeverDoorComponent::TickComponent`每帧直接写（推送，不是动画实例自己拉取），见`DoorComponent.md`。没有`.cpp`——没有任何逻辑。

## 做一个新门（UE里）
1. 启用插件 **Skeletal Mesh Editing Tools**，对门的静态网格右键 **Convert to Skeletal Mesh**（Create New 骨架，网格原点会有一根 root 骨）。
2. 用 Bone Creation 加一根骨（比如`door`），放在门扇的运动原点（平开=铰链处，推拉=门扇中心）；用 Skin Weight 工具把门扇权重 100% 刷给它，门框留在 root。
3. 新建动画蓝图：父类选 **DoorAnimInstance**，骨架选刚创建的。AnimGraph 里对门扇骨用 Transform (Modify) Bone（旋转/平移量由`OpenProgress`驱动，需要的话乘`OpenSide`）→ Output Pose；也可以用 Sequence Evaluator 把`OpenProgress`映射到动画时间。
4. 把骨骼网格和动画蓝图类（`…_C`）的路径填进 Basic 的 `door_assets.h`（当前所有门共用一对），重启编辑器。

## 注意
- 骨骼网格的包围盒必须是整扇门（含门框）。Forever 层按包围盒把它对齐门洞：X、Y 等比缩放到门宽，Z 独立缩放到门高。
- 动画蓝图的父类不是`DoorAnimInstance`时，门不会动，日志告警一次。
