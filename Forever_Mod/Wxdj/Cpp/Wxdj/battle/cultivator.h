#pragma once

#include "def.h"
#include "action.h"

#include <memory>
#include <string>
#include <vector>


namespace wxdj {

// 修炼者
class Cultivator {
public:
	Cultivator(const std::string& name, const std::string& file,
		int hp, int atk, int mp, int agi, int recover,
		float agiLoss = 1.f, float agiRecover = 0.2f,
		float bombMultiplier = BOMB_ACCUMULATE_MULTIPLIER);

	// 获取姓名
	std::string GetName() const;

	// 获取生命值
	int GetCurrentHP() const;

	// 获取武力值
	int GetCurrentATK() const;

	// 获取真气量
	int GetCurrentMP() const;

	// 获取初始(上限)健康值/武力值/真气量
	int GetMaxHP() const;
	int GetMaxATK() const;
	int GetMaxMP() const;

	// 获取已积攒的真气炸弹能量(释放前一直保留)
	int GetBomb() const;

	// 炸弹积攒倍率(个人属性，由所在阶位决定)
	float GetBombMultiplier() const;

	// 获取当前敏捷值(闪避一次损失agiLoss点，每回合自动恢复agiRecover点，不超过初始值)
	float GetAGI() const;
	float GetMaxAGI() const;
	float GetAGILoss() const;
	float GetAGIRecover() const;

	// 现在跳过得起吗：跳过要损失agiLoss点敏捷，之后敏捷不能低于1
	bool CanDodge() const;

	// 跳过一次，损失agiLoss点敏捷(不会低于1)
	void ConsumeAGI();
	// 这次跳过要损失多少敏捷：agiLoss + 连续跳过次数x0.5；出招/积攒/真放炸弹后连续次数清零
	float SkipLoss() const;
	void ResetSkipStreak();
	int GetSkipStreak() const;

	// 每回合自动恢复敏捷
	void RecoverAGI();

	// 获取阶位
	int GetCurrentRealm() const;

	// 获取全部单招
	const std::vector<SingleAction>& GetActions() const;

	// 通过索引获取单招
	std::shared_ptr<Action> GetAction(int idx);

	// 通过名称获取单招
	std::shared_ptr<Action> GetAction(std::string name);

	// 通过索引获取双招
	std::shared_ptr<Action> GetAction(int idx1, int idx2);

	// 通过名称获取双招
	std::shared_ptr<Action> GetAction(std::string name1, std::string name2);

	// 获取是否存活
	bool IsAlive() const;

	// 检查真气量是否足够
	bool ValidateAction(const std::shared_ptr<Action> action) const;

	// 回复生命值
	void RecoverMP(int amount);
	void UndoMP(int amount); // 撤销已经回复的真气(空炸弹的回合不回复真气)

	// 消耗真气量
	void ConsumeMP(int amount);

	// 回复武力值
	void RecoverATK(int amount);

	// 回复生命值
	void RecoverHP(int amount);

	// 受到伤害
	void TakeDamage(int amount, bool physical = false);

	// 闪避成功
	void DodgeSuccess();

	// 更新阶位
	void UpdateRealm();

	// 积攒真气炸弹
	void AccumulatePower();

	// 引爆真气炸弹
	int BombPower();

private:
	std::string name;
	int maxHP;
	int currentHP;
	int maxATK;
	int currentATK;
	int maxMP;
	int currentMP;
	float AGI; // 当前敏捷值
	float maxAGI; // 初始(上限)敏捷值
	float agiLoss; // 每次跳过损失的敏捷值(连续跳过会递增)
	int skipStreak = 0; // 已经连续跳过了几次
	float agiRecover; // 每回合自动恢复的敏捷值
	float bombMultiplier; // 炸弹积攒倍率
	Realm currentRealm;

	std::vector<SingleAction> actions; // 该修仙者拥有的招式列表
	int recover; // 每回合自动回复真气量
	int bomb; // 积攒的真气炸弹能量

	// 读取所有单招
	bool LoadActions(const std::string& filename);

	// 解析阶位
	Realm ParseRealm(const std::string& realm);

	// 解析属性
	ATTRIBUTE_TYPE ParseAttribute(const std::string& attr);

	// 解析效果
	std::shared_ptr<Effect> ParseEffect(const std::string& effect);
};

}
