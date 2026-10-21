#pragma once

#include <string>

#include "common/json.h"
#include "story/script_mod.h"

// TestScript：test.script的主线ScriptMod，把game_start里label为"control"的PlaceHolderChange换成"控制一个
// 随机有家的市民"的ChangeControlChange，否则玩家停在飞行相机上。
//
// controlChange是mod自己长期持有的成员，actionStack只存它的指针，见script_mod.h"mod往actionStack里
// 塞的指针必须是mod自己长期维护"的约定。
class TestScript : public ScriptMod {
public:
	TestScript() { name = std::string("test") + std::to_string(count++); }

	static const char* GetId() { return "script_test"; }
	virtual const char* GetType() const override { return "script_test"; }
	virtual const char* GetName() override { return name.c_str(); }

	virtual void WrapScript(const Event* event, const std::vector<ScriptAction>& actions,
		const ScriptContext& context, PostHandle* post) override {
		AutoCopy(actions);
		if (!post) return;

		int controlIdx = FindLabel("control", context);
		if (controlIdx < 0) return;

		JsonValue request(DATA_OBJECT);
		request["post"] = "random citizen";
		post->Post(request);
		const JsonValue& result = post->GetResult();
		if (result["result"].AsString() == "success") {
			controlChange.SetName(result["name"].AsString());
			actionStack.back()[controlIdx] = static_cast<const Change*>(&controlChange);
		}
	}

	virtual int GetInstanceCounter() const override { return count; }
	virtual void SetInstanceCounter(int value) override { count = value; }

private:
	static inline int count = 0;
	std::string name;
	ChangeControlChange controlChange;
};
