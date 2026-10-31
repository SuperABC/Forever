#pragma once

#include "CoreMinimal.h"
#include "Element/VehicleElement.h"

#include "PlaneElement.generated.h"

// 可驾驶飞机：停在机场停机位里、玩家可以上去开的那架(路线上的飞机是运动学的
// ATransitVehicleElement，和这个类无关)。继承AVehicleElement是为了复用它的Chaos轮子
// (鼻轮转向/主轮手刹/滚动)、骨骼网格+物理、近身"驾驶"选项、上下机和位置存档、
// LoadClass<AVehicleElement>(blueprintPath)那条生成/流式链路。
//
// 飞行力学是手写的(轮子之外的全部)，每帧对carMesh施加"加速度"(AddForce/AddTorque的bAccelChange=
// true，忽略质量，系数和飞机质量无关)：推力、阻力、升力、速度朝机头对齐、机头朝速度风标、
// 俯仰/滚转控制力矩、角速度阻尼。力和力矩都按机体局部轴向合成，不用欧拉角，所以任意姿态
// (360度筋斗、倒飞)都成立。详见PlaneElement.md。
//
// 操控：鼠标滚轮=动力(-50~100，负=反推减速/倒车)，W=机头向下，S=机头抬升，A/D同时给轮子转向和空中滚转
// (在地面轮子起作用、不能滚转；在空中滚转起作用、轮子悬空无效)，鼠标=环视，Q下机，Space手刹。
UCLASS(Blueprintable)
class FOREVER_API APlaneElement : public AVehicleElement
{
	GENERATED_BODY()

public:
	APlaneElement();

	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

	float GetPower() const { return power; }

protected:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// ---- 飞行系数(蓝图Details里调，加速度单位是cm/s²，速度cm/s，角加速度rad/s²) ----

	// 满动力(100)时机头方向的推力加速度(重力约980)：推重比小于1，所以垂直向上爬会越爬越慢，撑不住就失速。
	UPROPERTY(EditDefaultsOnly, Category = "Plane|Flight")
	float maxThrustAccel = 800.f;

	// 满动力时的极速(cm/s)，阻力系数由它和maxThrustAccel反推：阻力=k*|v|*v，k=maxThrustAccel/topSpeed²。
	UPROPERTY(EditDefaultsOnly, Category = "Plane|Flight")
	float topSpeed = 8300.f;

	// 基础升力刚好等于重力的速度(cm/s)：升力沿机体"上"方向、大小正比于机头方向速度的平方，但基础升力最多等于重力。
	UPROPERTY(EditDefaultsOnly, Category = "Plane|Flight")
	float stallSpeed = 4500.f;

	// 迎角升力增益：机头高于气流(迎角>0)时额外的升力，单位是"重力加速度的倍数/弧度(满stallSpeed时)"。
	// 平飞配平靠基础升力(最多等于重力)，拉杆产生迎角后才有多余升力把航迹拉起来，所以松手不会越飞越抬头。
	UPROPERTY(EditDefaultsOnly, Category = "Plane|Flight")
	float aoaLiftGain = 4.f;

	// 失速低头角加速度(rad/s²)：空中前向速度低于stallSpeed时，机头被重力往下压，越慢越强，
	// 垂直上爬速度耗尽后会自己翻下来。
	UPROPERTY(EditDefaultsOnly, Category = "Plane|Flight")
	float stallPitchAccel = 1.5f;

	// 升力加速度上限(cm/s²)，防止高速时升力无限增长。
	UPROPERTY(EditDefaultsOnly, Category = "Plane|Flight")
	float maxLiftAccel = 3000.f;

	// 速度方向向机头对齐的速率(1/s)，空中才起作用。越大飞机越"贴着机头飞"，越像街机；越小越飘。
	UPROPERTY(EditDefaultsOnly, Category = "Plane|Flight")
	float alignRate = 0.8f;

