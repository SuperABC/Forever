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
	const std::string& GetCondition() const;

	/*
	* 设置控制条件
	* @condition: 条件表达式
	*/
	void SetCondition(const std::string& condition);

private:
	// 控制条件
	std::string condition;

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
	ForRangeChange(std::string var, std::string from, std::string to, std::string step,
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
	const std::string& GetFrom() const;

	/*
	* 获取终止表达式
	*/
	const std::string& GetTo() const;

	/*
	* 获取步长表达式
	*/
	const std::string& GetStep() const;

	/*
	* 获取循环体变化列表
	*/
	const std::vector<const Change*>& GetChanges() const;

private:
	// 循环变量名
	std::string var;

	// 起始表达式
	std::string from;

	// 终止表达式
	std::string to;

	// 步长表达式
	std::string step;

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
	PlaceHolderChange(std::string label);

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
	void SetLabel(std::string label);

	/*
	* 获取标签
	*/
	const std::string& GetLabel() const;

private:
	// 标签
	std::string label;

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
	GlobalMessageChange(std::string message);

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
	void SetMessage(std::string message);

	/*
	* 获取消息内容
	*/
	const std::string& GetMessage() const;

private:
	// 消息内容
	std::string message;

};

// 调试打印（已实现，见Forever层的Change分发——Core/story/story.cpp的Story::ApplyChange
// 不处理这个类型，因为打印要调用GEngine::AddOnScreenDebugMessage，是UE调用，Core不能
// 依赖UE，见ForeverFrameworkActor.cpp/ForeverStoryFrameworkComponent.cpp里的
// dynamic_cast<const DebugPrintChange*>分支）。和GlobalMessageChange同一个形状，这次
// 单独新增一个类型而不是复用GlobalMessageChange，是因为两者语义不同：这个类型只用于
// 开发期调试输出，不是游戏内广播消息。
class DebugPrintChange : public Change {
public:
	/*
	* 默认构造调试打印变化
	*/
	DebugPrintChange();

	/*
	* 构造调试打印变化
	* @message: 打印内容
	*/
	DebugPrintChange(std::string message);

	/*
	* 析构调试打印变化
	*/
	virtual ~DebugPrintChange();

	/*
	* 变化类型
	*/
	virtual const std::string& GetType() const override;

	/*
	* 设置打印内容
	* @message: 打印内容
	*/
	void SetMessage(std::string message);

	/*
	* 获取打印内容
	*/
	const std::string& GetMessage() const;

private:
	// 打印内容
	std::string message;

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
	SetValueChange(std::string variable, std::string value);

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
	void SetValue(std::string value);

	/*
	* 获取值表达式
	*/
	const std::string& GetValue() const;

private:
	// 变量名
	std::string variable;

	// 值表达式
	std::string value;

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
	GlobalSettingChange(std::string setting, std::string value);

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
	void SetValue(std::string value);

	/*
	* 获取值表达式
	*/
	const std::string& GetValue() const;

private:
	// 设置名
	std::string setting;

	// 值表达式
	std::string value;

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
	AddOptionChange(std::string name, std::string option);

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
	// 目标名称
	std::string name;

	// 选项文本
	std::string option;

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
	RemoveOptionChange(std::string name, std::string option);

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
	// 目标名称
	std::string name;

	// 选项文本
	std::string option;

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
	AddGlobalChange(std::string option);

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
	void SetOption(std::string option);

	/*
	* 获取选项文本
	*/
	const std::string& GetOption() const;

private:
	// 选项文本
	std::string option;

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
	RemoveGlobalChange(std::string option);

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
	void SetOption(std::string option);

	/*
	* 获取选项文本
	*/
	const std::string& GetOption() const;

private:
	// 选项文本
	std::string option;

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
	SpawnNpcChange(std::string avatar, std::string name, std::string gender, std::string birthday, std::string height, std::string weight,
		std::string nick, std::string deposit, std::string phone, std::string home, std::vector<std::string> jobs, std::string scheduler);

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
	void SetAvatar(std::string avatar);

