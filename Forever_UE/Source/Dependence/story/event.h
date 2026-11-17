#pragma once

#include "../common/utility.h"
#include "../common/error.h"
#include "../common/json.h"
#include "expression.h"

#include <string>
#include <map>


#undef GetMessage


// 事件基类：Match默认只比较GetType()，需要字段级过滤的子类override，见event.md
class Event {
public:
	Event();

	virtual ~Event();

	virtual const std::string& GetType() const = 0;

	virtual bool Match(Event* e, const ScriptContext& context);

	const std::string& GetCondition() const;

	void SetCondition(const std::string& condition);

	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const;

private:
	// 控制条件
	std::string condition;
};

// 游戏开始
class GameStartEvent : public Event {
public:
	GameStartEvent();

	virtual ~GameStartEvent();

	virtual const std::string& GetType() const override;

private:
};

// 全局消息
class GlobalMessageEvent : public Event {
public:
	GlobalMessageEvent(std::string message);

	virtual ~GlobalMessageEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const override;

	void SetMessage(std::string message);

	const std::string& GetMessage() const;

private:
	// 消息内容
	std::string message;
};

// 选项对话
class OptionDialogEvent : public Event {
public:
	OptionDialogEvent(std::string name, std::string option);

	virtual ~OptionDialogEvent();

	virtual const std::string& GetType() const override;

	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const override;

	void SetName(std::string name);

	const std::string& GetName() const;

	void SetOption(std::string option);

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
	SpeakingFinishEvent(std::string label);

	virtual ~SpeakingFinishEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const override;

	void SetLabel(std::string label);

	const std::string& GetLabel() const;

private:
	// 对话标签
	std::string label;
};

// 进入园区
class EnterZoneEvent : public Event {
public:
	EnterZoneEvent(std::string zone);

	virtual ~EnterZoneEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const override;

	void SetZone(std::string zone);

	const std::string& GetZone() const;

private:
	// 园区名称
	std::string zone;
};

// 离开园区
class LeaveZoneEvent : public Event {
public:
	LeaveZoneEvent(std::string zone);

	virtual ~LeaveZoneEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const override;

	void SetZone(std::string zone);

	const std::string& GetZone() const;

private:
	// 园区名称
	std::string zone;
};

// 进入建筑
class EnterBuildingEvent : public Event {
public:
	EnterBuildingEvent(std::string zone, std::string building);

	virtual ~EnterBuildingEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const override;

	void SetZone(std::string zone);

	const std::string& GetZone() const;

	void SetBuilding(std::string building);

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
	LeaveBuildingEvent(std::string zone, std::string building);

	virtual ~LeaveBuildingEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const override;

	void SetZone(std::string zone);

	const std::string& GetZone() const;

	void SetBuilding(std::string building);

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
	EnterRoomEvent(std::string zone, std::string building, std::string room);

	virtual ~EnterRoomEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const override;

	void SetZone(std::string zone);

	const std::string& GetZone() const;

	void SetBuilding(std::string building);

	const std::string& GetBuilding() const;

	void SetRoom(std::string room);

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
	LeaveRoomEvent(std::string zone, std::string building, std::string room);

	virtual ~LeaveRoomEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const override;

	void SetZone(std::string zone);

	const std::string& GetZone() const;

	void SetBuilding(std::string building);

	const std::string& GetBuilding() const;

	void SetRoom(std::string room);

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
	PuzzleResultEvent();

	virtual ~PuzzleResultEvent();

	virtual const std::string& GetType() const override;

	virtual bool Match(Event* e, const ScriptContext& context) override;

	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const override;

	bool SetFromJson(const JsonValue& root);

	const std::map<std::string, ValueType>& GetFields() const;

private:
	// 递归展平
	void Flatten(const std::string& name, const JsonValue& value);

	// 展平后的结果字段
	std::map<std::string, ValueType> fields;
};

// 交易结果
class TransactionResultEvent : public Event {
public:
	TransactionResultEvent(std::string result, std::string name);

	virtual ~TransactionResultEvent();

	virtual const std::string& GetType() const override;

	void SetResult(std::string result);

	const std::string& GetResult() const;

	void SetName(std::string name);

	const std::string& GetName() const;

private:
	// 是否成功
	std::string result;

	// 交易对象姓名，为空代表玩家
	std::string name;
};

// 计时器到时
class TimeUpEvent : public Event {
public:
	TimeUpEvent(std::string name);

	virtual ~TimeUpEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const override;

	void SetName(std::string name);

