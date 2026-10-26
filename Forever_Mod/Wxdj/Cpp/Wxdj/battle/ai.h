#pragma once

#include "action.h"
#include "controller.h"
#include "cultivator.h"

#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>


namespace wxdj {

// 对战AI(通用模型)：Perceiver式小Transformer(约92万参数)，输入"双方招式 + 当前局面 + 最近8回合出招记录"，
// 输出这回合怎么出招(招类 -> 第一个招式 -> 双招的第二个招式)。结构和特征照 ai/transformer 的 model.py / features.h / env.cpp，
// 网页版(Online/public/ai.html)里的推理是同一份逻辑的JS移植；权重文件由 ai/transformer/export_weights.py 导出(Resource/act/ai_weights.bin)。
//
// 用法：
//   AI ai(seat); ai.Load(path);
//   每回合：controller.StartTurn()之后 ai.BeginRound(controller); action = ai.Choose(controller, round, moves);
//           结算完(EndTurn)之后 ai.EndRound(controller, {席位0的招, 席位1的招}, 本回合产生的日志);
//   新的一场战斗开始时 ai.Reset()。
class AI {
public:
	// seat：这个AI控制的修炼者在控制器里的下标
	explicit AI(int seat);

	// 加载权重文件，成功返回true
	bool Load(const std::string& file);
	bool IsLoaded() const { return loaded; }

	void Reset();

	// 每回合出招前调用(控制器已经StartTurn)：记下出招前的状态，回合结束时算"这回合状态变化"要用
	void BeginRound(const Controller& controller);

	// 出招结算(EndTurn)之后调用：actions按席位排列，logs是本回合产生的全部日志(要用其中的闪避记录)
	void EndRound(const Controller& controller, const std::vector<std::shared_ptr<Action>>& actions,
		const std::vector<Log>& logs);

	// moves：候选招式在招式表里的下标(每个家族取一式，自己和对手用同一张招式表)
	// greedy为true时取概率最大的，否则按概率采样。没有加载好/推理出错时返回空指针，由调用方退回别的出招方式
	std::shared_ptr<Action> Choose(Controller& controller, int round, const std::vector<int>& moves, bool greedy = false);

	// 诊断/对拍用：第一个决策的原始输出(招类5个logit + 每个候选招式的第一招logit)
	struct Output {
		std::vector<float> cls;
		std::vector<float> first;
		float value = 0.f;
	};
	Output Evaluate(Controller& controller, int round, const std::vector<int>& moves);

private:
	struct Tensor {
		std::vector<int> shape;
		std::vector<float> data;
	};

	struct Forward {
		std::vector<float> cls, first, h, ctx;
		float value = 0.f;
	};

	const std::vector<float>& W(const std::string& name) const;
	std::vector<float> Mlp(const std::vector<float>& x, int n, const std::string& prefix, int inD, int hD, int outD) const;
	std::vector<float> Lin(const std::vector<float>& x, int n, const std::string& prefix, int inD, int outD) const;
	std::vector<float> Ln(const std::vector<float>& x, int n, const std::string& prefix) const;
	std::vector<float> Mha(const std::vector<float>& q, int nq, const std::vector<float>& kv, int nk, const std::string& prefix) const;
	std::vector<float> SecondLogits(const std::vector<float>& h, const std::vector<float>& ctx, int nS, int pick,
		const std::vector<char>& allowed) const;

	void BuildInputs(Controller& controller, int round, const std::vector<int>& moves, std::vector<float>& state,
		std::vector<float>& hist, std::vector<char>& histValid, std::vector<float>& msFeat, std::vector<float>& moFeat) const;
	Forward Run(const std::vector<float>& state, const std::vector<float>& hist, const std::vector<char>& histValid,
		const std::vector<float>& msFeat, int nS, const std::vector<float>& moFeat, int nO) const;
	int Sample(const std::vector<float>& logits, bool greedy);

	int seat;
	bool loaded = false;
	std::map<std::string, Tensor> weights;

	// 最近8回合的出招记录(每回合HD维)，以及这回合出招前的状态快照
	std::vector<std::vector<float>> history;
	struct Snapshot { float hp, atk, mp; bool locked; };
	std::vector<Snapshot> snapshot;
	std::mt19937 rng;
};

}
