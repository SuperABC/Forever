#pragma once

#include "CoreMinimal.h"
#include "Element/VehicleElement.h"

#include "BoatElement.generated.h"

class Map;
class AForeverFrameworkActor;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

// 可驾驶的船：停在港口船位里、玩家可以上去开。继承AVehicleElement复用骨骼网格/选项/上下船/位置写回Core的链路，
// 没有轮子(Chaos轮式载具组件留着但WheelSetups清空)，浮力/推进/转向/阻力每帧手写，浪花是C++画的程序化粒子，详见BoatElement.md。
// 操控：W前进、S后退、A/D转向(停着转不了，越快转得越快)，鼠标环视，Q下船。
// 约定：骨骼网格船头朝+X，原点在船体中心附近。
UCLASS(Blueprintable)
class FOREVER_API ABoatElement : public AVehicleElement
{
	GENERATED_BODY()

public:
	ABoatElement();

	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

protected:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// ---- 浮力(蓝图Details里调，长度cm，加速度cm/s²) ----

	// 浮点(船体局部坐标，cm)：每个点各自按"水面减点高度"算浮力，多个点一起产生俯仰/横滚的恢复力矩。
	// 空=按骨骼网格包围盒在船底自动生成6个点(船头/船尾/左右各2)；船体不是长方体时自己填(点放在船底边缘)。
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Float")
	TArray<FVector> floatPoints;

	// 平衡时浮点沉在水面下的深度(cm)，也就是吃水。浮点在船底时就是船底入水深度。
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Float")
	float draft = 200.f;

	// 排水量(kg)：浮点都沉入draft时的总浮力=这么多kg的重量(浮力只跟排开的水的体积有关，和船自己多重无关)。
	// 船的质量=排水量时平衡吃水刚好是draft；船比它轻就浮得更高、重就沉得更深(平衡吃水≈draft*质量/排水量)。
	// 0=取船当前质量(旧行为：不管船多重吃水都是draft)。想"改轻船会浮高"：BeginPlay日志会打出当前质量(Log: Boat mass)，把原质量填这里，再去减质量。
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Float")
	float displacementMassKg = 0.f;

	// 浮点最多按"沉入几倍draft"算浮力，防止被浪/落水瞬间弹飞。
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Float")
	float maxSubmergeRatio = 1.5f;

	// 垂向阻尼(1/s)：浮力弹簧的临界阻尼≈2*sqrt(g/draft)(draft 200时约4.4)，小于它船会上下晃、落水后多弹几下(快艇一跳一跳要靠它)，
	// 大于它船像泡在油里。
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Float")
	float floatDamping = 1.5f;

	// 水中俯仰/横滚角速度阻尼(1/s)，让船不要一直晃。
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Float")
	float rollPitchDamping = 1.5f;

	// 防翻船：把船体"上"向世界上方向回正的角加速度(rad/s²/每弧度倾斜，约等于倾斜1弧度时的回正加速度)。
	// 浮点出水(船头抬起/跳起来)后浮力给不出恢复力矩，靠它把船拉平；船还是翻就调大。
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Float")
	float uprightAccel = 4.f;

	// ---- 推进 ----

	// 满油门时的推力加速度(cm/s²，质量无关)。
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Drive")
	float maxThrustAccel = 700.f;

	// 倒车推力相对前进推力的比例。
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Drive")
	float reverseThrustScale = 0.4f;

	// 推力的向上分量/水平分量(tanθ)：>0时前进推力会带一部分向世界上方向托起船体(作用在重心，不产生俯仰力矩)，
	// 托起后浮点出水、推力随螺旋桨出水而变小，船落回去再被托起——形成快艇一跳一跳。0=不跳。
	// 数值是相对水平推力的比例：0.35约等于0.35*推力加速度的抬升，大了船会整个被托出水。
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Drive")
	float thrustUpRatio = 0.f;

	// 船体倾斜超过这个角度(度)时向上分量淡出为0，倾斜越大越弱，防止船头抬起后还继续往上托。
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Drive")
	float maxLiftTilt = 25.f;

	// 水平推力的作用点高度：0=和重心同高(推力完全不产生俯仰)，1=真的在船底(推力点)，船尾推力会把船头往上抬。
	// 大船重心离船底高，1会让船头被抬得很厉害；取小一点只保留一点抬头感。
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Drive")
	float thrustLeverRatio = 0.6f;

