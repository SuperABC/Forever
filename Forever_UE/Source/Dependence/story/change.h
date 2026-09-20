#pragma once

#include "../common/utility.h"
#include "../common/error.h"
#include "expression.h"

#include <string>


#undef GetMessage
#undef GetObject


// 变化基类。老工程里Change本来就是纯数据类（不含执行逻辑，执行逻辑在调用方按GetType()/
// dynamic_cast分派），这次42个子类原样保留这个设计——只有字段+getter/setter，没有Apply之类
// 的虚方法。除了SetValueChange（已实现，见Core/story/story.cpp的ApplyChange分派），其余
// 41个子类这次都还没有对应的执行分支，等被点名实现时再补。
class Change {
public:
	/*
	* 构造变化
	*/
	Change();

	/*
	* 析构变化
	*/
	virtual ~Change();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const = 0;

	/*
	* 获取控制条件
	*/
	const Expression& GetCondition() const;

	/*
	* 设置控制条件
	* @condition: 条件表达式
	*/
	void SetCondition(const Expression& condition);

private:
	// 控制条件
	Expression condition;

};

// 范围循环
class ForRangeChange : public Change {
public:
	/*
	* 构造范围循环变化
	* @var: 循环变量名
	* @from, to, step: 起始、终止与步长表达式
	* @changes: 循环体变化列表
	*/
	ForRangeChange(std::string var, Expression from, Expression to, Expression step,
		std::vector<const Change*> changes);

	/*
	* 析构范围循环变化
	*/
	virtual ~ForRangeChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 获取循环变量名
	*/
	std::string GetVar() const;

	/*
	* 获取起始表达式
	*/
	const Expression& GetFrom() const;

	/*
	* 获取终止表达式
	*/
	const Expression& GetTo() const;

	/*
	* 获取步长表达式
	*/
	const Expression& GetStep() const;

	/*
	* 获取循环体变化列表
	*/
	const std::vector<const Change*>& GetChanges() const;

private:
	// 循环变量名
	std::string var;

	// 起始表达式
	Expression from;

	// 终止表达式
	Expression to;

	// 步长表达式
	Expression step;

	// 循环体变化列表（不持有所有权，见Milestone::changes的OBJECT_HOLDER约定）
	std::vector<const Change*> changes;

};

// 占位符
class PlaceHolderChange : public Change {
public:
	/*
	* 默认构造占位符变化
	*/
	PlaceHolderChange();

	/*
	* 构造占位符变化
	* @label: 标签
	*/
	PlaceHolderChange(Expression label);

	/*
	* 析构占位符变化
	*/
	virtual ~PlaceHolderChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置标签
	* @label: 标签
	*/
	void SetLabel(Expression label);

	/*
	* 获取标签
	*/
	const Expression& GetLabel() const;

private:
	// 标签
	Expression label;

};

// 全局广播
class GlobalMessageChange : public Change {
public:
	/*
	* 默认构造全局广播变化
	*/
	GlobalMessageChange();

	/*
	* 构造全局广播变化
	* @message: 广播消息内容
	*/
	GlobalMessageChange(Expression message);

	/*
	* 析构全局广播变化
	*/
	virtual ~GlobalMessageChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置消息内容
	* @message: 消息内容
	*/
	void SetMessage(Expression message);

	/*
	* 获取消息内容
	*/
	const Expression& GetMessage() const;

private:
	// 消息内容
	Expression message;

};

// 游戏结束
class GameEndChange : public Change {
public:
	/*
	* 构造游戏结束变化
	*/
	GameEndChange();

	/*
	* 析构游戏结束变化
	*/
	virtual ~GameEndChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

private:
};

// 变量赋值（已实现，见Core/story/story.cpp的ApplyChange分派）
class SetValueChange : public Change {
public:
	/*
	* 默认构造变量赋值变化
	*/
	SetValueChange();

	/*
	* 构造变量赋值变化
	* @variable, value: 变量名与值表达式
	*/
	SetValueChange(std::string variable, Expression value);

	/*
	* 析构变量赋值变化
	*/
	virtual ~SetValueChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置变量名
	* @variable: 变量名
	*/
	void SetVariable(std::string variable);

	/*
	* 获取变量名
	*/
	std::string GetVariable() const;

	/*
	* 设置值表达式
	* @value: 值表达式
	*/
	void SetValue(Expression value);

	/*
	* 获取值表达式
	*/
	const Expression& GetValue() const;

private:
	// 变量名
	std::string variable;

	// 值表达式
	Expression value;

};

// 全局设置修改
class GlobalSettingChange : public Change {
public:
	/*
	* 默认构造全局设置修改变化
	*/
	GlobalSettingChange();

	/*
	* 构造全局设置修改变化
	* @setting, value: 设置名与值表达式
	*/
	GlobalSettingChange(std::string setting, Expression value);

