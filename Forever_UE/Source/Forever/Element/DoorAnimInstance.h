#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"

#include "DoorAnimInstance.generated.h"

// 门的动画实例基类：只暴露"开合度"这一个轴，模型怎么动完全由继承它的动画蓝图决定(AnimGraph里读OpenProgress，
// 用Transform (Modify) Bone或Sequence Evaluator驱动门扇骨)。变量由UForeverDoorComponent每帧写入，见DoorAnimInstance.md。
UCLASS()
class FOREVER_API UDoorAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// 开合度：0=全关，1=全开，按DoorSpec::openSeconds线性变化(缓动在动画蓝图里自己加)。
	UPROPERTY(BlueprintReadOnly, Category = "Door")
	float OpenProgress = 0.f;

	// 这扇门这次往哪一侧开：+1=往组件本地+Y(墙法线)一侧，-1=往另一侧——永远是远离触发者的一侧。
	// 平开门的动画蓝图可以用它决定转动方向，推拉门忽略即可。
	UPROPERTY(BlueprintReadOnly, Category = "Door")
	float OpenSide = 1.f;
};