	const std::string& GetName() const;

private:
	// 计时器名称
	std::string name;
};

// NPC抵达
class NpcArriveEvent : public Event {
public:
	NpcArriveEvent(std::string name, std::string address);

	virtual ~NpcArriveEvent();

	virtual const std::string& GetType() const override;

	virtual bool Match(Event* e, const ScriptContext& context) override;

	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const override;

	void SetName(std::string name);

	const std::string& GetName() const;

	void SetAddress(std::string address);

	const std::string& GetAddress() const;

private:
	// 抵达者姓名
	std::string name;

	// 目标地址
	std::string address;
};

// 员工接到任务(组织的assign_task成功发起后)：只发给该员工的Job脚本。task=任务类别标签，destination=目的房间地址；
// 脚本里的触发条件两个字段都可省略(留空=不限)，运行时用$$local.task/$$local.destination读。
class ExecuteTaskEvent : public Event {
public:
	ExecuteTaskEvent(std::string task, std::string destination);

	virtual ~ExecuteTaskEvent();

	virtual const std::string& GetType() const override;

	virtual bool Match(Event* e, const ScriptContext& context) override;

	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const override;

private:
	std::string task;
	std::string destination;
};

// 载具抵达：车辆Controller导航到房间(vehicle_navigate)后，只发给这辆车自己的script。
class VehicleArriveEvent : public Event {
public:
	VehicleArriveEvent(std::string vehicle, std::string address);

	virtual ~VehicleArriveEvent();

	virtual const std::string& GetType() const override;

	virtual bool Match(Event* e, const ScriptContext& context) override;

	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const override;

	void SetVehicle(std::string vehicle);

	const std::string& GetVehicle() const;

	void SetAddress(std::string address);

	const std::string& GetAddress() const;

private:
	// 抵达的载具名称
	std::string vehicle;

	// 抵达的房间地址
	std::string address;
};

// NPC相遇
class NPCMeetEvent : public Event {
public:
	NPCMeetEvent(std::string npc);

	virtual ~NPCMeetEvent();

	virtual const std::string& GetType() const override;

	void SetNPC(std::string npc);

	const std::string& GetNPC() const;

private:
	// NPC名称
	std::string npc;
};

// 市民出生
class CitizenBornEvent : public Event {
public:
	CitizenBornEvent(std::string name);

	virtual ~CitizenBornEvent();

	virtual const std::string& GetType() const override;

	void SetName(std::string name);

	const std::string& GetName() const;

private:
	// 市民姓名
	std::string name;
};

// 市民死亡
class CitizenDeceaseEvent : public Event {
public:
	CitizenDeceaseEvent(std::string name, std::string reason);

	CitizenDeceaseEvent(std::string name, std::string reason, std::string killer, std::string weapon,
		std::string part, double damage, bool visible, std::string location);

	virtual ~CitizenDeceaseEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const override;

	void SetName(std::string name);

	const std::string& GetName() const;

	void SetReason(std::string reason);

	const std::string& GetReason() const;

	void SetKiller(std::string killer);
	const std::string& GetKiller() const;
	void SetWeapon(std::string weapon);
	const std::string& GetWeapon() const;
	void SetPart(std::string part);
	const std::string& GetPart() const;
	void SetDamage(double damage);
	double GetDamage() const;
	void SetVisible(bool visible);
	bool GetVisible() const;
	void SetLocation(std::string location);
	const std::string& GetLocation() const;

private:
	// 市民姓名
	std::string name;

	// 死亡原因
	std::string reason;

	// 击杀信息(武器击杀才有，其它死因为空/0)。killer/weapon/part作为触发条件时是表达式，
	// 其余三个只在运行时事件里有意义。
	std::string killer;
	std::string weapon;
	std::string part;
	double damage = 0.0;
	bool visible = false;
	std::string location;
};

// 受伤
class PlayerInjuredEvent : public Event {
public:
	PlayerInjuredEvent(std::string wound);

	virtual ~PlayerInjuredEvent();

	virtual const std::string& GetType() const override;

	void SetWound(std::string wound);

	const std::string& GetWound() const;

private:
	// 伤势描述
	std::string wound;
};

// 痊愈
class PlayerCuredEvent : public Event {
public:
	PlayerCuredEvent(std::string wound);

	virtual ~PlayerCuredEvent();

	virtual const std::string& GetType() const override;

	void SetWound(std::string wound);

	const std::string& GetWound() const;

private:
	// 痊愈的伤势
	std::string wound;
};

