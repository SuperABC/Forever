#include "traffic.h"

void Traffic::Tick(const Time& currentTime, bool crossedDay, PostHandle* post) {
	// 占位，等Traffic域真的有需要每帧处理的逻辑时再补，见traffic.h声明处注释。
}

void Traffic::ApplyChange(const Change* change, const ScriptContext& context) {
	// 占位，等Traffic域真的有需要处理的Change子类时再补，见traffic.h声明处注释。
}
