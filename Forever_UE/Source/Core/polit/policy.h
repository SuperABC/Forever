#pragma once

#include <cstdint>
#include <string>
#include <vector>


// 政府财政参数：补助额（分/天）和各税种税率（百分比）。政策值是状态：由政治域(Polit)持有，党派选举后调整，随polit段存档
// (见polit_save.md)；没有启用任何党派时就一直是下面的默认值。年龄分界、金融参数目前不可调。
struct GovernmentPolicy {
	int64_t pension = 5000;           // 养老金 50元/天
	int64_t unemploymentBenefit = 5000; // 失业补助 50元/天
	int64_t minorAllowance = 2000;    // 未成年补助 20元/天

	double corporateTaxPercent = 25;  // 企业税（月结）
	double incomeTaxPercent = 20;     // 所得税
	double consumptionTaxPercent = 10; // 消费税
	double rentTaxPercent = 20;       // 房租税
	double tariffPercent = 25;        // 关税税率（对不含税价；进口价已含关税，占进口价的25/125=20%），只对进口征收，出口无关税

	// 移民目标人口 = 开局人口 × 此值，Immigration::Daily 每天按"离目标还差多少"迁入。
	double immigrationTargetRatio = 1.5;

	int retirementAge = 60;           // 年满此年龄领养老金；18岁到此年龄之间没有工作的领失业补助
	int adultAge = 18;                // 低于此年龄是未成年，领未成年补助

	// 金融参数，见treasury.md/market.md/investor.md。利率单位是基点(1bp=0.01%)，年化。
	int depositRateBp = 300;          // 存款年利率3%，每月结息
	int bondPremiumBp = 300;          // 国债年利率 = 存款利率 + 此溢价
	int bondTermDays = 360;           // 国债期限（游戏天）
	int64_t reserve = 200000;         // 市民自留的现金缓冲 2000元，超出的才拿去存款/炒股/买国债
	int investIntervalDays = 7;       // 每个市民每隔多少天做一次存款/炒股决策（按下标错开）

	// 负反馈系数(派生数据，每天由Economy::SyncIndustry按政府净头寸/开局注资重算，不存档)：
	// 企业税/所得税/消费税/房租税 = 基础税率 × taxFactor(政府盈余越多越低，范围[0,2])，关税不参与；
	// 三种补助 = 基础补助 × allowanceFactor(政府盈余越多越高)。政府净头寸=政府账户−存款负债−国债负债。
	// 党派调整的是"基础税率/基础补助"(上面的字段)，和这两个系数叠加。
	double taxFactor = 1.0;
	double allowanceFactor = 1.0;

	// 按基础税率(百分数)和当前taxFactor算税额，向下取整到分。
	int64_t Tax(int64_t amount, double basePercent) const;
	// 按当前allowanceFactor算实际补助额(分/天)。
	int64_t Allowance(int64_t base) const;
	// 进口关税：cost是含关税的进口价，返回其中的关税部分(向下取整到分)。
	int64_t Tariff(int64_t cost) const;
};

// 税额 = 金额 × 税率%，向下取整到分。
int64_t TaxOf(int64_t amount, double percent);


// 一项可调政策的定义。Core动态提供：以后加政策只要在PolicyCatalog里多登记一项。
struct PolicyDef {
	enum Kind { TAX, BENEFIT, IMMIGRATION };

	std::string id;    // PolicyId::*，见Dependence/polit/policy_ids.h
	std::string name;  // 中文名，日志/界面用
	Kind kind = TAX;
	double (*get)(const GovernmentPolicy&) = nullptr;
	void (*set)(GovernmentPolicy&, double) = nullptr;
	double defaultValue = 0.0; // GovernmentPolicy的默认值
	double minValue = 0.0;     // 下限：默认值的0.25倍
	double maxValue = 0.0;     // 上限：默认值的4倍，税率另外不超过100
};

class PolicyCatalog {
public:
	// 全部可调政策(固定顺序)。
	static const std::vector<PolicyDef>& All();
	static const PolicyDef* Find(const std::string& id);

	// 把value夹到[min,max]后写进policy，返回实际写入的值(补助取整到分)。
	static double Apply(GovernmentPolicy& policy, const PolicyDef& def, double value);
};