	/*
	* 析构全局设置修改变化
	*/
	virtual ~GlobalSettingChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置设置名
	* @setting: 设置名
	*/
	void SetSetting(std::string setting);

	/*
	* 获取设置名
	*/
	std::string GetSetting() const;

	/*
	* 设置值表达式
	* @value: 值表达式
	*/
	void SetValue(Expression value);

	/*
	* 获取值表达式
	*/
	const Expression& GetValue() const;

private:
	// 设置名
	std::string setting;

	// 值表达式
	Expression value;

};

// 移除变量
class RemoveValueChange : public Change {
public:
	/*
	* 默认构造移除变量变化
	*/
	RemoveValueChange();

	/*
	* 构造移除变量变化
	* @variable: 变量名
	*/
	RemoveValueChange(std::string variable);

	/*
	* 析构移除变量变化
	*/
	virtual ~RemoveValueChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置变量名
	* @variable: 变量名
	*/
	void SetVariable(std::string variable);

	/*
	* 获取变量名
	*/
	std::string GetVariable() const;

private:
	// 变量名
	std::string variable;

};

// 停用里程碑
class DeactivateMilestoneChange : public Change {
public:
	/*
	* 默认构造停用里程碑变化
	*/
	DeactivateMilestoneChange();

	/*
	* 构造停用里程碑变化
	* @milestone: 里程碑名称
	*/
	DeactivateMilestoneChange(std::string milestone);

	/*
	* 析构停用里程碑变化
	*/
	virtual ~DeactivateMilestoneChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置里程碑名称
	* @milestone: 名称
	*/
	void SetMilestone(std::string milestone);

	/*
	* 获取里程碑名称
	*/
	std::string GetMilestone() const;

private:
	// 里程碑名称
	std::string milestone;

};

// 添加选项
class AddOptionChange : public Change {
public:
	/*
	* 默认构造添加选项变化
	*/
	AddOptionChange();

	/*
	* 构造添加选项变化
	* @name, option: 目标名称与选项文本
	*/
	AddOptionChange(Expression name, Expression option);

	/*
	* 析构添加选项变化
	*/
	virtual ~AddOptionChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置目标名称
	* @name: 名称
	*/
	void SetName(Expression name);

	/*
	* 获取目标名称
	*/
	const Expression& GetName() const;

	/*
	* 设置选项文本
	* @option: 选项文本
	*/
	void SetOption(Expression option);

	/*
	* 获取选项文本
	*/
	const Expression& GetOption() const;

private:
	// 目标名称
	Expression name;

	// 选项文本
	Expression option;

};

// 移除选项
class RemoveOptionChange : public Change {
public:
	/*
	* 默认构造移除选项变化
	*/
	RemoveOptionChange();

	/*
	* 构造移除选项变化
	* @name, option: 目标名称与选项文本
	*/
	RemoveOptionChange(Expression name, Expression option);

	/*
	* 析构移除选项变化
	*/
	virtual ~RemoveOptionChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置目标名称
	* @name: 名称
	*/
	void SetName(Expression name);

	/*
	* 获取目标名称
	*/
	const Expression& GetName() const;

	/*
	* 设置选项文本
	* @option: 选项文本
	*/
	void SetOption(Expression option);

	/*
	* 获取选项文本
	*/
	const Expression& GetOption() const;

private:
	// 目标名称
	Expression name;

	// 选项文本
	Expression option;

};

// 添加全局选项
class AddGlobalChange : public Change {
public:
	/*
	* 默认构造添加全局选项变化
	*/
	AddGlobalChange();

	/*
	* 构造添加全局选项变化
	* @option: 选项文本
	*/
	AddGlobalChange(Expression option);

	/*
	* 析构添加全局选项变化
	*/
	virtual ~AddGlobalChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置选项文本
	* @option: 选项文本
	*/
	void SetOption(Expression option);

	/*
	* 获取选项文本
	*/
	const Expression& GetOption() const;

private:
	// 选项文本
	Expression option;

};

// 移除全局选项
class RemoveGlobalChange : public Change {
public:
	/*
	* 默认构造移除全局选项变化
	*/
	RemoveGlobalChange();

	/*
	* 构造移除全局选项变化
	* @option: 选项文本
	*/
	RemoveGlobalChange(Expression option);

	/*
	* 析构移除全局选项变化
	*/
	virtual ~RemoveGlobalChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置选项文本
	* @option: 选项文本
	*/
	void SetOption(Expression option);

	/*
	* 获取选项文本
	*/
	const Expression& GetOption() const;

private:
	// 选项文本
	Expression option;

};

// 生成NPC
class SpawnNpcChange : public Change {
public:
	/*
	* 默认构造生成NPC变化
	*/
	SpawnNpcChange();