	// 机头朝速度方向转的风标角加速度(rad/s²)，空中才起作用，让倾斜转弯时机头跟着转向。
	UPROPERTY(EditDefaultsOnly, Category = "Plane|Flight")
	float weathervaneAccel = 1.0f;

	// 重心偏移(cm，机体局部坐标，+X=机头方向)：叠加在物理资产自动算出的重心上。重心太靠后会把
	// 鼻轮翘起来，往前移(正X)就能压住鼻轮。BeginPlay时应用到根骨刚体，蓝图里直接调。
	UPROPERTY(EditDefaultsOnly, Category = "Plane|Mass")
	FVector centerOfMassOffset = FVector(300.f, 0.f, 0.f);

	// ---- 操控 ----

	// 俯仰/滚转最大角加速度(rad/s²)。
	UPROPERTY(EditDefaultsOnly, Category = "Plane|Control")
	float pitchAccel = 2.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Plane|Control")
	float rollAccel = 3.0f;

	// 机体三个轴的角速度阻尼(1/s)——稳定后的最大角速度≈角加速度/阻尼，松手后也靠它停下旋转。
	UPROPERTY(EditDefaultsOnly, Category = "Plane|Control")
	float pitchDamping = 1.5f;
	UPROPERTY(EditDefaultsOnly, Category = "Plane|Control")
	float rollDamping = 2.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Plane|Control")
	float yawDamping = 1.5f;

	// 舵面权限随空速线性增长，到这个速度(cm/s)满权限；低于它按比例减弱(低速几乎打不动舵)。
	UPROPERTY(EditDefaultsOnly, Category = "Plane|Control")
	float controlSpeed = 2500.f;

	// 动力下限(%)：负值=反推，用来减速和倒车。
	UPROPERTY(EditDefaultsOnly, Category = "Plane|Control")
	float minPower = -50.f;

	// 视角无操作多少秒后开始回正，以及回正速度(1/s，越小越慢)。
	UPROPERTY(EditDefaultsOnly, Category = "Plane|Control")
	float recenterDelay = 10.f;
	UPROPERTY(EditDefaultsOnly, Category = "Plane|Control")
	float recenterSpeed = 0.3f;

	// 滚轮每格增减的动力。
	UPROPERTY(EditDefaultsOnly, Category = "Plane|Control")
	float powerStep = 5.f;

private:
	void MovePlane(const FInputActionValue& value);
	void StopMovePlane(const FInputActionValue& value);
	void LookPlane(const FInputActionValue& value);
	void ThrottleUp();
	void ThrottleDown();
	void PlaneExit();
	void PlaneHandbrake(const FInputActionValue& value);
	void PlaneStopHandbrake(const FInputActionValue& value);
	void StartFreeLook() { bFreeLook = true; }
	void StopFreeLook() { bFreeLook = false; }

	// 有没有轮子着地。没配好轮子(蓝图还没设)时当作在空中。
	bool IsAirborne() const;

	// 左上角HUD：动力/速度/高度，和游戏时钟同一个AddOnScreenDebugMessage做法。
	void UpdateHud(float speedCmPerSec);

	// 当前动力(minPower~100)，滚轮改，下机/无人时清零。
	float power = 0.f;

	// 输入：pitchCmd>0=机头抬升(S)，rollCmd>0=向右滚转(D)。
	float pitchCmd = 0.f;
	float rollCmd = 0.f;

	// 鼠标环视相对追尾视角的偏移(度)，一段时间不动会缓慢回正。
	float lookYaw = 0.f;
	float lookPitch = 0.f;
	bool bLookedThisFrame = false;
	// 距离上次环视操作(鼠标/按住C)过了多少秒。
	float lookIdleTime = 0.f;

	// 按住C=自由环视：视角不自动回正。
	bool bFreeLook = false;

	// 物理没模拟的警告只打一次。
	bool bWarnedNotSimulating = false;
};