// 生病
class PlayerIllEvent : public Event {
public:
	PlayerIllEvent(std::string illness);

	virtual ~PlayerIllEvent();

	virtual const std::string& GetType() const override;

	void SetIllness(std::string illness);

	const std::string& GetIllness() const;

private:
	// 病症描述
	std::string illness;
};

// 康复
class PlayerRecoverEvent : public Event {
public:
	PlayerRecoverEvent(std::string illness);

	virtual ~PlayerRecoverEvent();

	virtual const std::string& GetType() const override;

	void SetIllness(std::string illness);

	const std::string& GetIllness() const;

private:
	// 康复的病症
	std::string illness;
};

// 短暂休息
class PlayerRestEvent : public Event {
public:
	PlayerRestEvent(std::string minute);

	virtual ~PlayerRestEvent();

	virtual const std::string& GetType() const override;

	void SetMinute(std::string minute);

	const std::string& GetMinute() const;

private:
	// 休息时长（分钟）
	std::string minute;
};

// 睡觉
class PlayerSleepEvent : public Event {
public:
	PlayerSleepEvent(std::string hour);

	virtual ~PlayerSleepEvent();

	virtual const std::string& GetType() const override;

	void SetHour(std::string hour);

	const std::string& GetHour() const;

private:
	// 睡眠时长（小时）
	std::string hour;
};

// 通缉变化
class WantedChangeEvent : public Event {
public:
	WantedChangeEvent(std::string reason, std::string level);

	virtual ~WantedChangeEvent();

	virtual const std::string& GetType() const override;

	void SetReason(std::string reason);

	const std::string& GetReason() const;

	void SetLevel(std::string level);

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
	PlayerArrestedEvent(std::string reason);

	virtual ~PlayerArrestedEvent();

	virtual const std::string& GetType() const override;

	void SetReason(std::string reason);

	const std::string& GetReason() const;

private:
	// 被捕原因
	std::string reason;
};

// 释放
class PlayerReleasedEvent : public Event {
public:
	PlayerReleasedEvent(std::string reason);

	virtual ~PlayerReleasedEvent();

	virtual const std::string& GetType() const override;

	void SetReason(std::string reason);

	const std::string& GetReason() const;

private:
	// 释放原因
	std::string reason;
};

// 天气变化
class WeatherChangeEvent : public Event {
public:
	WeatherChangeEvent(std::string weather);

	virtual ~WeatherChangeEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const override;

	void SetWeather(std::string weather);

	const std::string& GetWeather() const;

private:
	// 天气类型
	std::string weather;
};

// 使用资产
class UseAssetEvent : public Event {
public:
	UseAssetEvent(std::string asset);

	virtual ~UseAssetEvent();

	virtual const std::string& GetType() const override;

	void SetAsset(std::string asset);

	const std::string& GetAsset() const;

private:
	// 资产类型
	std::string asset;
};

// 载具(公交)进站并停稳。由UE层在Core检测到停靠边沿后发给这辆载具自己的script(不广播)，
// 让script控制上下车选项：见Resource/Story/vehicle_bus.script。
class EnterStopEvent : public Event {
public:
	EnterStopEvent(std::string stop);

	virtual ~EnterStopEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const override;

	void SetStop(std::string stop);
	const std::string& GetStop() const;

private:
	std::string stop;
};

// 载具(公交)启动离站，字段同EnterStopEvent。
class LeaveStopEvent : public Event {
public:
	LeaveStopEvent(std::string stop);
	virtual ~LeaveStopEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const override;

	void SetStop(std::string stop);
	const std::string& GetStop() const;

private:
	std::string stop;
};

// 玩家进入载具(驾驶或搭乘都算)。由UE层只发给被进入的那辆载具自己的script。注意：和同名的
// EnterVehicleChange(让玩家去开某辆车)是两套东西——这个是"发生了"的通知，那个是"去做"的指令。
class EnterVehicleEvent : public Event {
public:
	EnterVehicleEvent(std::string vehicle, std::string mode);
	virtual ~EnterVehicleEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const override;

	void SetVehicle(std::string vehicle);
	const std::string& GetVehicle() const;
	void SetMode(std::string mode);
	const std::string& GetMode() const;

private:
	std::string vehicle;
	std::string mode;
};

// 玩家离开载具(驾驶或搭乘都算)，字段同EnterVehicleEvent。
class LeaveVehicleEvent : public Event {
public:
	LeaveVehicleEvent(std::string vehicle, std::string mode);
	virtual ~LeaveVehicleEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const override;