	/*
	* 构造生成NPC变化
	* @avatar, name, gender, birthday: 形象、姓名、性别与生日
	* @height, weight: 身高与体重
	* @nick, deposit, phone, home: 昵称、存款、手机号与住所
	* @jobs, scheduler: 职业列表与调度类型
	*/
	SpawnNpcChange(Expression avatar, Expression name, Expression gender, Expression birthday, Expression height, Expression weight,
		Expression nick, Expression deposit, Expression phone, Expression home, std::vector<Expression> jobs, Expression scheduler);

	/*
	* 析构生成NPC变化
	*/
	virtual ~SpawnNpcChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置形象标识
	* @avatar: 形象
	*/
	void SetAvatar(Expression avatar);

	/*
	* 获取形象标识
	*/
	const Expression& GetAvatar() const;

	/*
	* 设置姓名
	* @name: 姓名
	*/
	void SetName(Expression name);

	/*
	* 获取姓名
	*/
	const Expression& GetName() const;

	/*
	* 设置性别
	* @gender: 性别
	*/
	void SetGender(Expression gender);

	/*
	* 获取性别
	*/
	const Expression& GetGender() const;

	/*
	* 设置生日
	* @birthday: 生日
	*/
	void SetBirthday(Expression birthday);

	/*
	* 获取生日
	*/
	const Expression& GetBirthday() const;

	/*
	* 设置身高
	* @height: 身高
	*/
	void SetHeight(Expression height);

	/*
	* 获取身高
	*/
	const Expression& GetHeight() const;

	/*
	* 设置体重
	* @weight: 体重
	*/
	void SetWeight(Expression weight);

	/*
	* 获取体重
	*/
	const Expression& GetWeight() const;

	/*
	* 设置昵称
	* @nick: 昵称
	*/
	void SetNick(Expression nick);

	/*
	* 获取昵称
	*/
	const Expression& GetNick() const;

	/*
	* 设置存款金额
	* @deposit: 存款
	*/
	void SetDeposit(Expression deposit);

	/*
	* 获取存款金额
	*/
	const Expression& GetDeposit() const;

	/*
	* 设置手机号
	* @phone: 手机号
	*/
	void SetPhone(Expression phone);

	/*
	* 获取手机号
	*/
	const Expression& GetPhone() const;

	/*
	* 设置住所
	* @home: 住所名称
	*/
	void SetHome(Expression home);

	/*
	* 获取住所
	*/
	const Expression& GetHome() const;

	/*
	* 设置职业列表
	* @jobs: 职业类型标识列表
	*/
	void SetJobs(std::vector<Expression> jobs);

	/*
	* 获取职业列表
	*/
	const std::vector<Expression>& GetJobs() const;

	/*
	* 设置调度类型
	* @scheduler: 调度类型标识
	*/
	void SetScheduler(Expression scheduler);

	/*
	* 获取调度类型
	*/
	const Expression& GetScheduler() const;

private:
	// 形象标识
	Expression avatar;

	// 姓名
	Expression name;

	// 性别
	Expression gender;

	// 生日
	Expression birthday;

	// 身高
	Expression height;

	// 体重
	Expression weight;

	// 昵称
	Expression nick;

	// 存款金额
	Expression deposit;

	// 手机号
	Expression phone;

	// 住所
	Expression home;

	// 职业类型标识列表
	std::vector<Expression> jobs;

	// 调度类型标识
	Expression scheduler;

};

// 移除NPC
class RemoveNpcChange : public Change {
public:
	/*
	* 默认构造移除NPC变化
	*/
	RemoveNpcChange();

	/*
	* 构造移除NPC变化
	* @name: NPC姓名
	*/
	RemoveNpcChange(Expression name);

	/*
	* 析构移除NPC变化
	*/
	virtual ~RemoveNpcChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置NPC姓名
	* @name: 姓名
	*/
	void SetName(Expression name);

	/*
	* 获取NPC姓名
	*/
	const Expression& GetName() const;

private:
	// NPC姓名
	Expression name;

};

// 瞬移市民
class TeleportCitizenChange : public Change {
public:
	/*
	* 默认构造瞬移市民变化
	*/
	TeleportCitizenChange();

	/*
	* 构造瞬移市民变化
	* @name, destination: 市民姓名与目标房间名称
	*/
	TeleportCitizenChange(Expression name, Expression destination);

	/*
	* 析构瞬移市民变化
	*/
	virtual ~TeleportCitizenChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置市民姓名
	* @name: 姓名
	*/
	void SetName(Expression name);

	/*
	* 获取市民姓名
	*/
	const Expression& GetName() const;

	/*
	* 设置目标房间名称
	* @destination: 名称
	*/
	void SetDestination(Expression destination);

	/*
	* 获取目标房间名称
	*/
	const Expression& GetDestination() const;

private:
	// 市民姓名
	Expression name;