	/*
	* 获取形象标识
	*/
	const std::string& GetAvatar() const;

	/*
	* 设置姓名
	* @name: 姓名
	*/
	void SetName(std::string name);

	/*
	* 获取姓名
	*/
	const std::string& GetName() const;

	/*
	* 设置性别
	* @gender: 性别
	*/
	void SetGender(std::string gender);

	/*
	* 获取性别
	*/
	const std::string& GetGender() const;

	/*
	* 设置生日
	* @birthday: 生日
	*/
	void SetBirthday(std::string birthday);

	/*
	* 获取生日
	*/
	const std::string& GetBirthday() const;

	/*
	* 设置身高
	* @height: 身高
	*/
	void SetHeight(std::string height);

	/*
	* 获取身高
	*/
	const std::string& GetHeight() const;

	/*
	* 设置体重
	* @weight: 体重
	*/
	void SetWeight(std::string weight);

	/*
	* 获取体重
	*/
	const std::string& GetWeight() const;

	/*
	* 设置昵称
	* @nick: 昵称
	*/
	void SetNick(std::string nick);

	/*
	* 获取昵称
	*/
	const std::string& GetNick() const;

	/*
	* 设置存款金额
	* @deposit: 存款
	*/
	void SetDeposit(std::string deposit);

	/*
	* 获取存款金额
	*/
	const std::string& GetDeposit() const;

	/*
	* 设置手机号
	* @phone: 手机号
	*/
	void SetPhone(std::string phone);

	/*
	* 获取手机号
	*/
	const std::string& GetPhone() const;

	/*
	* 设置住所
	* @home: 住所名称
	*/
	void SetHome(std::string home);

	/*
	* 获取住所
	*/
	const std::string& GetHome() const;

	/*
	* 设置职业列表
	* @jobs: 职业类型标识列表
	*/
	void SetJobs(std::vector<std::string> jobs);

	/*
	* 获取职业列表
	*/
	const std::vector<std::string>& GetJobs() const;

	/*
	* 设置调度类型
	* @scheduler: 调度类型标识
	*/
	void SetScheduler(std::string scheduler);

	/*
	* 获取调度类型
	*/
	const std::string& GetScheduler() const;

private:
	// 形象标识
	std::string avatar;

	// 姓名
	std::string name;

	// 性别
	std::string gender;

	// 生日
	std::string birthday;

	// 身高
	std::string height;

	// 体重
	std::string weight;

	// 昵称
	std::string nick;

	// 存款金额
	std::string deposit;

	// 手机号
	std::string phone;

	// 住所
	std::string home;

	// 职业类型标识列表
	std::vector<std::string> jobs;

	// 调度类型标识
	std::string scheduler;

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
	RemoveNpcChange(std::string name);

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
	void SetName(std::string name);

	/*
	* 获取NPC姓名
	*/
	const std::string& GetName() const;

private:
	// NPC姓名
	std::string name;

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
	TeleportCitizenChange(std::string name, std::string destination);

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
	void SetName(std::string name);

	/*
	* 获取市民姓名
	*/
	const std::string& GetName() const;

	/*
	* 设置目标房间名称
	* @destination: 名称
	*/
	void SetDestination(std::string destination);

	/*
	* 获取目标房间名称
	*/
	const std::string& GetDestination() const;

private:
	// 市民姓名
	std::string name;

	// 目标房间名称
	std::string destination;

};

