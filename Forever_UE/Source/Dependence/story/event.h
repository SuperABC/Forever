#pragma once

#include "../common/utility.h"
#include "../common/error.h"
#include "expression.h"

#include <string>


#undef GetMessage


// 事件基类。老工程里31个具体事件子类的字段级Match逻辑这次没有照抄——除了GameStartEvent
// （已实现，见下），其余30个子类只搬运了字段定义（构造函数/getter/setter），继承这里的默认
// Match实现（按类型比较），等某个具体事件类型被点名实现字段级匹配时再单独override。
class Event {
public:
	/*
	* 构造事件
	*/
	Event();

	/*
	* 析构事件
	*/
	virtual ~Event();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const = 0;

	/*
	* 判断是否与给定事件匹配。默认实现只比较类型，不看字段——多数子类这次还没有字段级匹配
	* 逻辑，继承这份默认实现即可；GameStartEvent本来语义就是纯类型匹配，也直接用默认实现。
	* @e: 待匹配事件
	* @context: 变量路由上下文
	*/
	virtual bool Match(Event* e, const ScriptContext& context);

	/*
	* 获取控制条件
	*/
	const std::string& GetCondition() const;

	/*
	* 设置控制条件
	* @condition: 条件表达式
	*/
	void SetCondition(const std::string& condition);

	/*
	* 查询local.前缀变量：把这个事件实例自己的字段暴露给表达式引擎。默认没有任何字段可查，
	* 具体子类实现真逻辑时按需override，暴露自己的字段。
	* @name: local.之后的变量名（如"message"）
	* @return: {是否存在, 值}
	*/
	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const;

private:
	// 控制条件
	std::string condition;
};

// 游戏开始（已实现：唯一有真实广播/匹配逻辑的事件类型）
class GameStartEvent : public Event {
public:
	/*
	* 构造游戏开始事件
	*/
	GameStartEvent();

	/*
	* 析构游戏开始事件
	*/
	virtual ~GameStartEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

private:
};

// 全局消息
class GlobalMessageEvent : public Event {
public:
	/*
	* 构造全局消息事件
	* @message: 消息内容
	*/
	GlobalMessageEvent(std::string message);

	/*
	* 析构全局消息事件
	*/
	virtual ~GlobalMessageEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置消息内容
	* @message: 消息内容
	*/
	void SetMessage(std::string message);

	/*
	* 获取消息内容
	*/
	const std::string& GetMessage() const;

private:
	// 消息内容
	std::string message;
};

// 选项对话
class OptionDialogEvent : public Event {
public:
	/*
	* 构造选项对话事件
	* @name, option: 对话目标名称与选项文本
	*/
	OptionDialogEvent(std::string name, std::string option);

	/*
	* 析构选项对话事件
	*/
	virtual ~OptionDialogEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置目标名称
	* @name: 名称
	*/
	void SetName(std::string name);

	/*
	* 获取目标名称
	*/
	const std::string& GetName() const;

	/*
	* 设置选项文本
	* @option: 选项文本
	*/
	void SetOption(std::string option);

	/*
	* 获取选项文本
	*/
	const std::string& GetOption() const;

private:
	// 选项名称
	std::string name;

	// 选项文本
	std::string option;
};

// 全局对话
class GlobalDialogEvent : public Event {
public:
	/*
	* 构造全局对话事件
	* @name, option: 对话目标名称与选项文本
	*/
	GlobalDialogEvent(std::string name, std::string option);

	/*
	* 析构全局对话事件
	*/
	virtual ~GlobalDialogEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置目标名称
	* @name: 名称
	*/
	void SetName(std::string name);

	/*
	* 获取目标名称
	*/
	const std::string& GetName() const;

	/*
	* 设置选项文本
	* @option: 选项文本
	*/
	void SetOption(std::string option);

	/*
	* 获取选项文本
	*/
	const std::string& GetOption() const;

private:
	// 选项名称
	std::string name;

	// 选项文本
	std::string option;
};

// 对话完成
class SpeakingFinishEvent : public Event {
public:
	/*
	* 构造对话完成事件
	* @label: 对话标签
	*/
	SpeakingFinishEvent(std::string label);

	/*
	* 析构对话完成事件
	*/
	virtual ~SpeakingFinishEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置对话标签
	* @label: 标签
	*/
	void SetLabel(std::string label);

	/*
	* 获取对话标签
	*/
	const std::string& GetLabel() const;

private:
	// 对话标签
	std::string label;
};