	// 目标房间名称
	Expression destination;

};

// NPC自动导航
class NPCNavigateChange : public Change {
public:
	/*
	* 默认构造NPC自动导航变化
	*/
	NPCNavigateChange();

	/*
	* 构造NPC自动导航变化
	* @name, destination: NPC姓名与目标位置名称
	*/
	NPCNavigateChange(Expression name, Expression destination);

	/*
	* 析构NPC自动导航变化
	*/
	virtual ~NPCNavigateChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置NPC姓名
	* @name: 姓名
	*/
	void SetName(Expression name);

	/*
	* 获取NPC姓名
	*/
	const Expression& GetName() const;

	/*
	* 设置目标位置名称
	* @destination: 名称
	*/
	void SetDestination(Expression destination);

	/*
	* 获取目标位置名称
	*/
	const Expression& GetDestination() const;

private:
	// NPC姓名
	Expression name;

	// 目标位置名称
	Expression destination;

};

// 瞬移角色
class TeleportPlayerChange : public Change {
public:
	/*
	* 默认构造瞬移角色变化
	*/
	TeleportPlayerChange();

	/*
	* 构造瞬移角色变化
	* @destination: 目标房间名称
	*/
	TeleportPlayerChange(Expression destination);

	/*
	* 析构瞬移角色变化
	*/
	virtual ~TeleportPlayerChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置目标房间名称
	* @destination: 名称
	*/
	void SetDestination(Expression destination);

	/*
	* 获取目标房间名称
	*/
	const Expression& GetDestination() const;

private:
	// 目标房间名称
	Expression destination;

};

// 打开商店
class OpenShopChange : public Change {
public:
	/*
	* 默认构造打开商店变化
	*/
	OpenShopChange();

	/*
	* 构造打开商店变化
	* @saler: 售货员姓名
	*/
	OpenShopChange(Expression saler);

	/*
	* 析构打开商店变化
	*/
	virtual ~OpenShopChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置售货员姓名
	* @saler: 姓名
	*/
	void SetSaler(Expression saler);

	/*
	* 获取售货员姓名
	*/
	const Expression& GetSaler() const;

private:
	// 售货员姓名
	Expression saler;

};

// 启动小游戏
class StartPuzzleChange : public Change {
public:
	/*
	* 默认构造启动小游戏变化
	*/
	StartPuzzleChange();

	/*
	* 构造启动小游戏变化
	* @puzzle: 小游戏类型标识
	*/
	StartPuzzleChange(Expression puzzle);

	/*
	* 析构启动小游戏变化
	*/
	virtual ~StartPuzzleChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置小游戏类型标识
	* @puzzle: 标识
	*/
	void SetPuzzle(Expression puzzle);

	/*
	* 获取小游戏类型标识
	*/
	const Expression& GetPuzzle() const;

private:
	// 小游戏类型标识
	Expression puzzle;

};

// 进入载具
class EnterVehicleChange : public Change {
public:
	/*
	* 默认构造进入载具变化
	*/
	EnterVehicleChange();

	/*
	* 构造进入载具变化
	* @vehicle: 载具名称
	*/
	EnterVehicleChange(Expression vehicle);

	/*
	* 析构进入载具变化
	*/
	virtual ~EnterVehicleChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置载具名称
	* @vehicle: 名称
	*/
	void SetVehicle(Expression vehicle);

	/*
	* 获取载具名称
	*/
	const Expression& GetVehicle() const;

private:
	// 载具名称
	Expression vehicle;

};

// 离开载具
class LeaveVehicleChange : public Change {
public:
	/*
	* 默认构造离开载具变化
	*/
	LeaveVehicleChange();

	/*
	* 构造离开载具变化
	* @vehicle: 载具名称
	*/
	LeaveVehicleChange(Expression vehicle);

	/*
	* 析构离开载具变化
	*/
	virtual ~LeaveVehicleChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置载具名称
	* @vehicle: 名称
	*/
	void SetVehicle(Expression vehicle);

	/*
	* 获取载具名称
	*/
	const Expression& GetVehicle() const;

private:
	// 载具名称
	Expression vehicle;

};

// 创建计时器
class CreateTimerChange : public Change {
public:
	/*
	* 默认构造创建计时器变化
	*/
	CreateTimerChange();

	/*
	* 构造创建计时器变化
	* @name, time: 计时器名称与目标时刻
	* @category, label: 所属脚本类型与实体名称
	*/
	CreateTimerChange(Expression name, Expression time, Expression category, Expression label);

	/*
	* 析构创建计时器变化
	*/
	virtual ~CreateTimerChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置计时器名称
	* @name: 名称
	*/
	void SetName(Expression name);

	/*
	* 获取计时器名称
	*/
	const Expression& GetName() const;

	/*
	* 设置目标时刻
	* @time: 时刻字符串
	*/
	void SetTime(Expression time);

