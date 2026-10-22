#pragma once

#include "def.h"
#include "effect.h"

#include <memory>
#include <string>
#include <vector>


namespace wxdj {

// 招式基类
class Action {
protected:
	ACTION_TYPE type; // 招式类型
	ATTRIBUTE_TYPE attribute; // 招式属性
	int point; // 真气消耗
	int power; // 基础威力
	std::vector<std::shared_ptr<Effect>> effects; // 附加效果列表

public:
	Action(ACTION_TYPE type, ATTRIBUTE_TYPE attribute, int point, int power,
		std::vector<std::shared_ptr<Effect>> effects);
	virtual ~Action();

	// 获取招式类型
	ACTION_TYPE GetType() const;

	// 获取招式属性
	ATTRIBUTE_TYPE GetAttribute() const;

	// 获取招式真气量消耗
	int GetPoint() const;

	// 获取招式基础攻击力
	int GetPower() const;

	// 获取指定类型的效果
	std::shared_ptr<Effect> GetEffect(EFFECT_TYPE type) const;

	// 获取所有效果
	std::vector<std::shared_ptr<Effect>>& GetEffects();

	// 获取招式文本描述
	virtual std::string GetText() const = 0;

	static std::vector<std::string> attributeText;
};

// 跳过回合
class SkipAction : public Action {
public:
	SkipAction();
	virtual std::string GetText() const override { return "无"; }
};

// 积攒真气炸弹
class AccumulateAction : public Action {
public:
	AccumulateAction();
	virtual std::string GetText() const override { return "积攒真气炸弹"; }
};

// 释放真气炸弹
class BombAction : public Action {
public:
	BombAction();
	void SetPower(int power);
	virtual std::string GetText() const override { return "释放真气炸弹"; }
};

// 单发招式
class SingleAction : public Action {
private:
	std::string name; // 招式名称
	Realm realm; // 所需阶位

	friend class DualAction;

public:
	SingleAction(const std::string& name, ATTRIBUTE_TYPE attribute, int point, int power, Realm realm,
		std::vector<std::shared_ptr<Effect>> effects = {});
	SingleAction(const SingleAction& action); // 拷贝构造

	std::string GetName() const;
	Realm GetRealm() const;
	std::vector<std::shared_ptr<Effect>>& GetEffects();

	virtual std::string GetText() const override { return name; }
};

// 双发招式
class DualAction : public Action {
private:
	std::shared_ptr<SingleAction> action1, action2;

public:
	DualAction(std::shared_ptr<SingleAction> action1, std::shared_ptr<SingleAction> action2);
	virtual ~DualAction();

	std::shared_ptr<SingleAction> GetAction1() const;
	std::shared_ptr<SingleAction> GetAction2() const;

	virtual std::string GetText() const override;
};

}