// 进入园区
class EnterZoneEvent : public Event {
public:
	/*
	* 构造进入园区事件
	* @zone: 园区名称
	*/
	EnterZoneEvent(std::string zone);

	/*
	* 析构进入园区事件
	*/
	virtual ~EnterZoneEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置园区名称
	* @zone: 园区名称
	*/
	void SetZone(std::string zone);

	/*
	* 获取园区名称
	*/
	const std::string& GetZone() const;

private:
	// 园区名称
	std::string zone;
};

// 离开园区
class LeaveZoneEvent : public Event {
public:
	/*
	* 构造离开园区事件
	* @zone: 园区名称
	*/
	LeaveZoneEvent(std::string zone);

	/*
	* 析构离开园区事件
	*/
	virtual ~LeaveZoneEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置园区名称
	* @zone: 园区名称
	*/
	void SetZone(std::string zone);

	/*
	* 获取园区名称
	*/
	const std::string& GetZone() const;

private:
	// 园区名称
	std::string zone;
};

// 进入建筑
class EnterBuildingEvent : public Event {
public:
	/*
	* 构造进入建筑事件
	* @zone, building: 园区名称与建筑名称
	*/
	EnterBuildingEvent(std::string zone, std::string building);

	/*
	* 析构进入建筑事件
	*/
	virtual ~EnterBuildingEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置园区名称
	* @zone: 园区名称
	*/
	void SetZone(std::string zone);

	/*
	* 获取园区名称
	*/
	const std::string& GetZone() const;

	/*
	* 设置建筑名称
	* @building: 建筑名称
	*/
	void SetBuilding(std::string building);

	/*
	* 获取建筑名称
	*/
	const std::string& GetBuilding() const;

private:
	// 园区名称
	std::string zone;

	// 建筑名称
	std::string building;
};

// 离开建筑
class LeaveBuildingEvent : public Event {
public:
	/*
	* 构造离开建筑事件
	* @zone, building: 园区名称与建筑名称
	*/
	LeaveBuildingEvent(std::string zone, std::string building);

	/*
	* 析构离开建筑事件
	*/
	virtual ~LeaveBuildingEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置园区名称
	* @zone: 园区名称
	*/
	void SetZone(std::string zone);

	/*
	* 获取园区名称
	*/
	const std::string& GetZone() const;

	/*
	* 设置建筑名称
	* @building: 建筑名称
	*/
	void SetBuilding(std::string building);

	/*
	* 获取建筑名称
	*/
	const std::string& GetBuilding() const;

private:
	// 园区名称
	std::string zone;

	// 建筑名称
	std::string building;
};

// 进入房间
class EnterRoomEvent : public Event {
public:
	/*
	* 构造进入房间事件
	* @zone, building, room: 园区、建筑与房间名称
	*/
	EnterRoomEvent(std::string zone, std::string building, std::string room);

	/*
	* 析构进入房间事件
	*/
	virtual ~EnterRoomEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置园区名称
	* @zone: 园区名称
	*/
	void SetZone(std::string zone);

	/*
	* 获取园区名称
	*/
	const std::string& GetZone() const;

	/*
	* 设置建筑名称
	* @building: 建筑名称
	*/
	void SetBuilding(std::string building);

	/*
	* 获取建筑名称
	*/
	const std::string& GetBuilding() const;

	/*
	* 设置房间名称
	* @room: 房间名称
	*/
	void SetRoom(std::string room);

	/*
	* 获取房间名称
	*/
	const std::string& GetRoom() const;

private:
	// 园区名称
	std::string zone;

	// 建筑名称
	std::string building;

	// 房间名称
	std::string room;
};

// 离开房间
class LeaveRoomEvent : public Event {
public:
	/*
	* 构造离开房间事件
	* @zone, building, room: 园区、建筑与房间名称
	*/
	LeaveRoomEvent(std::string zone, std::string building, std::string room);

	/*
	* 析构离开房间事件
	*/
	virtual ~LeaveRoomEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置园区名称
	* @zone: 园区名称
	*/
	void SetZone(std::string zone);

	/*
	* 获取园区名称
	*/
	const std::string& GetZone() const;

	/*
	* 设置建筑名称
	* @building: 建筑名称
	*/
	void SetBuilding(std::string building);

	/*
	* 获取建筑名称
	*/
	const std::string& GetBuilding() const;

	/*
	* 设置房间名称
	* @room: 房间名称
	*/
	void SetRoom(std::string room);

	/*
	* 获取房间名称
	*/
	const std::string& GetRoom() const;

private:
	// 园区名称
	std::string zone;

