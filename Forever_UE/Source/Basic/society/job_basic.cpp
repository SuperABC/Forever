#include "job_basic.h"

namespace {
	// 构造一个只求值出固定字面量的Expression——Expression::Parse按DSL文本解析，字符串
	// 字面量用引号包起来(见Dependence/story/expression.cpp的Tokenize)，这里直接拼一个
	// 带引号的常量表达式，不走variable/运算符那套。
	Expression MakeLiteralExpression(const std::string& value) {
		Expression expression;
		expression.Parse("\"" + value + "\"");
		return expression;
	}
}

void ShopSalerJob::DailyPlan(const Time& currentTime) {
	plans.clear();
	plans["leave_home"] = Time(currentTime.GetYear(), currentTime.GetMonth(), currentTime.GetDay(), 9, 0);
	plans["leave_work"] = Time(currentTime.GetYear(), currentTime.GetMonth(), currentTime.GetDay(), 12, 0);
}

void ShopSalerJob::ExecNode(const std::string& node) {
	for (Change* change : changes) delete change;
	changes.clear();

	if (node == "leave_home") {
		changes.push_back(new NPCNavigateChange(MakeLiteralExpression(occupantName), MakeLiteralExpression("workplace")));
	}
	else if (node == "leave_work") {
		changes.push_back(new NPCNavigateChange(MakeLiteralExpression(occupantName), MakeLiteralExpression("home")));
	}
}
