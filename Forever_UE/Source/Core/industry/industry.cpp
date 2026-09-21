#include "industry.h"

void Industry::Tick(const Time& currentTime, bool crossedDay, PostHandle* post) {
	// 占位，等Industry域真的有需要每帧处理的逻辑时再补，见industry.h声明处注释。
}

void Industry::ApplyChange(const Change* change, const ScriptContext& context) {
	// 占位，等Industry域真的有需要处理的Change子类时再补，见industry.h声明处注释。
}
