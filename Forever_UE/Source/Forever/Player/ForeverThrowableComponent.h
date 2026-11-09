#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include <string>
#include <unordered_map>
#include <vector>

#include "ForeverThrowableComponent.generated.h"

class AForeverCharacter;
class Player;

// 长按3列表里的一项：一组可投掷物(同一个type合并)。
USTRUCT()
struct FThrowListItem
{
	GENERATED_BODY()

	UPROPERTY()
	FString type; // 资产id

	UPROPERTY()
	int32 count = 0;
};
class UStaticMeshComponent;
class UProceduralMeshComponent;

// 投掷物(手雷/烟雾弹)：切换、瞄准预览、投掷。与UForeverWeaponComponent互斥，见ForeverThrowableComponent.md。
UCLASS()
class FOREVER_API UForeverThrowableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UForeverThrowableComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 短按3：没有激活投掷物时激活缓存里最靠前的一个，已经激活则切到下一个(末尾循环)。
	void CycleThrowable();

	// 当前身上所有投掷物(先Refresh，按稳定顺序)，供长按3的选择列表使用。
	TArray<FThrowListItem> GetThrowableList();
	FString GetActiveType() const;
	// 按type激活(先Refresh，type不存在返回false)——选择列表点击按钮时调用。
	bool ActivateByType(const FString& type);

	// 退出投掷状态(不处理瞄准标志，由AForeverCharacter负责)——切回肩上武器时调用。
	void Deactivate();

	bool IsActive() const { return !activeType.empty(); }

	// 重新扫描身上的投掷物并刷新缓存(背包没有变化事件，调用时机见md)。激活的用完了会自动切到下一个，全没了则退出投掷状态。
	void Refresh();

	// 能不能瞄准/投掷：激活的投掷物本身就拿在手里(拿着它的手就是投掷手)，或者至少一只手是空的。
	bool CanThrow() const;

	// 左键：扔出一个单位。要求已激活且在瞄准中。
	void Throw();

	// 投掷参数(预览和真实投掷共用，真实轨迹由物理决定，可能和预览有偏差)。
	// 起点相对角色的本地偏移(cm，+X前、+Y右、+Z上)。
	UPROPERTY(EditAnywhere, Category = "Throw")
	FVector throwOffset = FVector(60.f, 20.f, 50.f);

	// 初速度大小(cm/s)。
	UPROPERTY(EditAnywhere, Category = "Throw")
	float throwSpeed = 1500.f;

	// 在视线方向基础上额外抬高的仰角(度)，让平视时也能抛出弧线。
	UPROPERTY(EditAnywhere, Category = "Throw")
	float throwElevation = 8.f;

	// 预览曲线最长模拟时间(秒)。
	UPROPERTY(EditAnywhere, Category = "Throw")
	float previewDuration = 3.f;

private:
	AForeverCharacter* GetCharacter() const;
	Player* GetPlayer() const;

	void ActivateType(const std::string& type); // 不叫Activate：会隐藏UActorComponent::Activate(bool)
	void ShowActiveMessage() const;
	void ShowNoThrowableMessage() const;
	void UpdatePreview();
	void UpdatePathMesh(const TArray<FVector>& points, bool bHit);
	void HidePreview();
	void ComputeLaunch(FVector& outStart, FVector& outVelocity) const;
	int32 IndexOfType(const std::string& type) const;

	// 当前激活的投掷物type(资产id)，空=没激活。
	std::string activeType;

	// 缓存：rankByType给每个type永久序号，entries按序号排序，保持顺序稳定(见md)。
	std::unordered_map<std::string, int> rankByType;
	int nextRank = 0;
	struct FThrowableEntry {
		std::string type;
		int count = 0;
		std::string meshPath;
		float meshScale = 1.f;
	};
	std::vector<FThrowableEntry> entries;

	// 瞄准时显示在投掷起点的模型(用资产自己的meshPath/meshScale)。
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> previewMesh;
	std::string previewMeshPath; // 当前previewMesh已经设成了哪个路径，避免每帧重复加载

	// 预览曲线：朝向摄像机的平面带(程序化网格)，每帧重建。
	UPROPERTY()
	TObjectPtr<UProceduralMeshComponent> pathMesh;
};
