#include "job_video_maker.h"

#include "common/json.h"

using namespace std;

int VideoMakerJob::count = 0;

VideoMakerJob::VideoMakerJob() : id(count++) {
}

const char* VideoMakerJob::GetName() {
	name = "VideoMakerJob" + to_string(id);
	return name.data();
}

void VideoMakerJob::SetProperty() {
	milestoneNames = { "job_video_maker" };
}

void VideoMakerJob::DailyPlan(const Time&, PostHandle*) {
	// 上下班由job_video_maker.script的计时器驱动，这里不排任何C++节点(plans保持为空)。
}