	/*
	* 获取目标时刻
	*/
	const Expression& GetTime() const;

	/*
	* 设置所属脚本类型
	* @category: 类型标识
	*/
	void SetCategory(Expression category);

	/*
	* 获取所属脚本类型
	*/
	const Expression& GetCategory() const;

	/*
	* 设置所属实体名称
	* @label: 名称
	*/
	void SetLabel(Expression label);

	/*
	* 获取所属实体名称
	*/
	const Expression& GetLabel() const;

private:
	// 计时器名称
	Expression name;

	// 目标时刻
	Expression time;

	// 所属脚本类型
	Expression category;

	// 所属实体名称
	Expression label;

};

// 移除计时器
class RemoveTimerChange : public Change {
public:
	/*
	* 默认构造移除计时器变化
	*/
	RemoveTimerChange();

	/*
	* 构造移除计时器变化
	* @name: 计时器名称
	*/
	RemoveTimerChange(Expression name);

	/*
	* 析构移除计时器变化
	*/
	virtual ~RemoveTimerChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置计时器名称
	* @name: 名称
	*/
	void SetName(Expression name);

	/*
	* 获取计时器名称
	*/
	const Expression& GetName() const;

private:
	// 计时器名称
	Expression name;

};

// 启动电梯
class LaunchElevatorChange : public Change {
public:
	/*
	* 默认构造启动电梯变化
	*/
	LaunchElevatorChange();

	/*
	* 构造启动电梯变化
	* @building, elevator, command: 建筑名称、电梯名称与指令
	*/
	LaunchElevatorChange(Expression building, Expression elevator, Expression command);

	/*
	* 析构启动电梯变化
	*/
	virtual ~LaunchElevatorChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置建筑名称
	* @building: 名称
	*/
	void SetBuilding(Expression building);

	/*
	* 获取建筑名称
	*/
	const Expression& GetBuilding() const;

	/*
	* 设置电梯名称
	* @elevator: 名称
	*/
	void SetElevator(Expression elevator);

	/*
	* 获取电梯名称
	*/
	const Expression& GetElevator() const;

	/*
	* 设置指令
	* @command: 指令字符串
	*/
	void SetCommand(Expression command);

	/*
	* 获取指令
	*/
	const Expression& GetCommand() const;

private:
	// 建筑名称
	Expression building;

	// 电梯名称
	Expression elevator;

	// 指令
	Expression command;

};

// 播放视频
class PlayVideoChange : public Change {
public:
	/*
	* 默认构造播放视频变化
	*/
	PlayVideoChange();

	/*
	* 构造播放视频变化
	* @path: 视频文件路径
	*/
	PlayVideoChange(Expression path);

	/*
	* 析构播放视频变化
	*/
	virtual ~PlayVideoChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置视频文件路径
	* @path: 路径
	*/
	void SetPath(Expression path);

	/*
	* 获取视频文件路径
	*/
	const Expression& GetPath() const;

private:
	// 视频文件路径
	Expression path;

};

// 播放背景音乐
class PlayBgmChange : public Change {
public:
	/*
	* 默认构造播放背景音乐变化
	*/
	PlayBgmChange();

	/*
	* 构造播放背景音乐变化
	* @bgm, loop: 背景音乐标识与是否循环播放
	*/
	PlayBgmChange(Expression bgm, Expression loop);

	/*
	* 析构播放背景音乐变化
	*/
	virtual ~PlayBgmChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置背景音乐标识
	* @bgm: 标识
	*/
	void SetBgm(Expression bgm);

	/*
	* 获取背景音乐标识
	*/
	const Expression& GetBgm() const;

	/*
	* 设置是否循环播放
	* @loop: 是否循环
	*/
	void SetLoop(Expression loop);

	/*
	* 获取是否循环播放
	*/
	const Expression& GetLoop() const;

private:
	// 背景音乐标识
	Expression bgm;

	// 是否循环播放
	Expression loop;

};

// 停止背景音乐
class StopBgmChange : public Change {
public:
	/*
	* 构造停止背景音乐变化
	*/
	StopBgmChange();

	/*
	* 析构停止背景音乐变化
	*/
	virtual ~StopBgmChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

private:
};

// 存款收支
class BankTransactionChange : public Change {
public:
	/*
	* 默认构造存款收支变化
	*/
	BankTransactionChange();

	/*
	* 构造存款收支变化
	* @name: 收款人姓名（空字符串表示玩家）
	* @amount: 金额（正数存入，负数取出）
	*/
	BankTransactionChange(Expression name, Expression amount);

	/*
	* 析构存款收支变化
	*/
	virtual ~BankTransactionChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置收款人姓名
	* @name: 姓名（空字符串表示玩家）
	*/
	void SetName(Expression name);

