# TrafficLightElement.h / .cpp

## 职责

`ATrafficLightElement`：一个路口的全部红绿灯模型，由`UForeverTrafficLightFrameworkComponent`按距离生成/销毁、每帧推信号。

## 关键设计

- 4个ISM：框架`SM_TrafficLight_Frame`和三个灯罩`SM_TrafficLight_Red/Yellow/Green`(都在`/Game/Asset/TrafficLight`，由`Tools/TrafficLight`从`/Game/3rdParty/Retro_traffic_light`拆出来，路径写在构造函数里)。每个灯在4个ISM里各有一个实例，变换相同(世界坐标)。
- 灯罩材质`MI_TrafficLamp_*`(父材质`M_TrafficLamp`)的发光=灯色×lerp(0, OnIntensity=100, 逐实例数据0)，lerp不截断：亮写`kLampLitValue`(100，发光强度约10000，工程曝光重，白天要这个量级才看得出)，灭写0。`Build`建完全灭；`RefreshSignals`按`JunctionTraffic::GetSignal(junctionId, 这个灯的路, now)`算每个灯的信号，变了才改那个灯三个灯罩的数据(绿=绿灯亮，黄=黄灯亮，红=红灯亮，`None`=全灭)。左右两个灯显示同一条路的信号。
- 碰撞：框架`BlockAll`(挡相机弹簧臂和玩家)；灯罩不投影、无碰撞。自己不Tick。

## 依赖关系

- 依赖：Core `traffic/junction.h`(`JunctionTraffic::Signal`/`GetSignal`)。
- 被谁依赖：`UForeverTrafficLightFrameworkComponent`。
