#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ForeverCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;
class UForeverWeaponComponent;
class UForeverThrowableComponent;
class UForeverWeaponVisualComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;
class AForeverFrameworkActor;

UCLASS()
class FOREVER_API AForeverCharacter : public ACharacter
{
	GENERATED_BODY()

	// 投掷物激活/用完/手不够时要直接开关瞄准状态(StartAim/StopAim是protected)。
	friend class UForeverThrowableComponent;
	// 肩膀里的武器被拖走/换掉时要重新ActivateShoulderWeapon(见ForeverWeaponVisualComponent.cpp)。
	friend class UForeverWeaponVisualComponent;

public:
	// 按性别换身体网格(男Manny/女Quinn)，并重新摆放头像方块。由ACitizenElement::Init调用。
	void ApplyBodyMesh(bool bFemale);

	// 换头像贴图(市民按各自的头像文件贴)，见UForeverAvatarSubsystem。
	void SetAvatarTexture(UTexture2D* texture);

	AForeverCharacter();

	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

	// 玩家操控的这个角色死了(UForeverDeathFrameworkComponent调)：停火/退出瞄准、第一人称切回第三人称、身体变布娃娃(没有物理资产就保持原姿势)，
	// 之后每帧按真实时间把镜头拉远、绕着尸体慢慢转、画面褪色。不可逆：这个Actor随后会被撤掉。见ForeverCharacter.md"死亡镜头"。
	void BeginDeathPresentation();
	FORCEINLINE bool IsInDeathPresentation() const { return bDeathPresentation; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void Move(const FInputActionValue& value);
	void Look(const FInputActionValue& value);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> cameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> followCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> firstPersonCamera;

	// 头像方块：用来区分长得一样的角色，见ForeverCharacter.md的头像方块一节。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> avatarMesh;

	// 身体网格：男Manny、女Quinn(同一骨骼，共用动画蓝图)，ApplyBodyMesh按性别切换。
	UPROPERTY(EditDefaultsOnly, Category = "Mesh")
	TObjectPtr<USkeletalMesh> maleBodyMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Mesh")
	TObjectPtr<USkeletalMesh> femaleBodyMesh;

	// 按骨骼参考姿势摆放头像方块(avatarOffset/avatarRotation等)，换身体网格后要重新摆。
	void PlaceAvatar();

	// 方块边长(厘米)；avatarOffset相对head骨骼(骨骼局部坐标)；avatarRotation相对角色本身(PlaceAvatar抵消骨骼朝向，0=+X面朝正前)。
	UPROPERTY(EditDefaultsOnly, Category = "Avatar")
	float avatarSize = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "Avatar")
	FVector avatarOffset = FVector(10.f, 0.f, 0.f);

	// 在avatarOffset基础上按角色方向微调(厘米，+X前、+Y右、+Z上)；当前往下5cm盖住下巴。
	UPROPERTY(EditDefaultsOnly, Category = "Avatar")
	FVector avatarOffsetInCharacter = FVector(0.f, 0.f, -5.f);

	// 绕Z轴顺时针(俯视)转90度，让头顶那一面的头像摆正——UE里正yaw俯视是顺时针。
	UPROPERTY(EditDefaultsOnly, Category = "Avatar")
	FRotator avatarRotation = FRotator(0.f, 90.f, 0.f);

	// 头像材质(需要名为Texture的贴图参数)和默认头像贴图；市民的头像经SetAvatarTexture替换。
	UPROPERTY(EditDefaultsOnly, Category = "Avatar")
	TObjectPtr<UMaterialInterface> avatarBaseMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Avatar")
	TObjectPtr<UTexture2D> avatarTexture;

	// 头像方块的动态材质实例(PlaceAvatar建，SetAvatarTexture更新贴图参数)。
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> avatarMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> moveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> lookAction;

	// 鼠标视角单独一个Input Action：增量输入和摇杆模拟量需要不同的Mapping配置，但都路由到Look()。
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> mouseLookAction;

	// 在PossessedBy/UnPossessed里跟随占有状态增删(CONVENTIONS第7节)。
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> inputMapping;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> inputLookMapping;

	void ToggleCameraView();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	bool bIsFirstPerson = false;

	void StartSprint();
	void StopSprint();

	// MeetOption对话选项UI：滚轮上下移动高亮/F键确认，转发给AForeverPlayerController::GetMeetOptionWidget()。
	void MeetOptionFocusUp();
	void MeetOptionFocusDown();
	void MeetOptionSelect();

