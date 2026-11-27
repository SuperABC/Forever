#pragma once

#include "CoreMinimal.h"
#include "Framework/ForeverFrameworkComponent.h"

#include <string>

#include "ForeverDeathFrameworkComponent.generated.h"

class ACitizenElement;
class AForeverFrameworkActor;
class Citizen;
class UDeathWidget;

// 玩家死亡流程：慢镜头+镜头拉远褪色+画面渐暗 → 结算界面(继续游戏/退出游戏) → 继续游戏=跳时间后在复活点重新操控，
// 详见ForeverDeathFrameworkComponent.md。只处理"玩家扮演的那个市民"(GetPlayerCitizen)被打死，NPC照旧。
UCLASS()
class FOREVER_API UForeverDeathFrameworkComponent : public UForeverFrameworkComponent
{
	GENERATED_BODY()

public:
	UForeverDeathFrameworkComponent();
	virtual void TickComponent(float deltaTime, ELevelTick tickType, FActorComponentTickFunction* thisTickFunction) override;

	// FForeverDamage::DamageCitizen在市民被打死、发完citizen_decease之后调：死的是玩家扮演的市民就接管(开始慢镜头、
	// 发player_decease)并返回true，调用方不再撤掉它的Actor(尸体留着演慢镜头，继续游戏时才撤)；不是玩家/已经在死亡流程里返回false。
	// element=死者在场景里的Actor(玩家开车时是藏起来的那个)；其余是player_decease的字段。
	bool HandlePlayerDeath(ACitizenElement* element, Citizen* citizen, const std::string& killer, const std::string& weapon,
		const std::string& part, const std::string& location);

	// 死亡流程中(慢镜头/结算界面/复活跳时间中)：PlayerController据此屏蔽手机/地图/暂停等热键。
	bool IsPlayerDead() const { return state != EState::None; }
	// 死掉、还没复活的那个市民(复活跳时间期间没有Pawn，GetPlayerCitizen退回这里)，不在死亡流程里为nullptr。
	Citizen* GetDeadCitizen() const { return state != EState::None ? deadCitizen : nullptr; }

	// 结算界面的两个按钮(UDeathWidget转过来)。只在结算界面显示时有效。
	void Continue();
	void ExitToMenu();

	// 慢镜头期间的全局时间缩放(0..1)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death")
	float slowMotionDilation = 0.2f;

	// 慢镜头+渐暗持续的真实秒数，之后世界停住、出现按钮。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death")
	float slowMotionDuration = 3.f;

	// 继续游戏跳过的天数。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death")
	int32 reviveSkipDays = 15;

private:
	enum class EState : uint8 {
		None,     // 没死
		Dying,    // 慢镜头+渐暗
		Menu,     // 世界停住，等玩家点按钮
		Reviving, // 点了继续游戏：黑屏跳时间，跳完复活
	};

	void EnterMenu();
	// 跳时间黑屏盖住之后撤掉尸体(玩家这时没有Pawn)。幂等。
	void RemoveCorpse();
	// 跳完时间(或没法跳时直接)：在复活点复活并重新操控，发player_revive，结束流程。
	void Revive();
	AForeverFrameworkActor* GetFramework() const;
	UDeathWidget* GetWidget() const;

	EState state = EState::None;
	float elapsed = 0.f; // 慢镜头已经过去的真实秒数
	// 死者。Core的市民对象整局都在(死了也不删)，这里只在流程期间持有；复活时还是按名字重新找一遍。
	Citizen* deadCitizen = nullptr;
	std::string deadName;
	TWeakObjectPtr<ACitizenElement> corpse;
	bool bCorpseRemoved = false;
};
