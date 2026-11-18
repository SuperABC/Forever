#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "LampElement.generated.h"

class ULightComponent;

// 真实光源的类型。
UENUM(BlueprintType)
enum class ELampLightType : uint8
{
	Point,
	Spot,
};

// 一个真实光源的参数，在蓝图Class Defaults里填(ALampElement::lights数组，一盏灯可以有多个)。
// 光源只在"近处且亮灯"时由C++现场创建，其余时间不存在，见ALampElement::RefreshLampEffects。
USTRUCT(BlueprintType)
struct FLampLightSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ELampLightType type = ELampLightType::Spot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor color = FLinearColor(1.f, 0.85f, 0.6f);

	// 光强，单位是坎德拉(cd)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float intensity = 2000.f;

	// 衰减半径(cm)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float attenuationRadius = 1500.f;

	// 聚光灯的内/外锥角(度)，点光源忽略。
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float innerConeAngle = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float outerConeAngle = 45.f;

	// 相对Actor根的位置/旋转。聚光灯朝+X，默认旋转pitch=-90即朝下；点光源忽略旋转。
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector relativeLocation = FVector(0.0, 0.0, 600.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator relativeRotation = FRotator(-90.0, 0.0, 0.0);

	// 是否投影。路灯数量多，默认关。
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool castShadows = false;
};

// 灯杆网格改走全城共享ISM时，每种灯(灯蓝图类)自己的参数，在蓝图Class Defaults里填(ALampElement::meshSettings)。
// UForeverLampFrameworkComponent按灯蓝图类读它来建ISM，同一种灯的所有实例共用。
USTRUCT(BlueprintType)
struct FLampMeshSettings
{
	GENERATED_BODY()

	// 灯头发光的材质槽下标(对应蓝图里原来Create Dynamic Material Instance的Element Index)；<0=这种灯没有会发光的槽。
	// 该槽会换成这种灯所有实例共享的一个动态材质，其它槽直接用网格组件原来的材质。
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 emissiveMaterialSlot = 0;

	// 昼夜要改的发光强度标量参数名(要和灯头材质里的参数名一致)。
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName emissiveParameterName = TEXT("EmissiveStrength");

	// 亮灯(亮度1)和灭灯(亮度0)时写进上面参数的值，亮度在两者之间线性插值。原蓝图里是亮度×100，即on=100、off=0。
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float emissiveStrengthOn = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float emissiveStrengthOff = 0.f;

	// 灯杆实例的最大绘制距离(cm)，逐实例剔除，只对非Nanite网格生效；<=0不剔除。网格开了Nanite就留0。
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float cullDistance = 0.f;

	// 灯杆是否投射阴影。
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool castShadow = true;

	// 灯杆是否有碰撞(用ISM默认的碰撞设置；关掉=无碰撞，开局创建物理体的开销也省掉)。
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool hasCollision = true;
};

// 路灯的基类Actor：每盏灯一个实例，由UForeverLampFrameworkComponent生成并按昼夜、距离驱动。
// 近处用真实光源、远处用光斑Decal，详见LampElement.md。
UCLASS()
class FOREVER_API ALampElement : public AActor
{
	GENERATED_BODY()

public:
	ALampElement();

	// 调节亮度，brightness目前只取0(关)或1(开)。记下亮度，触发蓝图事件SetBrightness，再刷新光源/Decal。
	UFUNCTION(BlueprintCallable, Category = "Lamp")
	void ApplyBrightness(float newBrightness);

	UFUNCTION(BlueprintPure, Category = "Lamp")
	float GetBrightness() const { return brightness; }

	// 玩家走近：标记为近处并刷新(亮灯时创建真实光源、隐藏Decal)。蓝图可重写做额外表现，重写时调父类。
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Lamp")
	void SwitchNear();

	// 玩家走远：标记为远处并刷新(移除真实光源、亮灯时显示Decal)。蓝图可重写做额外表现，重写时调父类。
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Lamp")
	void SwitchFar();

	UFUNCTION(BlueprintPure, Category = "Lamp")
	bool IsNear() const { return bNear; }

	// 灯杆网格的共享ISM参数，框架组件按这种灯的蓝图类读取。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lamp|Mesh")
	FLampMeshSettings meshSettings;

protected:
	virtual void BeginPlay() override;

	// 蓝图事件：亮度变化时调用。在蓝图里把brightness乘上一个倍率写进灯头材质的EmissiveStrength。
	UFUNCTION(BlueprintImplementableEvent, Category = "Lamp")
	void SetBrightness(float newBrightness);

	// 近处亮灯时要创建的真实光源，蓝图Class Defaults里填；空数组=这种灯没有真实光源。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lamp|Light")
	TArray<FLampLightSettings> lights;

private:
	// 按"真实光源 = 近 && 亮度>0；Decal可见 = 远 && 亮度>0"对齐当前状态。
	void RefreshLampEffects();
	void CreateRealLights();
	void DestroyRealLights();

	float brightness = 0.f;
	bool bNear = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ULightComponent>> activeLights;
};
