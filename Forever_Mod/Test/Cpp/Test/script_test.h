#pragma once

#include <string>

#include "common/json.h"
#include "story/script_mod.h"

// 设计说明见script_test.md。
class TestScript : public ScriptMod {
public:
	TestScript() : id(count++) { }

	static const char* GetId() { return "script_test"; }
	virtual const char* GetType() const override { return "script_test"; }
	virtual const char* GetName() override {
		name = "test" + std::to_string(id);
		return name.data();
	}

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
	int id;
	std::string name;
	ChangeControlChange controlChange;
};
