#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"

#include "DoorComponent.generated.h"

class Door;
class Citizen;
class UStaticMesh;
class UStaticMeshComponent;
class UBoxComponent;
class UPrimitiveComponent;
struct FHitResult;

// 自动门：门扇+(可选)门框+触发盒+开关动画，挂在"所属对象的渲染Actor"身上(建筑门/房间门
// 挂ABuildingElement，园区门挂UForeverZoneFrameworkComponent的owner)。所有数据来自构造
// 时绑定的Core端Door*(几何/外观/门禁)，这个组件本身不持有任何跨帧状态之外的游戏逻辑——
// 门禁判定本身是Door::CanPass()，这里只负责"谁进了触发盒→该不该开→动画插值"。
//
// 实现细节见DoorComponent.md，这里只记三条读代码前必须知道的事：
// ①每扇门扇是一个独立的USceneComponent"铰链/滑轨枢轴"+挂在它下面的UStaticMeshComponent
// 可视网格——动画只改枢轴的相对Transform(Swing转Yaw，Slide平移)，可视网格相对枢轴的
// offset在Init()时就定好、动画过程中不变，这样枢轴的旋转/平移中心和网格资产自身的pivot
// 约定(资产原点在哪)完全解耦，哪怕用没有调整过pivot的占位Cube也能正确摆位(自动按包围盒
// 算offset，见door.md"实际摆位")。
// ②`style`（Slide/Swing）和`leaves`（1/2）决定枢轴的初始布局，`Door::IsFlippedSide()`只在
// `leaves==1`时影响"滑动方向/铰链朝哪边"，双扇恒不读这个值。
// ③开门的触发判定是"触发盒里有没有CanPass()通过的passer"，不是"有没有任何Actor"——门禁
// 拒绝时门不开，但如果这扇门有Script(可交互)，MeetOption选项照常弹出(拒绝通行和拒绝对话
// 是两件独立的事)。
UCLASS()
class FOREVER_API UForeverDoorComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UForeverDoorComponent();

	// 由ABuildingElement::BuildFloorSection/UForeverZoneFrameworkComponent调用一次：绑定
	// 对应哪个Core端Door*，按Door的世界坐标/朝向/外观生成门扇/门框/触发盒。inOwnerName只用
	// 于诊断日志(不参与任何判定逻辑)。
	void Init(Door* inDoor);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// PIE停止/退出游戏时置空door——和ACitizenElement/AVehicleElement同一套安全原则，这个
	// 组件的Tick/Overlap回调都要先判空。
	void ClearDoor() { door = nullptr; }

private:
	// 门扇的"枢轴"结构——Swing绕Z轴转(铰链)，Slide沿本地X轴平移(滑轨)，具体数值在Init()
	// 算好存成pivotClosed/pivotOpen两个目标相对Transform，Tick()只在两者间按SmoothStep插值，
	// 不每帧重新计算。
	// 纯C++结构体，不是USTRUCT——不需要反射/序列化，GC安全性由pivot/mesh各自的Attach
	// 层级保证(SetupAttachment挂到this/pivot之后，USceneComponent自己的AttachChildren这个
	// UPROPERTY数组就是真正的GC根，这里存一份指针只是方便按leafIndex访问，不需要再单独
	// UPROPERTY标记一次)。
	struct FLeaf {
		TObjectPtr<USceneComponent> pivot;
		TObjectPtr<UStaticMeshComponent> mesh;
		FTransform closedTransform;
		FTransform openTransform;
	};

	// 把一个mesh按desiredLocalSize(X=沿墙宽/Y=厚度/Z=高，UE单位)缩放，并按它自己的实际
	// 包围盒(有些资产pivot不在几何中心，占位Cube是中心pivot)算出本地offset，使包围盒中心
	// 落在desiredLocalCenterOffset这个相对pivot的本地坐标——这样不管资产pivot约定是什么，
	// 视觉网格都会精确填满[desiredLocalCenterOffset - size/2, +size/2]这个本地区间，见
	// door.md"实际摆位"一节。返回值已经RegisterComponent，调用方负责SetupAttachment之后
	// 再调这个函数设RelativeLocation/Scale(所以这个函数假定mesh组件已经Attach好)。
	void AlignMeshToBounds(UStaticMeshComponent* meshComp, UStaticMesh* mesh,
		const FVector& desiredLocalSize, const FVector& desiredLocalCenterOffset) const;

	// 创建所有门扇(leaves==1时1扇，==2时2扇)——按door->GetStyle()分Slide/Swing两套摆位+
	// 开合目标Transform的算法，统一在这一个函数里做(两种style分支共享"建pivot+建mesh+
	// AlignMeshToBounds+存closed/open transform"这套机制，只是目标Transform的算法不同，
	// 拆成两个独立函数反而要重复传一堆已经算好的中间量，见door.md)。
	void BuildLeaves(UStaticMesh* mesh, float doorWidth, float doorHeight, float thickness);

	void BuildFrame(UStaticMesh* frameMesh, float doorWidth, float doorHeight, float thickness);
	void BuildTriggerBox(float doorWidth, float doorHeight);

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// 按OtherActor解出(Citizen*,isPlayer)这对身份——ACitizenElement取GetCitizen()；玩家
	// pawn(含被ChangeControlChange换去操控的任意AForeverCharacter)isPlayer=true；
	// AVehicleElement有人驾驶时用GetPreviousPawn()递归判定，空车返回(nullptr,false)(只开
	// Open的门，见Door::CanPass)；其余(公交载具等)也是(nullptr,false)。
	void ResolvePasserIdentity(AActor* actor, Citizen*& outCitizen, bool& outIsPlayer) const;

	void UpdateOpenState();

	Door* door = nullptr;

	// 不是UPROPERTY——FLeaf不是USTRUCT，UHT不认；GC安全性见FLeaf注释。
	TArray<FLeaf> leaves;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> frameMeshComp;

	UPROPERTY()
	TObjectPtr<UBoxComponent> trigger;

	// 当前在触发盒内、且CanPass()通过的Actor集合——非空就该开，清空后delayedCloseSeconds
	// 秒再关，见door.md"开关状态"一节。TWeakObjectPtr防止Actor销毁后变成悬空指针。
	TSet<TWeakObjectPtr<AActor>> passers;
	float closeAtTime = -1.f; // <0表示没有正在倒计时的关门请求
	bool bWantsOpen = false;
	float openProgress = 0.f; // 0=全关，1=全开，Tick()按openSeconds插值趋近bWantsOpen对应的目标

	// 这扇门的交互名(烘焙好的FString，Overlap回调只读这份快照，不解引用door)——和
	// AVehicleElement::vehicleNameOnly同一套安全原则。
	FString interactNameOnly;
	TArray<FString> cachedOptions;
};
