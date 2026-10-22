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
		int hp, int atk, int mp, int agi, int recover);

	// 获取姓名
	std::string GetName() const;

	// 获取生命值
	int GetCurrentHP() const;

	// 获取武力值
	int GetCurrentATK() const;

	// 获取真气量
	int GetCurrentMP() const;

	// 获取已积攒的真气炸弹能量(释放前一直保留)
	int GetBomb() const;

	// 获取敏捷值
	int GetAGI() const;

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
	int AGI;
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
