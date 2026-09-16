#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "CitizenElement.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class Citizen;
struct FHitResult;
class UForeverPopulaceFrameworkComponent;

// 一个citizen在场景里显形时对应的Actor——和ABuildingElement同一套"每个Core实例对应一个
// 强类型Actor子类"模式（Actor自己持有指向Core对象的裸指针+指向共享框架组件的
// TWeakObjectPtr），区别只有一点：ABuildingElement开局为每栋building常驻生成一个，
// ACitizenElement按玩家距离由UForeverPopulaceFrameworkComponent::TickComponent流式
// SpawnActor/Destroy，不常驻，详见Source/Forever/Element/CitizenElement.md。
//
// 基类是ACharacter，不是纯AActor——用户明确要求citizen不能只是静态骨架，必须是真正的
// Character（自带CapsuleComponent/SkeletalMeshComponent/CharacterMovementComponent），
// 为将来的行走AI预留好组件骨架，和玩家角色AForeverCharacter同一个基类。这次仍然不驱动
// 任何移动（CharacterMovementComponent这次显式设成MOVE_None，见.cpp构造函数），只是
// 提前把骨架搭对，避免以后加AI时还要把AActor整个换成ACharacter重新搭一遍。
UCLASS()
class FOREVER_API ACitizenElement : public ACharacter
{
	GENERATED_BODY()

public:
	ACitizenElement();

	// 由UForeverPopulaceFrameworkComponent::TickComponent在流式判定"该显形"时调用一次：
	// 绑定这个Element对应哪个Citizen，摆放mesh(HasPosition()为true直接读记录位置，否则用
	// 房间中心+随机抖动、并把算出来的位置写回Citizen)，建一个靠近检测碰撞盒。
	void Init(Citizen* inCitizen, UForeverPopulaceFrameworkComponent* inFramework);

	// PIE停止/退出游戏时置空citizen——和ABuildingElement::EndPlay同一套安全原则：只保证
	// "自己的EndPlay一跑完，自己不会再解引用citizen"，不需要关心和框架Actor/其它Element
	// 的EndPlay谁先谁后。
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BuildProximityBox();

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	Citizen* citizen = nullptr;
	TWeakObjectPtr<UForeverPopulaceFrameworkComponent> framework;

	UPROPERTY()
	TObjectPtr<UBoxComponent> proximityBox; // 玩家靠近检测——和流式生成/销毁的距离判定是两回事

	FString collisionLabel; // Overlap回调只读这份烘焙好的字符串，绝不解引用citizen
};
