#pragma once

#include <string>
#include <vector>
#include <utility>

#include "map/geometry.h"

// BuildingMod：Building这次有两种生成方式（显式占位 + 权重CDF随机填充，照抄老工程
// Map::InitContents对Building的处理，见Source/Core/map/map.md"InitBuildings"一节）。
class BuildingMod {
public:
	BuildingMod() = default;
	virtual ~BuildingMod() = default;

	virtual const char* GetType() const = 0;
	virtual const char* GetName() = 0;

	// config.json里"building_mods"数组中该mod id后面的命令行式参数字符串(如
	// "pengzhan --density 1.0"里的"--density 1.0"),由<Concept>Factory::Create<Concept>
	// 创建实例后立刻调用一次。默认空实现,不需要参数的mod不用重写,格式解析完全由重写者
	// 自己决定。
	virtual void ApplyArgs(const std::string& args) {}

	std::vector<LotPlacementRequest> explicitPlacements;

	// 某个lot应该登记的权重——不直接调用lot->AddCandidate()，原因见下。
	struct CandidateWeight {
		Lot* lot = nullptr;
		float weight = 0.f;
	};

	// mod自己拥有的候选权重表：Distribute()里想给某个lot登记权重时push进这里，不要直接调用
	// lot->AddCandidate(...)。原因：Lot::AddCandidate是非虚成员函数，Basic.dll/Empty.dll等
	// mod dll和Forever.dll各自独立编译了一份Dependence.lib，如果mod直接调用它往Lot自己的
	// std::vector<candidates>里塞数据，这块内存会被mod dll的分配器分配；但Lot对象本身是
	// Forever.dll分配、也由Forever.dll(经Roadnet::~Roadnet())析构的，UE给每个模块都覆写了
	// operator new/delete(PerModuleInline.inl，走FMemory)，Forever.dll析构时会用自己的
	// FMemory::Free释放一块mod dll用普通CRT分配出来的内存，两边分配器对不上，程序退出时
	// 析构Lot会直接崩溃(EXCEPTION_ACCESS_VIOLATION，已实测复现)。改成mod只把
	// (lot, weight)这对纯数据push进自己拥有、自己负责释放的candidateWeights，引擎
	// (Map::InitBuildings)读到这个表之后再自己调用lot->AddCandidate(...)——这次是
	// Forever.dll编译的那份AddCandidate在执行，分配器和后续析构完全一致。
	std::vector<CandidateWeight> candidateWeights;

	// 引擎按当前全图lot列表（剩余空闲面积降序）调用一次。mod在其中可以对任意lot调用两种
	// 方式之一（或都不调）：①push一条explicitPlacements请求；②push一条candidateWeights，
	// 供Zone阶段结束后的Building权重CDF随机填充使用（引擎会代为调用lot->AddCandidate，
	// 见上）。两种方式可以对不同lot混用。
	virtual void Distribute(const std::vector<Lot*>& lots) = 0;

	virtual float RandomAcreage() = 0;
	virtual float GetAcreageMin() = 0;
	virtual float GetAcreageMax() = 0;
};
