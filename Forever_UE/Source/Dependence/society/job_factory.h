#pragma once

#include "society/job_mod.h"

#include <string>
#include <unordered_map>
#include <vector>


// 最小注册表(创建/销毁/枚举)。旧工程的Temp暂存区+MergeTemp()两段式注册(用来隔离
// "探测阶段"和"正式生效阶段")这次没有搬过来——那套机制是为了绕开非virtual方法
// 跨DLL调用时,mod编译的那份代码去扩容/释放host分配的容器这个问题;现在改成全部公开
// 方法都是virtual(见下),调用永远落在构造这个Factory实例的那一侧,分配器天然一致,
// 不再需要暂存区这层隔离,详见Source/Dependence/README.md"关键设计"一节。
//
// creator/deleter用裸函数指针,不用std::function——mod侧注册的都是无捕获
// (capture-less)lambda,天然能隐式转换成函数指针;裸指针是POD类型,跨DLL传递没有
// "std::function内部堆缓冲由一侧分配、由另一侧释放"的风险(实测std::function版本会在
// Factory析构时崩溃,详见Source/Dependence/README.md)。
//
// 所有公开方法(含析构函数)都标记virtual,即使目前没有任何派生类——原因见
// Source/Dependence/README.md"关键设计"一节:Mod DLL里调用这些方法时,只有virtual
// 才能保证实际执行的是宿主(构造这个Factory实例的那一侧)编译的那份代码,从而让
// registries内部节点的分配和后续析构使用同一侧的堆分配器,避免跨DLL堆损坏。
//
// 参数传递:调用方在RegisterConcept之前调用SetModArgs,把从config.json
// "job_mods"数组解析出的(id, 参数字符串)表交给Factory,一直保留在
// configuredArgs里(不并入registries,避免同一份参数存两份);CreateJob创建实例时
// 直接按id查configuredArgs、把参数字符串传给creator,mod自己的creator决定怎么用
// (传给构造函数/自己存着都行)——Factory不再替mod调用任何"创建后初始化"钩子。
class JobFactory {
public:
	using CreateFunc = JobMod*(*)(const std::string&);
	using DestroyFunc = void(*)(JobMod*);

	JobFactory() = default;
	virtual ~JobFactory() = default;

	virtual void RegisterJob(const std::string& id, CreateFunc creator, DestroyFunc deleter);

	virtual JobMod* CreateJob(const std::string& id);

	// 必须走注册时mod提供的deleter释放,不能Factory直接delete——跨DLL new/delete安全,
	// 通过liveInstances反查实例对应的注册id、再取出对应deleter调用。
	virtual void DestroyJob(JobMod* instance);

	virtual bool CheckRegistered(const std::string& id) const;
	virtual std::vector<std::string> GetRegisteredIds() const;

	// 设置这个concept从config.json"job_mods"数组解析出的(id, 参数字符串)表,
	// 必须在调用方触发mod的RegisterModJobs导出函数之前调用,否则CreateJob
	// 拿实例创建时查不到对应参数。
	virtual void SetModArgs(const std::unordered_map<std::string, std::string>& argsById);

private:
	struct Entry {
		CreateFunc creator;
		DestroyFunc deleter;
	};

	std::unordered_map<std::string, Entry> registries;
	std::unordered_map<JobMod*, std::string> liveInstances;
	std::unordered_map<std::string, std::string> configuredArgs;
};
