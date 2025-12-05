#pragma once

#include "utility.h"
#include "asset.h"
#include "job.h"
#include "name.h"
#include "person.h"
#include "scheduler.h"
#include "room.h"
#include "change.h"
#include "script.h"

#include <windows.h>


class Populace {
public:
	Populace();
	~Populace();

	// 读取Mods
	void InitAssets();
	void InitJobs();
	void InitNames();
	void InitSchedulers();

	// 读取配置文件
	void ReadConfigs(std::string path) const;

	// 初始化市民
	void Init(int accomodation);

	// 分配调度
	void Schedule();

	// 释放空间
	void Destroy();

	// 时钟前进
	void Tick();

	// 输出当前地图
	void Print();

	// 应用变更
	void ApplyChange(std::shared_ptr<Change> change, std::unique_ptr<Script>& script);

	// 保存/加载人口
	void Load(std::string path);
	void Save(std::string path);

	// 获取当前时间
	Time GetTime();

	// 获取全部市民
	std::vector<std::shared_ptr<Person>>& GetCitizens();

	// 获取附近市民
	std::vector<std::shared_ptr<Person>> GetPasser(int x, int y);
	std::vector<std::shared_ptr<Person>> GetPasser(std::shared_ptr<Room>);
	std::vector<std::shared_ptr<Person>> GetComer(int x, int y);
	std::vector<std::shared_ptr<Person>> GetComer(std::shared_ptr<Room>);

	// 获取Job工厂
	std::unique_ptr<JobFactory>& GetJobFactory();

private:
	// Mod管理
	std::vector<HMODULE> modHandles;
	std::unique_ptr<AssetFactory> assetFactory;
	std::unique_ptr<JobFactory> jobFactory;
	std::unique_ptr<NameFactory> nameFactory;
	std::unique_ptr<SchedulerFactory> schedulerFactory;

	// 游戏内时间
	Time time;

	// 市民管理
	std::vector<std::shared_ptr<Person>> citizens;

	// 生成市民
	void GenerateCitizens(int num);
};