// NPC自动导航——name/destination这次故意用纯std::string，不是Expression（和
// SetValueChange::variable"这是要被赋值的变量名本身，不是内容"同一个理由：这个Change
// 唯一的构造方只有Source/Basic/society/job_basic.cpp的ShopSalerJob::ExecNode，值永远是
// 已经算好的具体字符串，不需要$$动态求值能力；没有任何JSON分支消费这个类型）。**这不只是
// 风格选择，是一个真实崩溃的修复**：std::string::EvaluateValue()内部对着mod（Basic.dll）
// 构造的ConstantExpression节点做虚函数调用，这个调用实际执行的是Basic.dll编译的代码，
// 返回值（含堆分配的字符串缓冲区）由Basic.dll的标准CRT operator new分配；但UE给每个UE
// 模块（如UnrealEditor-Forever.dll）单独重载了全局operator new/delete、经由FMemory分配
// （见PerModuleInline.inl），Basic.dll是普通Win32 DLL没有这层重载——当这个返回值在
// Forever.dll侧被销毁时，触发的是Forever.dll重载过的operator delete（转发进FMemory），
// 但这块内存根本不是FMemory分配的，直接堆损坏崩溃（PIE验证复现：9点市民上班、地址字符串
// 一旦长到超过std::string的SSO阈值就必现，"home"/"workplace"这类短字面量因为完全不触发
// 堆分配才一直没暴露这个问题）。纯std::string字段则完全不涉及虚函数求值——
// GetDestination()这类non-virtual getter直接返回引用，调用方拷贝时用的是调用方自己模块
// 编译的std::string拷贝构造函数，从头到尾都在同一个模块的分配器里，不会跨这条边界。
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
	NPCNavigateChange(std::string name, std::string destination);

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
	void SetName(std::string name);

	/*
	* 获取NPC姓名
	*/
	const std::string& GetName() const;

	/*
	* 设置目标位置名称
	* @destination: 名称
	*/
	void SetDestination(std::string destination);

	/*
	* 获取目标位置名称
	*/
	const std::string& GetDestination() const;

private:
	// NPC姓名
	std::string name;

	// 目标位置名称
	std::string destination;

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
	TeleportPlayerChange(std::string destination);

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
	void SetDestination(std::string destination);

	/*
	* 获取目标房间名称
	*/
	const std::string& GetDestination() const;

private:
	// 目标房间名称
	std::string destination;

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
	OpenShopChange(std::string saler);

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
	void SetSaler(std::string saler);

	/*
	* 获取售货员姓名
	*/
	const std::string& GetSaler() const;

private:
	// 售货员姓名
	std::string saler;

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
	StartPuzzleChange(std::string puzzle);

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
	void SetPuzzle(std::string puzzle);

	/*
	* 获取小游戏类型标识
	*/
	const std::string& GetPuzzle() const;

private:
	// 小游戏类型标识
	std::string puzzle;

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
	EnterVehicleChange(std::string vehicle);

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
	void SetVehicle(std::string vehicle);

	/*
	* 获取载具名称
	*/
	const std::string& GetVehicle() const;

private:
	// 载具名称
	std::string vehicle;

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
	LeaveVehicleChange(std::string vehicle);

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
	void SetVehicle(std::string vehicle);

	/*
	* 获取载具名称
	*/
	const std::string& GetVehicle() const;

private:
	// 载具名称
	std::string vehicle;

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
	CreateTimerChange(std::string name, std::string time, std::string category, std::string label);

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
	void SetName(std::string name);

	/*
	* 获取计时器名称
	*/
	const std::string& GetName() const;

	/*
	* 设置目标时刻
	* @time: 时刻字符串
	*/
	void SetTime(std::string time);

	/*
	* 获取目标时刻
	*/
	const std::string& GetTime() const;

	/*
	* 设置所属脚本类型
	* @category: 类型标识
	*/
	void SetCategory(std::string category);

	/*
	* 获取所属脚本类型
	*/
	const std::string& GetCategory() const;

	/*
	* 设置所属实体名称
	* @label: 名称
	*/
	void SetLabel(std::string label);

	/*
	* 获取所属实体名称
	*/
	const std::string& GetLabel() const;