	/*
	* 获取收款人姓名
	*/
	const Expression& GetName() const;

	/*
	* 设置金额
	* @amount: 金额
	*/
	void SetAmount(Expression amount);

	/*
	* 获取金额
	*/
	const Expression& GetAmount() const;

private:
	// 收款人姓名（空字符串表示玩家）
	Expression name;

	// 金额
	Expression amount;

};

// 给予房产
class GiveEstateChange : public Change {
public:
	/*
	* 默认构造给予房产变化
	*/
	GiveEstateChange();

	/*
	* 构造给予房产变化
	* @estate: 房产名称
	* @name: 接收者姓名（空字符串表示玩家）
	* @force: 是否强制覆盖已有归属
	*/
	GiveEstateChange(Expression estate, Expression name, Expression force);

	/*
	* 析构给予房产变化
	*/
	virtual ~GiveEstateChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置房产名称
	* @estate: 名称
	*/
	void SetEstate(Expression estate);

	/*
	* 获取房产名称
	*/
	const Expression& GetEstate() const;

	/*
	* 设置接收者姓名
	* @name: 姓名（空字符串表示玩家）
	*/
	void SetName(Expression name);

	/*
	* 获取接收者姓名
	*/
	const Expression& GetName() const;

	/*
	* 设置是否强制转移
	* @force: 是否强制
	*/
	void SetForce(Expression force);

	/*
	* 获取是否强制转移
	*/
	const Expression& GetForce() const;

private:
	// 房产名称
	Expression estate;

	// 接收者姓名（空字符串表示玩家）
	Expression name;

	// 是否强制覆盖已有归属
	Expression force;

};

// 移除房产
class RemoveEstateChange : public Change {
public:
	/*
	* 默认构造移除房产变化
	*/
	RemoveEstateChange();

	/*
	* 构造移除房产变化
	* @estate: 房产名称
	* @name: 当前所有者姓名（空字符串表示玩家）
	*/
	RemoveEstateChange(Expression estate, Expression name);

	/*
	* 析构移除房产变化
	*/
	virtual ~RemoveEstateChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置房产名称
	* @estate: 名称
	*/
	void SetEstate(Expression estate);

	/*
	* 获取房产名称
	*/
	const Expression& GetEstate() const;

	/*
	* 设置当前所有者姓名
	* @name: 姓名（空字符串表示玩家）
	*/
	void SetName(Expression name);

	/*
	* 获取当前所有者姓名
	*/
	const Expression& GetName() const;

private:
	// 房产名称
	Expression estate;

	// 当前所有者姓名（空字符串表示玩家）
	Expression name;

};

// 给予载具
class GiveVehicleChange : public Change {
public:
	/*
	* 默认构造给予载具变化
	*/
	GiveVehicleChange();

	/*
	* 构造给予载具变化
	* @vehicle: 载具名称
	* @name: 接收者姓名（空字符串表示玩家）
	* @force: 是否强制覆盖已有归属
	*/
	GiveVehicleChange(Expression vehicle, Expression name, Expression force);

	/*
	* 析构给予载具变化
	*/
	virtual ~GiveVehicleChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置载具名称
	* @vehicle: 名称
	*/
	void SetVehicle(Expression vehicle);

	/*
	* 获取载具名称
	*/
	const Expression& GetVehicle() const;

	/*
	* 设置接收者姓名
	* @name: 姓名（空字符串表示玩家）
	*/
	void SetName(Expression name);

	/*
	* 获取接收者姓名
	*/
	const Expression& GetName() const;

	/*
	* 设置是否强制转移
	* @force: 是否强制
	*/
	void SetForce(Expression force);

	/*
	* 获取是否强制转移
	*/
	const Expression& GetForce() const;

private:
	// 载具名称
	Expression vehicle;

	// 接收者姓名（空字符串表示玩家）
	Expression name;

	// 是否强制覆盖已有归属
	Expression force;

};

// 移除载具
class RemoveVehicleChange : public Change {
public:
	/*
	* 默认构造移除载具变化
	*/
	RemoveVehicleChange();

	/*
	* 构造移除载具变化
	* @vehicle: 载具名称
	* @name: 当前所有者姓名（空字符串表示玩家）
	*/
	RemoveVehicleChange(Expression vehicle, Expression name);

	/*
	* 析构移除载具变化
	*/
	virtual ~RemoveVehicleChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置载具名称
	* @vehicle: 名称
	*/
	void SetVehicle(Expression vehicle);

	/*
	* 获取载具名称
	*/
	const Expression& GetVehicle() const;

	/*
	* 设置当前所有者姓名
	* @name: 姓名（空字符串表示玩家）
	*/
	void SetName(Expression name);

	/*
	* 获取当前所有者姓名
	*/
	const Expression& GetName() const;

private:
	// 载具名称
	Expression vehicle;

