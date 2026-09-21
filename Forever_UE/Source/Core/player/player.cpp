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
	day = time->GetDay();
	time->AddMilliseconds(static_cast<int>(delta * 60 * 1000 * timeFlowRatio));
}

void Player::SetTimeFlowRatio(double ratio) {
	timeFlowRatio = ratio;
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