	// 武器系统：左键开火/R键换弹/数字键1、2切枪，转发给自己身上的weaponComponent。
	void StartFireWeapon();
	void StopFireWeapon();
	void ReloadWeapon();
	void SwitchToWeapon1();
	void SwitchToWeapon2();

	// 数字键1/2的实现：激活左肩(slot=1)/右肩(slot=2)挂着的武器(若有)，同一时刻最多一把激活。
	void ActivateShoulderWeapon(int32 slot);

	// 最近一次ActivateShoulderWeapon的slot(1=左肩/2=右肩，0=还没激活过)，存档用。
	int32 activeShoulderSlot = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UForeverWeaponComponent> weaponComponent;

	// 武器可视化，见ForeverWeaponVisualComponent.md。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UForeverWeaponVisualComponent> weaponVisualComponent;

	// 投掷物：数字键3，短按切换、长按弹出选择列表，与肩上的武器互斥，见ForeverThrowableComponent.md。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UForeverThrowableComponent> throwableComponent;

	void ThrowKeyPressed();
	void ThrowKeyReleased();
	// 按下3的**真实**时间(秒)——世界时间会被长按列表的慢放影响，不能用。
	double throwKeyPressTime = 0.0;
	// 3键正按着、还没判断出是长按。Tick里到阈值就弹出选择列表。
	bool bThrowKeyHeld = false;
	// 这次按键已经用来弹出列表(或尝试弹出)：随后的松开不再当作短按切换。下一次按下重置，所以
	// 即使松开事件在UI Only输入模式下丢了，也不会吞掉下一次短按。
	bool bSwallowThrowRelease = false;
	void OpenThrowList();

	// 按住超过这个真实时间(秒)算长按，弹出选择列表；没到就松开算短按，切换到下一个投掷物。
	UPROPERTY(EditDefaultsOnly, Category = "Throw")
	float longPressThreshold = 0.3f;

	// 瞄准(默认右键，按住生效)：越肩瞄准，插值cameraBoom的SocketOffset/TargetArmLength，见md。
	void StartAim();
	void StopAim();

	// 统一计算朝向模式(第一人称或瞄准时朝向跟摄像机)，ToggleCameraView/StartAim/StopAim都调。
	void UpdateRotationMode();

	bool bIsAiming = false;

	// 非瞄准状态下cameraBoom的目标值(不能读cameraBoom当前值，那是插值中间态)。
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Aim")
	FVector defaultSocketOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|Aim")
	float defaultArmLength = 400.f;

	// 瞄准时cameraBoom的目标值：Y正=向右肩偏移，Z正=略升高，配合更短的臂长。数值为初始值，需PIE调。
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Aim")
	FVector aimSocketOffset = FVector(0.f, 60.f, 40.f);

	UPROPERTY(EditDefaultsOnly, Category = "Camera|Aim")
	float aimArmLength = 150.f;

	// 瞄准过渡的插值速度(VInterpTo/FInterpTo的InterpSpeed)，越大越快；必须是有限值才平滑。
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Aim")
	float aimTransitionSpeed = 10.f;

	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float walkSpeed = 180.f; // 平常走路的基准速度；市民脚本speed倍率(1.0=按它走)和Core推算的走路速度都以它为准

	// 冲刺速度=walkSpeed×倍数。走路速度从500降到180时倍数同步提到27.8，冲刺速度仍约5000。
	UPROPERTY(EditDefaultsOnly, Category = "Movement")
	float sprintSpeedMultiplier = 27.8f;

	bool bIsSprinting = false;

	// 第一人称相机在head骨骼基础上再抬高的距离(厘米，世界向上)。
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float firstPersonCameraRaise = 15.f;

	// 游泳/潜水时第三人称弹簧臂连接点相对胶囊体中心的高度(厘米)，=头部高度；平时在胶囊体中心。
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float cameraBoomHeight = 70.f;
	FVector boomBaseLocation = FVector::ZeroVector; // 弹簧臂平时的相对位置

	// ---- 游泳/潜水(见ForeverCharacter.md"游泳") ----
	// 两个状态：游泳(浮在水面，不下潜，镜头不能朝上看)和潜水(可上下，镜头全视角)。从地面入水先进游泳；
	// 游泳时按住冲刺并且视线朝下(与竖直向下的点积大于swimDiveLookDot)才切潜水；潜水回到水面且不再满足下潜条件时回游泳。
	// 入水：脚底被水淹没的深度达到身高(胶囊体全高)的这个比例才切成游泳。
	UPROPERTY(EditDefaultsOnly, Category = "Swim")
	float swimEnterDepthRatio = 0.8f;