private:
	// 计时器名称
	std::string name;

	// 目标时刻
	std::string time;

	// 所属脚本类型
	std::string category;

	// 所属实体名称
	std::string label;

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
	RemoveTimerChange(std::string name);

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
	void SetName(std::string name);

	/*
	* 获取计时器名称
	*/
	const std::string& GetName() const;

private:
	// 计时器名称
	std::string name;

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
	LaunchElevatorChange(std::string building, std::string elevator, std::string command);

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
	void SetBuilding(std::string building);

	/*
	* 获取建筑名称
	*/
	const std::string& GetBuilding() const;

	/*
	* 设置电梯名称
	* @elevator: 名称
	*/
	void SetElevator(std::string elevator);

	/*
	* 获取电梯名称
	*/
	const std::string& GetElevator() const;

	/*
	* 设置指令
	* @command: 指令字符串
	*/
	void SetCommand(std::string command);

	/*
	* 获取指令
	*/
	const std::string& GetCommand() const;

private:
	// 建筑名称
	std::string building;

	// 电梯名称
	std::string elevator;

	// 指令
	std::string command;

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
	PlayVideoChange(std::string path);

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
	void SetPath(std::string path);

	/*
	* 获取视频文件路径
	*/
	const std::string& GetPath() const;

private:
	// 视频文件路径
	std::string path;

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
	PlayBgmChange(std::string bgm, std::string loop);

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
	void SetBgm(std::string bgm);

	/*
	* 获取背景音乐标识
	*/
	const std::string& GetBgm() const;

	/*
	* 设置是否循环播放
	* @loop: 是否循环
	*/
	void SetLoop(std::string loop);

	/*
	* 获取是否循环播放
	*/
	const std::string& GetLoop() const;

private:
	// 背景音乐标识
	std::string bgm;

	// 是否循环播放
	std::string loop;

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
	BankTransactionChange(std::string name, std::string amount);

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
	void SetName(std::string name);

	/*
	* 获取收款人姓名
	*/
	const std::string& GetName() const;

	/*
	* 设置金额
	* @amount: 金额
	*/
	void SetAmount(std::string amount);

	/*
	* 获取金额
	*/
	const std::string& GetAmount() const;

private:
	// 收款人姓名（空字符串表示玩家）
	std::string name;

	// 金额
	std::string amount;

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
	GiveEstateChange(std::string estate, std::string name, std::string force);

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
	void SetEstate(std::string estate);

	/*
	* 获取房产名称
	*/
	const std::string& GetEstate() const;

	/*
	* 设置接收者姓名
	* @name: 姓名（空字符串表示玩家）
	*/
	void SetName(std::string name);

	/*
	* 获取接收者姓名
	*/
	const std::string& GetName() const;

	/*
	* 设置是否强制转移
	* @force: 是否强制
	*/
	void SetForce(std::string force);

	/*
	* 获取是否强制转移
	*/
	const std::string& GetForce() const;

private:
	// 房产名称
	std::string estate;

	// 接收者姓名（空字符串表示玩家）
	std::string name;

	// 是否强制覆盖已有归属
	std::string force;

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
	RemoveEstateChange(std::string estate, std::string name);

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
	void SetEstate(std::string estate);

	/*
	* 获取房产名称
	*/
	const std::string& GetEstate() const;

	/*
	* 设置当前所有者姓名
	* @name: 姓名（空字符串表示玩家）
	*/
	void SetName(std::string name);

	/*
	* 获取当前所有者姓名
	*/
	const std::string& GetName() const;

private:
	// 房产名称
	std::string estate;

	// 当前所有者姓名（空字符串表示玩家）
	std::string name;

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
	GiveVehicleChange(std::string vehicle, std::string name, std::string force);

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
	void SetVehicle(std::string vehicle);

	/*
	* 获取载具名称
	*/
	const std::string& GetVehicle() const;

	/*
	* 设置接收者姓名
	* @name: 姓名（空字符串表示玩家）
	*/
	void SetName(std::string name);

	/*
	* 获取接收者姓名
	*/
	const std::string& GetName() const;

	/*
	* 设置是否强制转移
	* @force: 是否强制
	*/
	void SetForce(std::string force);

	/*
	* 获取是否强制转移
	*/
	const std::string& GetForce() const;