	// 当前所有者姓名（空字符串表示玩家）
	Expression name;

};

// 给予物品
class GiveObjectChange : public Change {
public:
	/*
	* 默认构造给予物品资产变化
	*/
	GiveObjectChange();

	/*
	* 构造给予物品资产变化
	* @object: 资产类型标识
	* @num: 数量
	*/
	GiveObjectChange(Expression object, Expression num);

	/*
	* 析构给予物品资产变化
	*/
	virtual ~GiveObjectChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置资产类型标识
	* @object: 标识
	*/
	void SetObject(Expression object);

	/*
	* 获取资产类型标识
	*/
	const Expression& GetObject() const;

	/*
	* 设置数量
	* @num: 数量
	*/
	void SetNum(Expression num);

	/*
	* 获取数量
	*/
	const Expression& GetNum() const;

private:
	// 资产类型标识
	Expression object;

	// 数量
	Expression num;

};

// 移除物品
class RemoveObjectChange : public Change {
public:
	/*
	* 默认构造移除物品资产变化
	*/
	RemoveObjectChange();

	/*
	* 构造移除物品资产变化
	* @object: 资产类型标识
	* @num: 数量
	* @force: 数量不足时是否删除已有数量
	*/
	RemoveObjectChange(Expression object, Expression num, Expression force);

	/*
	* 析构移除物品资产变化
	*/
	virtual ~RemoveObjectChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置资产类型标识
	* @object: 标识
	*/
	void SetObject(Expression object);

	/*
	* 获取资产类型标识
	*/
	const Expression& GetObject() const;

	/*
	* 设置数量
	* @num: 数量
	*/
	void SetNum(Expression num);

	/*
	* 获取数量
	*/
	const Expression& GetNum() const;

	/*
	* 设置数量不足时是否强制删除
	* @force: 是否强制
	*/
	void SetForce(Expression force);

	/*
	* 获取数量不足时是否强制删除
	*/
	const Expression& GetForce() const;

private:
	// 资产类型标识
	Expression object;

	// 数量
	Expression num;

	// 数量不足时是否强制删除已有数量
	Expression force;

};

// 受伤
class PlayerInjuredChange : public Change {
public:
	/*
	* 默认构造受伤变化
	*/
	PlayerInjuredChange();

	/*
	* 构造受伤变化
	* @wound: 伤势描述
	*/
	PlayerInjuredChange(Expression wound);

	/*
	* 析构受伤变化
	*/
	virtual ~PlayerInjuredChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置伤势描述
	* @wound: 伤势
	*/
	void SetWound(Expression wound);

	/*
	* 获取伤势描述
	*/
	const Expression& GetWound() const;

private:
	// 伤势描述
	Expression wound;

};

// 痊愈
class PlayerCuredChange : public Change {
public:
	/*
	* 默认构造痊愈变化
	*/
	PlayerCuredChange();

	/*
	* 构造痊愈变化
	* @wound: 痊愈的伤势
	*/
	PlayerCuredChange(Expression wound);

	/*
	* 析构痊愈变化
	*/
	virtual ~PlayerCuredChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置痊愈的伤势
	* @wound: 伤势
	*/
	void SetWound(Expression wound);

	/*
	* 获取痊愈的伤势
	*/
	const Expression& GetWound() const;

private:
	// 痊愈的伤势
	Expression wound;

};

// 生病
class PlayerIllChange : public Change {
public:
	/*
	* 默认构造生病变化
	*/
	PlayerIllChange();

	/*
	* 构造生病变化
	* @illness: 病症描述
	*/
	PlayerIllChange(Expression illness);

	/*
	* 析构生病变化
	*/
	virtual ~PlayerIllChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置病症描述
	* @illness: 病症
	*/
	void SetIllness(Expression illness);

	/*
	* 获取病症描述
	*/
	const Expression& GetIllness() const;

private:
	// 病症描述
	Expression illness;

};

// 康复
class PlayerRecoverChange : public Change {
public:
	/*
	* 默认构造康复变化
	*/
	PlayerRecoverChange();

	/*
	* 构造康复变化
	* @illness: 康复的病症
	*/
	PlayerRecoverChange(Expression illness);

	/*
	* 析构康复变化
	*/
	virtual ~PlayerRecoverChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置康复的病症
	* @illness: 病症
	*/
	void SetIllness(Expression illness);

	/*
	* 获取康复的病症
	*/
	const Expression& GetIllness() const;

private:
	// 康复的病症
	Expression illness;

};

// 强制睡眠
class PlayerSleepChange : public Change {
public:
	/*
	* 默认构造强制睡眠变化
	*/
	PlayerSleepChange();

	/*
	* 构造强制睡眠变化
	* @hour: 睡眠时长（小时）
	*/
	PlayerSleepChange(Expression hour);

