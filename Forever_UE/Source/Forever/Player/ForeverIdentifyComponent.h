#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ForeverIdentifyComponent.generated.h"

class UWidgetComponent;
class ACitizenElement;
class UForeverIdentifyTagWidget;
class Citizen;

// 识别模式：按住鼠标中键进入、松开退出。模式下，距离当前操控的Pawn一定范围内(默认20m)的所有人物
// (包括被操控的角色)头上出现世界空间的文字标签，近大远小、始终正对摄像机。同一次识别模式里
// 参与的人物和内容在进入那一刻定下来，不再更新，退出再进入才刷新。挂在AForeverPlayerController上
// (换人操控/上下载具时Controller不变)。见ForeverIdentifyComponent.md。
UCLASS()
class FOREVER_API UForeverIdentifyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UForeverIdentifyComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void Enter();
	void Exit();
	bool IsActive() const { return bActive; }

	// 识别范围(cm)，以进入那一刻操控的Pawn位置为中心，和人物流式生成同一个基准。
	UPROPERTY(EditAnywhere, Category = "Identify")
	float identifyRadius = 2000.f;

	// 标签底边在头顶上方多少cm。
	UPROPERTY(EditAnywhere, Category = "Identify")
	float headMargin = 5.f;

	// 头顶比胶囊体顶端低多少cm(胶囊半高96，Mannequin的头顶大约在它下面这么多)，用来从胶囊体顶端
	// 往下修正到真正的头顶；换了身高不同的角色可以调。
	UPROPERTY(EditAnywhere, Category = "Identify")
	float headBelowCapsuleTop = 15.f;

	// 标签控件的绘制尺寸(像素)，高度要留够最多行数；整体世界缩放用tagScale(1像素=1cm*缩放)。
	UPROPERTY(EditAnywhere, Category = "Identify")
	FVector2D drawSize = FVector2D(400.f, 300.f);

	UPROPERTY(EditAnywhere, Category = "Identify")
	float tagScale = 0.4f;

	// 分辨率倍数：标签的绘制尺寸和字号乘它、世界缩放除以它，看起来大小不变，但字更精细、描边(整数
	// 像素，最小1)相对字形更细。描边相对粗细 = 1 / (字号 * 倍数)。代价是每个标签的渲染贴图面积
	// 按倍数平方增长(2倍=800x600)，附近人很多时别设太大。
	UPROPERTY(EditAnywhere, Category = "Identify")
	float resolutionScale = 1.f;

	// 标签颜色乘数(可以大于1)：白天曝光很重，世界里的UI默认亮度不够，用它提亮。
	UPROPERTY(EditAnywhere, Category = "Identify")
	float tintBrightness = 10000.f;

	// 标签控件类，默认的纯C++控件即可；想用自己做的WBP就指定。
	UPROPERTY(EditAnywhere, Category = "Identify")
	TSubclassOf<UForeverIdentifyTagWidget> tagWidgetClass;

private:
	// 一个人要显示哪几行——唯一生成内容的地方，以后加年龄/职业等只改这里。
	TArray<FString> BuildLines(const Citizen& citizen) const;

	void AddTag(ACitizenElement* citizenActor);

	bool bActive = false;
	TArray<TWeakObjectPtr<UWidgetComponent>> tags;
};