	// 出水：脚下水底到水面的深度低于身高的这个比例才切回走路(只能从缓坡沙滩走上岸，没有跳跃出水)。必须小于入水比例，中间是滞回区间。
	UPROPERTY(EditDefaultsOnly, Category = "Swim")
	float swimExitDepthRatio = 0.6f;

	// 游泳速度=walkSpeed×该值；按住冲刺键再×swimSprintMultiplier(快速游泳)。
	UPROPERTY(EditDefaultsOnly, Category = "Swim")
	float swimSpeedMultiplier = 0.7f;

	UPROPERTY(EditDefaultsOnly, Category = "Swim")
	float swimSprintMultiplier = 2.5f;

	// 浮在水面时胶囊体中心相对水面的高度(厘米，正=中心在水面上方)，游泳(原地/游动)统一用这一个值，高度不随姿势变。
	// 要让水面到脖子、头露一点就调它。
	UPROPERTY(EditDefaultsOnly, Category = "Swim")
	float swimFloatOffset = -60.f;

	// 游泳/潜水时胶囊体缩成这个半高(厘米，半径不变)，以中心为基准缩。要小于"浮高对应的下端深度"，
	// 即 < 出水深度(swimExitDepthRatio×身高) + swimFloatOffset，这样走到出水深度之前胶囊体不会先顶住岸坡。
	UPROPERTY(EditDefaultsOnly, Category = "Swim")
	float swimCapsuleHalfHeight = 45.f;

	// 游泳→潜水：视线方向与竖直向下的点积大于该值(0.5≈俯角30度，即视线与水平面夹角>30度)，并且按住冲刺。
	UPROPERTY(EditDefaultsOnly, Category = "Swim")
	float swimDiveLookDot = 0.5f;

	// 潜水时没有任何移动输入的上浮速度(厘米/秒)，0=悬停不浮。
	UPROPERTY(EditDefaultsOnly, Category = "Swim")
	float swimBuoyancySpeed = 0.f;

	// 水平速度超过游泳最大速度的部分的衰减速度(1/秒，指数衰减，越大入水后减速越快)。冲刺入水时速度可达几千厘米/秒。
	UPROPERTY(EditDefaultsOnly, Category = "Swim")
	float swimOverspeedDecay = 8.f;

	// 游泳时的减速度，越大松开按键停得越快。
	UPROPERTY(EditDefaultsOnly, Category = "Swim")
	float swimBrakingDeceleration = 400.f;

	// 潜水时身体俯仰=镜头俯仰×该比例(0=身体一直平的，1=和镜头方向完全一致)。
	UPROPERTY(EditDefaultsOnly, Category = "Swim")
	float diveBodyPitchFollow = 0.6f;

	// 潜水时按住空格上浮，身体额外向上转的角度(度)。
	UPROPERTY(EditDefaultsOnly, Category = "Swim")
	float diveRiseBodyPitch = 35.f;

	// 身体俯仰的插值速度(1/秒)。
	UPROPERTY(EditDefaultsOnly, Category = "Swim")
	float diveBodyPitchInterpSpeed = 5.f;

	// 给动画蓝图读的状态(蓝图里Cast成AForeverCharacter)：游泳或潜水(两者都为真，用同一套游泳动画) / 在潜水 / 头部在水面以下 / 按着冲刺键在游。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Swim")
	bool bIsSwimming = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Swim")
	bool bIsDiving = false;

	// 游泳/潜水且有移动输入(趴姿)。动画蓝图可用它在"游动/踩水"间切(比用速度同步)。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Swim")
	bool bIsSwimMoving = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Swim")
	bool bIsUnderwater = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Swim")
	bool bIsFastSwimming = false;

	// 跳跃键：潜水时是"上浮"，否则是普通跳跃(游泳时无效)。
	void JumpPressed();
	void JumpReleased();
	bool bSwimUpHeld = false;

	// 每帧判定入水/出水、游泳/潜水切换、浮力和水下状态，Tick里调；只对玩家操控的角色生效。
	void UpdateWaterState(float DeltaTime);
	void EnterSwimming();
	void ExitSwimming();
	void SetDiving(bool bDive);
	// 游泳时限制镜头不能朝上看，潜水/出水后恢复。
	void SetSwimCameraLimit(bool bLimit);
	// 身体俯仰(转网格，胶囊体保持竖直)和游泳时的偏航朝向。
	void UpdateSwimBody(float DeltaTime);