	void SetVehicle(std::string vehicle);
	const std::string& GetVehicle() const;
	void SetMode(std::string mode);
	const std::string& GetMode() const;

private:
	std::string vehicle;
	std::string mode;
};

// 获得资产：捡起(pick_asset/背包界面拖进身上)或凭空获得(grant_asset)之后通知脚本成功与否。执行Change的
// 情况发给执行这个Change的脚本，背包界面里的移动发给这件资产自己的脚本；根资产是容器时，容器和递归进去
// 的每一件资产各发一次。
class GetAssetEvent : public Event {
public:
	GetAssetEvent(std::string type, std::string result);
	virtual ~GetAssetEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const override;

	void SetAssetType(std::string type);
	const std::string& GetAssetType() const;
	void SetResult(std::string result);
	const std::string& GetResult() const;

private:
	std::string type;
	std::string result;
};

// 失去资产：drop_asset/deduct_asset(成功或失败：身上没有指定物品或数量不足)，以及背包界面里把东西
// 放到地面/房间。分发规则同GetAssetEvent。
class LoseAssetEvent : public Event {
public:
	LoseAssetEvent(std::string type, std::string result);
	virtual ~LoseAssetEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const override;

	void SetAssetType(std::string type);
	const std::string& GetAssetType() const;
	void SetResult(std::string result);
	const std::string& GetResult() const;

private:
	std::string type;
	std::string result;
};

// 投掷：玩家把一件可投掷资产(手雷/烟雾弹等，AssetMod::throwable)扔出去之后，只发给被扔出的这件资产自己的脚本
// (没有脚本就不发)。脚本里读$$local.type(资产mod id)。手雷脚本在这里create_timer计时爆炸。
class ThrowAssetEvent : public Event {
public:
	// thrower：投掷者(玩家当前操控的市民)姓名，只作为local变量给脚本用，不参与Match。
	ThrowAssetEvent(std::string type, std::string thrower = std::string());
	virtual ~ThrowAssetEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const override;

	void SetAssetType(std::string type);
	const std::string& GetAssetType() const;
	void SetThrower(std::string thrower);
	const std::string& GetThrower() const;

private:
	std::string type;
	std::string thrower;
};

// 开火：玩家用一把武器(WeaponMod)开了一枪(霰弹每颗弹丸各一次)，通过瞄准/冷却/弹药闸门之后由UE层发出，
// 只发给这把武器自己的脚本(Weapon持有，没有脚本则不发)。触发条件里weapon_type可空=不限，留空匹配所有武器。
// 脚本里读$$local.xxx：
//   weapon(武器mod id)、shooter(开火市民姓名)、sufferer(被射线打中的市民姓名，没有则空)、
//   part(命中部位head/body/legs，没有则空)、distance(浮点，摄像机到hit的距离)；
//   vec3：asset(武器模型位置)、fire(枪口)、hit(射线交点，没打中是射程终点)、direction(出射单位方向)、
//   normal(命中面法线，没打中朝上)。每个vec3有字符串形式$$local.hit="(x, y, z)"和浮点分量$$local.hit.x/.y/.z。
class WeaponFireEvent : public Event {
public:
	WeaponFireEvent(std::string weapon);
	virtual ~WeaponFireEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const override;

	void SetWeapon(std::string weapon);
	const std::string& GetWeapon() const;

	// 设置一个普通local字段(字符串/浮点)。
	void SetField(const std::string& name, ValueType value);
	// 设置一个vec3 local：同时写入name="(x, y, z)"和name.x/name.y/name.z三个浮点。
	void SetVec3(const std::string& name, double x, double y, double z);

private:
	std::string weapon;
	std::map<std::string, ValueType> fields;
};

// 弹射物命中：mod蓝图里的弹射物Actor(UForeverProjectileBridgeComponent)撞到东西后报告，发给发射它的武器脚本。
// 脚本里读$$local.hit/normal(vec3，规则同WeaponFireEvent)、$$local.sufferer(被击中的市民姓名，可空)。
class ProjectileHitEvent : public Event {
public:
	ProjectileHitEvent();
	virtual ~ProjectileHitEvent();

	virtual const std::string& GetType() const override;
	virtual bool Match(Event* e, const ScriptContext& context) override;
	virtual std::pair<bool, ValueType> GetLocalValue(const std::string& name) const override;

	void SetField(const std::string& name, ValueType value);
	void SetVec3(const std::string& name, double x, double y, double z);

private:
	std::map<std::string, ValueType> fields;
};