	// 建筑名称
	std::string building;

	// 房间名称
	std::string room;
};

// 小游戏结果
class PuzzleResultEvent : public Event {
public:
	/*
	* 构造小游戏结果事件
	* @result: 结果值
	*/
	PuzzleResultEvent(std::string result);

	/*
	* 析构小游戏结果事件
	*/
	virtual ~PuzzleResultEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置结果值
	* @result: 结果值
	*/
	void SetResult(std::string result);

	/*
	* 获取结果值
	*/
	const std::string& GetResult() const;

private:
	// 小游戏结果值
	std::string result;
};

// 交易结果
class TransactionResultEvent : public Event {
public:
	/*
	* 构造交易结果事件
	* @result: 是否成功
	* @name: 交易对象姓名，为空代表玩家
	*/
	TransactionResultEvent(std::string result, std::string name);

	/*
	* 析构交易结果事件
	*/
	virtual ~TransactionResultEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置交易结果
	* @result: 是否成功
	*/
	void SetResult(std::string result);

	/*
	* 获取交易结果
	*/
	const std::string& GetResult() const;

	/*
	* 设置交易对象姓名
	* @name: 姓名，为空代表玩家
	*/
	void SetName(std::string name);

	/*
	* 获取交易对象姓名
	*/
	const std::string& GetName() const;

private:
	// 是否成功
	std::string result;

	// 交易对象姓名，为空代表玩家
	std::string name;
};

// 物品操作结果
class ObjectResultEvent : public Event {
public:
	/*
	* 构造物品操作结果事件
	* @action: 操作类型，"give"/"remove"，为空时 Match 匹配任意操作
	* @object: 被操作的 object 类型
	* @result: 是否全部成功
	* @num: 失败时剩余未操作数量，-1 表示不指定
	*/
	ObjectResultEvent(std::string action, std::string object, std::string result, std::string num);

	/*
	* 析构物品操作结果事件
	*/
	virtual ~ObjectResultEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置操作类型
	* @action: 操作类型
	*/
	void SetAction(std::string action);

	/*
	* 获取操作类型
	*/
	const std::string& GetAction() const;

	/*
	* 设置 object 类型
	* @object: object 类型
	*/
	void SetObject(std::string object);

	/*
	* 获取 object 类型
	*/
	const std::string& GetObject() const;

	/*
	* 设置是否成功
	* @result: 是否成功
	*/
	void SetResult(std::string result);

	/*
	* 获取是否成功
	*/
	const std::string& GetResult() const;

	/*
	* 设置剩余未操作数量
	* @num: 数量，-1 表示不指定
	*/
	void SetNum(std::string num);

	/*
	* 获取剩余未操作数量
	*/
	const std::string& GetNum() const;

private:
	// 操作类型
	std::string action;

	// object 类型
	std::string object;

	// 是否成功
	std::string result;

	// 剩余未操作数量
	std::string num;
};

// 计时器到时
class TimeUpEvent : public Event {
public:
	/*
	* 构造计时器到时事件
	* @name: 计时器名称
	*/
	TimeUpEvent(std::string name);

	/*
	* 析构计时器到时事件
	*/
	virtual ~TimeUpEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置计时器名称
	* @name: 名称
	*/
	void SetName(std::string name);

	/*
	* 获取计时器名称
	*/
	const std::string& GetName() const;

private:
	// 计时器名称
	std::string name;
};

// NPC抵达
class NpcArriveEvent : public Event {
public:
	/*
	* 构造导航抵达事件
	* @name, address: 抵达者姓名与目标地址
	*/
	NpcArriveEvent(std::string name, std::string address);

	/*
	* 析构导航抵达事件
	*/
	virtual ~NpcArriveEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置抵达者姓名
	* @name: 姓名
	*/
	void SetName(std::string name);

	/*
	* 获取抵达者姓名
	*/
	const std::string& GetName() const;

	/*
	* 设置目标地址
	* @address: 地址
	*/
	void SetAddress(std::string address);

	/*
	* 获取目标地址
	*/
	const std::string& GetAddress() const;

private:
	// 抵达者姓名
	std::string name;

	// 目标地址
	std::string address;
};

// NPC相遇
class NPCMeetEvent : public Event {
public:
	/*
	* 构造NPC相遇事件
	* @npc: NPC名称
	*/
	NPCMeetEvent(std::string npc);

	/*
	* 析构NPC相遇事件
	*/
	virtual ~NPCMeetEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置NPC名称
	* @npc: NPC名称
	*/
	void SetNPC(std::string npc);