private:
	// 载具名称
	std::string vehicle;

	// 接收者姓名（空字符串表示玩家）
	std::string name;

	// 是否强制覆盖已有归属
	std::string force;

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
	RemoveVehicleChange(std::string vehicle, std::string name);

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
	void SetVehicle(std::string vehicle);

	/*
	* 获取载具名称
	*/
	const std::string& GetVehicle() const;

	/*
	* 设置当前所有者姓名
	* @name: 姓名（空字符串表示玩家）
	*/
	void SetName(std::string name);

	/*
	* 获取当前所有者姓名
	*/
	const std::string& GetName() const;

private:
	// 载具名称
	std::string vehicle;

	// 当前所有者姓名（空字符串表示玩家）
	std::string name;

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
	GiveObjectChange(std::string object, std::string num);

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
	void SetObject(std::string object);

	/*
	* 获取资产类型标识
	*/
	const std::string& GetObject() const;

	/*
	* 设置数量
	* @num: 数量
	*/
	void SetNum(std::string num);

	/*
	* 获取数量
	*/
	const std::string& GetNum() const;

private:
	// 资产类型标识
	std::string object;

	// 数量
	std::string num;

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
	RemoveObjectChange(std::string object, std::string num, std::string force);

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
	void SetObject(std::string object);

	/*
	* 获取资产类型标识
	*/
	const std::string& GetObject() const;

	/*
	* 设置数量
	* @num: 数量
	*/
	void SetNum(std::string num);

	/*
	* 获取数量
	*/
	const std::string& GetNum() const;

	/*
	* 设置数量不足时是否强制删除
	* @force: 是否强制
	*/
	void SetForce(std::string force);

	/*
	* 获取数量不足时是否强制删除
	*/
	const std::string& GetForce() const;

private:
	// 资产类型标识
	std::string object;

	// 数量
	std::string num;

	// 数量不足时是否强制删除已有数量
	std::string force;

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
	PlayerInjuredChange(std::string wound);

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
	PlayerCuredChange(std::string wound);

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
	PlayerIllChange(std::string illness);

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
	PlayerRecoverChange(std::string illness);

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
	void SetIllness(std::string illness);

	/*
	* 获取康复的病症
	*/
	const std::string& GetIllness() const;

private:
	// 康复的病症
	std::string illness;

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
	PlayerSleepChange(std::string hour);

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
	void SetHour(std::string hour);

	/*
	* 获取睡眠时长
	*/
	const std::string& GetHour() const;

private:
	// 睡眠时长（小时）
	std::string hour;

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
	ChangeTimeChange(std::string delta);

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
	void SetDelta(std::string delta);

	/*
	* 获取时间偏移量
	*/
	const std::string& GetDelta() const;

private:
	// 时间偏移量表达式，求值后交给Time解析
	std::string delta;

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
	ChangeCultivationChange(std::string method, std::string level);

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

	// 等级
	std::string level;

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
	ChangeWantedChange(std::string reason, std::string level);

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
	ChangeWeatherChange(std::string weather);

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
	void SetWeather(std::string weather);

	/*
	* 获取天气类型
	*/
	const std::string& GetWeather() const;

private:
	// 天气类型
	std::string weather;

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
	ChangePolicyChange(std::string policy);

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
	void SetPolicy(std::string policy);

	/*
	* 获取政策名称
	*/
	const std::string& GetPolicy() const;

private:
	// 政策名称
	std::string policy;

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
	ChangeControlChange(std::string name);

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
	void SetName(std::string name);

	/*
	* 获取市民姓名
	*/
	const std::string& GetName() const;

private:
	// 市民姓名
	std::string name;

};
