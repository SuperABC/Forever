#pragma once

#include "class.h"

#include <map>
#include <string>
#include <vector>


struct WorldStage {
	std::string name;
	double seconds = 0;
	unsigned long long unscopedDelta = 0; // 这个阶段里没包随机作用域的抽取次数
	std::string signature;                // 阶段结束时的状态签名(随机流状态+各域计数+地形哈希)，同种子必须逐阶段一致
};

// 一个完整的无画面世界(和AForeverFrameworkActor同一个编排顺序，去掉UE可视化步骤)。
// 故意不析构：Core对象之间互相持有裸指针，进程随后就退出，和EconSim一致。
struct TestWorld {
	Map* map = nullptr;
	Populace* populace = nullptr;
	Society* society = nullptr;
	Industry* industry = nullptr;
	Traffic* traffic = nullptr;
	Story* story = nullptr;
	Player* player = nullptr;
	Polit* polit = nullptr;
	std::vector<WorldStage> stages;
	int mapSize = 0;
};

// 读config(seed==0表示不注入种子)、刷新Registry。种子通过临时config的顶层"seed"注入，临时文件放在workDir。
bool LoadTestConfig(const std::string& basePath, unsigned long long seed, const std::string& workDir, std::string& error);

// 按顺序生成整个世界，每个阶段记耗时和unscopedDelta。失败(异常)返回false并写error。
bool BuildWorld(TestWorld& world, int mapSize, std::string& error);

// 全部域的确定性指纹：key->value，两次同种子运行必须逐项相同。
std::map<std::string, std::string> ComputeFingerprint(TestWorld& world);