	/*
	* 获取NPC名称
	*/
	const std::string& GetNPC() const;

private:
	// NPC名称
	std::string npc;
};

// 市民出生
class CitizenBornEvent : public Event {
public:
	/*
	* 构造市民出生事件
	* @name: 市民姓名
	*/
	CitizenBornEvent(std::string name);

	/*
	* 析构市民出生事件
	*/
	virtual ~CitizenBornEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置市民姓名
	* @name: 姓名
	*/
	void SetName(std::string name);

	/*
	* 获取市民姓名
	*/
	const std::string& GetName() const;

private:
	// 市民姓名
	std::string name;
};

// 市民死亡
class CitizenDeceaseEvent : public Event {
public:
	/*
	* 构造市民死亡事件
	* @name, reason: 市民姓名与死亡原因
	*/
	CitizenDeceaseEvent(std::string name, std::string reason);

	/*
	* 析构市民死亡事件
	*/
	virtual ~CitizenDeceaseEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置市民姓名
	* @name: 姓名
	*/
	void SetName(std::string name);

	/*
	* 获取市民姓名
	*/
	const std::string& GetName() const;

	/*
	* 设置死亡原因
	* @reason: 原因
	*/
	void SetReason(std::string reason);

	/*
	* 获取死亡原因
	*/
	const std::string& GetReason() const;

private:
	// 市民姓名
	std::string name;

	// 死亡原因
	std::string reason;
};

// 受伤
class PlayerInjuredEvent : public Event {
public:
	/*
	* 构造受伤事件
	* @wound: 伤势描述
	*/
	PlayerInjuredEvent(std::string wound);

	/*
	* 析构受伤事件
	*/
	virtual ~PlayerInjuredEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置伤势描述
	* @wound: 伤势
	*/
	void SetWound(std::string wound);

	/*
	* 获取伤势描述
	*/
	const std::string& GetWound() const;

private:
	// 伤势描述
	std::string wound;
};

// 痊愈
class PlayerCuredEvent : public Event {
public:
	/*
	* 构造痊愈事件
	* @wound: 痊愈的伤势
	*/
	PlayerCuredEvent(std::string wound);

	/*
	* 析构痊愈事件
	*/
	virtual ~PlayerCuredEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置痊愈的伤势
	* @wound: 伤势
	*/
	void SetWound(std::string wound);

	/*
	* 获取痊愈的伤势
	*/
	const std::string& GetWound() const;

private:
	// 痊愈的伤势
	std::string wound;
};

// 生病
class PlayerIllEvent : public Event {
public:
	/*
	* 构造生病事件
	* @illness: 病症描述
	*/
	PlayerIllEvent(std::string illness);

	/*
	* 析构生病事件
	*/
	virtual ~PlayerIllEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置病症描述
	* @illness: 病症
	*/
	void SetIllness(std::string illness);

	/*
	* 获取病症描述
	*/
	const std::string& GetIllness() const;

private:
	// 病症描述
	std::string illness;
};

// 康复
class PlayerRecoverEvent : public Event {
public:
	/*
	* 构造康复事件
	* @illness: 康复的病症
	*/
	PlayerRecoverEvent(std::string illness);

	/*
	* 析构康复事件
	*/
	virtual ~PlayerRecoverEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置康复的病症
	* @illness: 病症
	*/
	void SetIllness(std::string illness);

	/*
	* 获取康复的病症
	*/
	const std::string& GetIllness() const;

private:
	// 康复的病症
	std::string illness;
};

// 短暂休息
class PlayerRestEvent : public Event {
public:
	/*
	* 构造短暂休息事件
	* @minute: 休息时长（分钟）
	*/
	PlayerRestEvent(std::string minute);

	/*
	* 析构短暂休息事件
	*/
	virtual ~PlayerRestEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置休息时长
	* @minute: 分钟数
	*/
	void SetMinute(std::string minute);

	/*
	* 获取休息时长
	*/
	const std::string& GetMinute() const;

private:
	// 休息时长（分钟）
	std::string minute;
};

// 睡觉
class PlayerSleepEvent : public Event {
public:
	/*
	* 构造睡觉事件
	* @hour: 睡眠时长（小时）
	*/
	PlayerSleepEvent(std::string hour);

	/*
	* 析构睡觉事件
	*/
	virtual ~PlayerSleepEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置睡眠时长
	* @hour: 小时数
	*/
	void SetHour(std::string hour);