	// 推力作用点(船体局部坐标，cm，螺旋桨位置)；不勾"自定义"就用包围盒自动算：船尾、船底。
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Drive")
	bool bCustomThrustPoint = false;
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Drive", meta = (EditCondition = "bCustomThrustPoint"))
	FVector thrustPoint = FVector::ZeroVector;

	// 阻力：纵向/横向各一个一次项(1/s)和一个二次项(1/cm)，加速度=-(一次*v+二次*|v|*v)，都按船体入水程度缩放。
	// 横向阻力远大于纵向：船头和速度方向夹角大了，横向阻力就把船速吃掉(甩尾会让船慢下来)。
	// 极速≈推力和纵向阻力平衡处。
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Drive")
	float forwardDrag = 0.1f;
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Drive")
	float forwardDragQuad = 0.00025f;
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Drive")
	float lateralDrag = 1.5f;
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Drive")
	float lateralDragQuad = 0.003f;

	// ---- 转向 ----

	// 沿船头方向的速度(cm/s)达到这个值时转向满效；低于它按比例减弱，停着(0)完全转不了，倒车方向反过来。
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Steer")
	float steerFullSpeed = 1000.f;

	// 满效时的目标转向角速度(度/秒)。
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Steer")
	float maxYawRate = 40.f;

	// 转向角速度向目标靠拢的速率(1/s)，越大船越"灵"。
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Steer")
	float yawResponse = 2.f;

	// ---- 浪花(程序化粒子，没有Niagara/新资产依赖) ----

	// 浪花起点相对推力点的偏移(船体局部坐标，cm)。
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Wake")
	FVector wakeOffset = FVector(-150.f, 0.f, 0.f);

	// 速度(cm/s)超过它才出浪花；到wakeFullSpeed时发射率满。
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Wake")
	float wakeMinSpeed = 150.f;
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Wake")
	float wakeFullSpeed = 1200.f;

	// 满速时每秒发射的粒子数(粒子池一共32个，rate*life不要超过它)。
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Wake")
	float wakeRate = 30.f;

	// 粒子寿命(秒)和最大直径(cm)。
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Wake")
	float wakeLife = 1.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Wake")
	float wakeSize = 120.f;

	// 粒子颜色和峰值不透明度(用ExplosionSmoke材质的Color/Opacity参数)。
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Wake")
	FLinearColor wakeColor = FLinearColor(0.9f, 0.95f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "Boat|Wake")
	float wakeOpacity = 0.5f;

private:
	void MoveBoat(const FInputActionValue& value);
	void StopMoveBoat(const FInputActionValue& value);
	void LookBoat(const FInputActionValue& value);
	void BoatExit();

	// 取Core地图(缓存框架Actor)；没有返回nullptr(此时没有浮力)。
	Map* GetCoreMap();

	// 按包围盒生成自动浮点/推力点(BeginPlay里floatPoints为空、没有自定义推力点时)。
	void BuildAutoPoints();

	// 一个浮点(世界坐标)的当前入水深度：水面-点高度，没水格子返回false。surfaceZ输出水面高度。
	bool SamplePoint(const FVector& world, double time, float& outDepth, float& outSurfaceZ);

	void UpdateWake(float deltaTime, const FTransform& transform, const FVector& velocity, double time, float propellerWet);

	void UpdateHud(float speedCmPerSec);

	// 输入：throttleCmd>0=W前进，<0=S后退；steerCmd>0=D右转。
	float throttleCmd = 0.f;
	float steerCmd = 0.f;

	// 实际使用的浮点/推力点(局部坐标)：蓝图填了就用蓝图的，否则是BuildAutoPoints的结果。
	TArray<FVector> activeFloatPoints;
	FVector activeThrustPoint = FVector::ZeroVector;

	TWeakObjectPtr<AForeverFrameworkActor> cachedFramework;

	bool bWarnedNotSimulating = false;


	// ---- 浪花粒子池 ----
	static constexpr int32 kWakeParticleCount = 32;

	struct FWakeParticle
	{
		bool bActive = false;
		FVector position = FVector::ZeroVector;
		FVector velocity = FVector::ZeroVector;
		float age = 0.f;
		float life = 1.f;
		float size = 100.f;
	};

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> wakeMeshes;
	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> wakeMaterials;
	TArray<FWakeParticle> wakeParticles;
	float wakeEmitAccumulator = 0.f;
};
