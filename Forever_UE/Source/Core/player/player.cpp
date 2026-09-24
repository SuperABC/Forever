#include "player/player.h"

#include "common/utility.h"

#include <algorithm>


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

float Player::GetHealth() const { return health; }
float Player::GetMaxHealth() const { return maxHealth; }

float Player::TakeDamage(float amount) {
	health = std::max(0.f, health - amount);
	return health;
}

float Player::Heal(float amount) {
	health = std::min(maxHealth, health + amount);
	return health;
}

bool Player::IsDead() const { return health <= 0.f; }