	/*
	* 获取睡眠时长
	*/
	const std::string& GetHour() const;

private:
	// 睡眠时长（小时）
	std::string hour;
};

// 修炼变化
class CultivationChangeEvent : public Event {
public:
	/*
	* 构造修炼变化事件
	* @method, level: 修炼方式与等级
	*/
	CultivationChangeEvent(std::string method, std::string level);

	/*
	* 析构修炼变化事件
	*/
	virtual ~CultivationChangeEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置修炼方式
	* @method: 方式
	*/
	void SetMethod(std::string method);

	/*
	* 获取修炼方式
	*/
	const std::string& GetMethod() const;

	/*
	* 设置等级
	* @level: 等级
	*/
	void SetLevel(std::string level);

	/*
	* 获取等级
	*/
	const std::string& GetLevel() const;

private:
	// 修炼方式
	std::string method;

	// 修炼等级
	std::string level;
};

// 通缉变化
class WantedChangeEvent : public Event {
public:
	/*
	* 构造通缉变化事件
	* @reason, level: 通缉原因与等级
	*/
	WantedChangeEvent(std::string reason, std::string level);

	/*
	* 析构通缉变化事件
	*/
	virtual ~WantedChangeEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置通缉原因
	* @reason: 原因
	*/
	void SetReason(std::string reason);

	/*
	* 获取通缉原因
	*/
	const std::string& GetReason() const;

	/*
	* 设置通缉等级
	* @level: 等级
	*/
	void SetLevel(std::string level);

	/*
	* 获取通缉等级
	*/
	const std::string& GetLevel() const;

private:
	// 通缉原因
	std::string reason;

	// 通缉等级
	std::string level;
};

// 被捕
class PlayerArrestedEvent : public Event {
public:
	/*
	* 构造被捕事件
	* @reason: 被捕原因
	*/
	PlayerArrestedEvent(std::string reason);

	/*
	* 析构被捕事件
	*/
	virtual ~PlayerArrestedEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置被捕原因
	* @reason: 原因
	*/
	void SetReason(std::string reason);

	/*
	* 获取被捕原因
	*/
	const std::string& GetReason() const;

private:
	// 被捕原因
	std::string reason;
};

// 释放
class PlayerReleasedEvent : public Event {
public:
	/*
	* 构造释放事件
	* @reason: 释放原因
	*/
	PlayerReleasedEvent(std::string reason);

	/*
	* 析构释放事件
	*/
	virtual ~PlayerReleasedEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置释放原因
	* @reason: 原因
	*/
	void SetReason(std::string reason);

	/*
	* 获取释放原因
	*/
	const std::string& GetReason() const;

private:
	// 释放原因
	std::string reason;
};

// 天气变化
class WeatherChangeEvent : public Event {
public:
	/*
	* 构造天气变化事件
	* @weather: 天气类型
	*/
	WeatherChangeEvent(std::string weather);

	/*
	* 析构天气变化事件
	*/
	virtual ~WeatherChangeEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置天气类型
	* @weather: 天气
	*/
	void SetWeather(std::string weather);

	/*
	* 获取天气类型
	*/
	const std::string& GetWeather() const;

private:
	// 天气类型
	std::string weather;
};

// 政策变化
class PolicyChangeEvent : public Event {
public:
	/*
	* 构造政策变化事件
	* @policy, status: 政策名称与启用状态
	*/
	PolicyChangeEvent(std::string policy, std::string status);

	/*
	* 析构政策变化事件
	*/
	virtual ~PolicyChangeEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置政策名称
	* @policy: 名称
	*/
	void SetPolicy(std::string policy);

	/*
	* 获取政策名称
	*/
	const std::string& GetPolicy() const;

	/*
	* 设置启用状态
	* @status: 是否启用
	*/
	void SetStatus(std::string status);

	/*
	* 获取启用状态
	*/
	const std::string& GetStatus() const;

private:
	// 政策名称
	std::string policy;

	// 是否启用
	std::string status;
};

// 使用资产
class UseAssetEvent : public Event {
public:
	/*
	* 构造使用资产事件
	* @asset: 资产类型
	*/
	UseAssetEvent(std::string asset);

	/*
	* 析构使用资产事件
	*/
	virtual ~UseAssetEvent();

	/*
	* 事件类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置资产类型
	* @asset: 资产类型
	*/
	void SetAsset(std::string asset);

	/*
	* 获取资产类型
	*/
	const std::string& GetAsset() const;

private:
	// 资产类型
	std::string asset;
};
