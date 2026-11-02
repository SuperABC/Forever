# ForeverExplosionShake.h / ForeverExplosionShake.cpp

## 职责

爆炸时的**镜头震动**：`UForeverExplosionShakePattern`(波形)+`UForeverExplosionShake`(`UCameraShakeBase`)。

## 关键设计

- UE5.7里`ULegacyCameraShake`已移除，Perlin/Wave模式在`EngineCameras`插件里(项目没启用)，所以自己继承`UCameraShakePattern`实现波形。
- **波形**：几条不同频率的正弦叠加(`phaseA`/`phaseB`是每次启动随机的相位)，振幅按`(1-t)^2`衰减；`duration`(0.45秒)、`frequency`(22Hz)；滚转/俯仰/偏航最大角度`rollAmplitude`/`pitchAmplitude`/`yawAmplitude`(度)，位置抖动最大偏移`locationAmplitude`(cm)，都是`UPROPERTY(EditAnywhere)`。
- **播放**：`UGameplayStatics::PlayWorldCameraShake(世界, 本类, 爆炸中心, 内半径, 外半径)`，距离衰减由引擎处理。

## 依赖关系

- 依赖：`UCameraShakePattern`/`UCameraShakeBase`。
- 被谁依赖：`Player/ForeverExplosionEffect.cpp`(爆炸特效播放时调`PlayWorldCameraShake`)。

## 待办/后续阶段

- 无。