	// 按是否冲刺/游泳刷新MaxWalkSpeed和MaxFlySpeed。
	void UpdateMoveSpeeds();

	// ---- 死亡镜头(BeginDeathPresentation) ----
	// 镜头拉远到的臂长(厘米)。
	UPROPERTY(EditDefaultsOnly, Category = "Death")
	float deathArmLength = 550.f;

	// 镜头俯视角(度，负=往下看)。
	UPROPERTY(EditDefaultsOnly, Category = "Death")
	float deathCameraPitch = -35.f;

	// 绕尸体转的速度(度/真实秒)。
	UPROPERTY(EditDefaultsOnly, Category = "Death")
	float deathOrbitSpeed = 10.f;

	// 臂长/俯仰插值速度(FInterpTo的InterpSpeed，按真实时间)。
	UPROPERTY(EditDefaultsOnly, Category = "Death")
	float deathCameraInterpSpeed = 1.5f;

	// 褪色到的饱和度(1=原色，0=黑白)和用的真实秒数。
	UPROPERTY(EditDefaultsOnly, Category = "Death")
	float deathSaturation = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "Death")
	float deathDesaturateDuration = 2.5f;

	bool bDeathPresentation = false;
	float deathElapsed = 0.f; // 真实秒
	void TickDeathPresentation(float realDeltaSeconds);

	// 自己的游泳状态：游泳借用MOVE_Flying实现(没有水体PhysicsVolume，引擎的MOVE_Swimming会立刻被踢回Falling)。
	bool bSwimState = false;
	bool bCapsuleShrunk = false;
	float standingCapsuleRadius = 34.f;
	float standingCapsuleHalfHeight = 96.f; // 站立时的胶囊体尺寸，BeginPlay缓存
	bool bDiveState = false;
	float bodyPitch = 0.f;       // 当前身体俯仰(度)
	bool bCameraLimited = false;
	float savedViewPitchMax = 89.9f;
	FVector meshDefaultLocation = FVector::ZeroVector;
	FQuat meshDefaultRotation = FQuat::Identity;
	bool bMeshDefaultCaptured = false;
	mutable TWeakObjectPtr<AForeverFrameworkActor> cachedFramework;

public:
	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return cameraBoom; }
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return followCamera; }
	FORCEINLINE UCameraComponent* GetFirstPersonCamera() const { return firstPersonCamera; }
	// 供UForeverWeaponComponent::Fire()判断该用哪个摄像机做开火射线的起点/方向。
	FORCEINLINE bool IsFirstPerson() const { return bIsFirstPerson; }
	FORCEINLINE UForeverWeaponComponent* GetWeaponComponent() const { return weaponComponent; }
	FORCEINLINE UForeverWeaponVisualComponent* GetWeaponVisualComponent() const { return weaponVisualComponent; }
	FORCEINLINE UForeverThrowableComponent* GetThrowableComponent() const { return throwableComponent; }
	// 瞄准中且手上激活的是投掷物——供AForeverHUD决定画投掷曲线还是准星。
	bool IsAimingThrow() const;

	// 存档/读档：当前激活的肩膀槽位；读档时RestoreWeaponState按存档重新激活并恢复弹匣余量。
	int32 GetActiveShoulderSlot() const { return activeShoulderSlot; }
	void RestoreWeaponState(int32 slot, int32 magazineAmmo);
	// 供AForeverHUD判断要不要画屏幕中心的准星。
	FORCEINLINE bool IsAiming() const { return bIsAiming; }

	// 世界坐标(厘米)所在地图格有没有水：有则返回水面高度和该点地形(水底)高度(世界Z，厘米)。水体数据来自Core的Map::GetWater。
	// outSurfaceZ是带波浪的水面高度(本格水位+WaterWave.inc的波浪，和水面材质WPO是同一个表达式)；
	// outBaseSurfaceZ(可选)是不带波浪的本格水位，入水/出水阈值用它，免得在阈值边缘被浪推着抖动。
	bool QueryWater(const FVector& worldLocation, float& outSurfaceZ, float& outBedZ, float* outBaseSurfaceZ = nullptr) const;
	FORCEINLINE bool IsSwimmingNow() const { return bIsSwimming; }
	FORCEINLINE bool IsDivingNow() const { return bIsDiving; }
	FORCEINLINE bool IsUnderwaterNow() const { return bIsUnderwater; }
};
