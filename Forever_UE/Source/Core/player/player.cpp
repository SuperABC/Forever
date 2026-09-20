#include "player/player.h"

#include "common/utility.h"


Player::Player() {
}

Player::~Player() {
	delete time;
}

void Player::Init() {
	time = new Time();
	time->SetHour(8);
}

void Player::Tick(float delta) {
	constexpr double kTimeFlowRatio = 2.0; // 见player.md"time_flow_ratio"一节
	day = time->GetDay();
	time->AddMilliseconds(static_cast<int>(delta * 60 * 1000 * kTimeFlowRatio));
}

Time* Player::GetTime() const {
	return time;
}

void Player::SetTime(const Time& newTime) {
	day = time->GetDay();
	*time = newTime;
}

bool Player::CrossDay() {
	return day != time->GetDay();
}