	/*
	* 析构强制睡眠变化
	*/
	virtual ~PlayerSleepChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置睡眠时长
	* @hour: 小时数
	*/
	void SetHour(Expression hour);

	/*
	* 获取睡眠时长
	*/
	const Expression& GetHour() const;

private:
	// 睡眠时长（小时）
	Expression hour;

};

// 变化时间
class ChangeTimeChange : public Change {
public:
	/*
	* 默认构造变化时间变化
	*/
	ChangeTimeChange();

	/*
	* 构造变化时间变化
	* @delta: 时间偏移量表达式字符串
	*/
	ChangeTimeChange(Expression delta);

	/*
	* 析构变化时间变化
	*/
	virtual ~ChangeTimeChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置时间偏移量
	* @delta: 偏移量表达式字符串
	*/
	void SetDelta(Expression delta);

	/*
	* 获取时间偏移量
	*/
	const Expression& GetDelta() const;

private:
	// 时间偏移量表达式，求值后交给Time解析
	Expression delta;

};

// 变化修炼
class ChangeCultivationChange : public Change {
public:
	/*
	* 默认构造变化修炼变化
	*/
	ChangeCultivationChange();

	/*
	* 构造变化修炼变化
	* @method, level: 修炼方式与等级
	*/
	ChangeCultivationChange(Expression method, Expression level);

	/*
	* 析构变化修炼变化
	*/
	virtual ~ChangeCultivationChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置修炼方式
	* @method: 方式
	*/
	void SetMethod(Expression method);

	/*
	* 获取修炼方式
	*/
	const Expression& GetMethod() const;

	/*
	* 设置等级
	* @level: 等级
	*/
	void SetLevel(Expression level);

	/*
	* 获取等级
	*/
	const Expression& GetLevel() const;

private:
	// 修炼方式
	Expression method;

	// 等级
	Expression level;

};

// 变化通缉
class ChangeWantedChange : public Change {
public:
	/*
	* 默认构造变化通缉变化
	*/
	ChangeWantedChange();

	/*
	* 构造变化通缉变化
	* @reason, level: 通缉原因与等级
	*/
	ChangeWantedChange(Expression reason, Expression level);

	/*
	* 析构变化通缉变化
	*/
	virtual ~ChangeWantedChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置通缉原因
	* @reason: 原因
	*/
	void SetReason(Expression reason);

	/*
	* 获取通缉原因
	*/
	const Expression& GetReason() const;

	/*
	* 设置通缉等级
	* @level: 等级
	*/
	void SetLevel(Expression level);

	/*
	* 获取通缉等级
	*/
	const Expression& GetLevel() const;

private:
	// 通缉原因
	Expression reason;

	// 通缉等级
	Expression level;

};

// 变化天气
class ChangeWeatherChange : public Change {
public:
	/*
	* 默认构造变化天气变化
	*/
	ChangeWeatherChange();

	/*
	* 构造变化天气变化
	* @weather: 天气类型
	*/
	ChangeWeatherChange(Expression weather);

	/*
	* 析构变化天气变化
	*/
	virtual ~ChangeWeatherChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置天气类型
	* @weather: 天气
	*/
	void SetWeather(Expression weather);

	/*
	* 获取天气类型
	*/
	const Expression& GetWeather() const;

private:
	// 天气类型
	Expression weather;

};

// 变化政策
class ChangePolicyChange : public Change {
public:
	/*
	* 默认构造变化政策变化
	*/
	ChangePolicyChange();

	/*
	* 构造变化政策变化
	* @policy: 政策名称
	*/
	ChangePolicyChange(Expression policy);

	/*
	* 析构变化政策变化
	*/
	virtual ~ChangePolicyChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置政策名称
	* @policy: 名称
	*/
	void SetPolicy(Expression policy);

	/*
	* 获取政策名称
	*/
	const Expression& GetPolicy() const;

private:
	// 政策名称
	Expression policy;

};

// 切换控制（阶段4新增，老工程没有对应类型）：把玩家的操控权切换到指定姓名的市民身上。
// 执行逻辑不在Core层（Story::ApplyChange认不出这个类型，Core不知道Actor/Controller的存在），
// 由Forever层（UForeverStoryFrameworkComponent）在遇到这个类型时直接拦截处理，见story.md。
class ChangeControlChange : public Change {
public:
	/*
	* 默认构造切换控制变化
	*/
	ChangeControlChange();

	/*
	* 构造切换控制变化
	* @name: 市民姓名表达式
	*/
	ChangeControlChange(Expression name);

	/*
	* 析构切换控制变化
	*/
	virtual ~ChangeControlChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置市民姓名
	* @name: 姓名表达式
	*/
	void SetName(Expression name);

	/*
	* 获取市民姓名
	*/
	const Expression& GetName() const;

private:
	// 市民姓名
	Expression name;

};
