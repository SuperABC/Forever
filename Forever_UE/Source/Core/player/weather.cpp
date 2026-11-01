#include "player/weather.h"

#include "common/utility.h"

#include <algorithm>

using namespace std;

namespace {
	// 游戏分钟。均值2天、标准差半天，下限3个游戏小时防止抽到负数或过密。
	constexpr float kIntervalMeanMinutes = 2.f * 24.f * 60.f;
	constexpr float kIntervalStddevMinutes = 12.f * 60.f;
	constexpr float kIntervalMinMinutes = 3.f * 60.f;

	// 随机选择的权重：晴/多云/阴/小/大。"小/大"冬季(12~2月)是雪，其它月份是雨。
	constexpr int kWeightSunny = 40;
	constexpr int kWeightCloudy = 25;
	constexpr int kWeightOvercast = 15;
	constexpr int kWeightLight = 12;
	constexpr int kWeightHeavy = 8;

	bool IsWinter(int month) {
		return month == 12 || month == 1 || month == 2;
	}
}

Weather::Weather() {
	remainingMs = DrawIntervalMs();
}

const char* Weather::ToName(Type type) {
	switch (type) {
	case Type::Sunny: return "sunny";
	case Type::Cloudy: return "cloudy";
	case Type::Overcast: return "overcast";
	case Type::LightRain: return "light_rain";
	case Type::HeavyRain: return "heavy_rain";
	case Type::LightSnow: return "light_snow";
	case Type::HeavySnow: return "heavy_snow";
	}
	return "sunny";
}

bool Weather::FromName(const string& name, Type& out) {
	static const Type all[] = {
		Type::Sunny, Type::Cloudy, Type::Overcast, Type::LightRain,
		Type::HeavyRain, Type::LightSnow, Type::HeavySnow,
	};
	for (Type type : all) {
		if (name == ToName(type)) {
			out = type;
			return true;
		}
	}
	return false;
}

long long Weather::DrawIntervalMs() {
	float minutes = max(GetRandomNormal(kIntervalMeanMinutes, kIntervalStddevMinutes), kIntervalMinMinutes);
	return static_cast<long long>(minutes * 60.f * 1000.f);
}

Weather::Type Weather::PickRandom(int month) const {
	const bool winter = IsWinter(month);
	const Type light = winter ? Type::LightSnow : Type::LightRain;
	const Type heavy = winter ? Type::HeavySnow : Type::HeavyRain;
	const Type options[] = { Type::Sunny, Type::Cloudy, Type::Overcast, light, heavy };
	const int weights[] = { kWeightSunny, kWeightCloudy, kWeightOvercast, kWeightLight, kWeightHeavy };

	int total = 0;
	for (int weight : weights) total += weight;

	// 抽到和当前相同的就重抽几次；万一一直撞上，下面兜底顺延到下一个选项。
	Type picked = type;
	for (int attempt = 0; attempt < 8 && picked == type; attempt++) {
		int roll = GetRandom(total);
		for (int i = 0; i < 5; i++) {
			if (roll < weights[i]) {
				picked = options[i];
				break;
			}
			roll -= weights[i];
		}
	}
	if (picked == type) {
		for (int i = 0; i < 5; i++) {
			if (options[i] != type) return options[i];
		}
	}
	return picked;
}

void Weather::Tick(long long deltaMs, int month) {
	remainingMs -= deltaMs;
	if (remainingMs > 0) return;

	// 一次Tick最多换一次：即使时间跳得很大，也只是换到下一个天气并重抽间隔。
	Type next = PickRandom(month);
	if (next != type) {
		type = next;
		changed = true;
	}
	remainingMs = DrawIntervalMs();
}

bool Weather::SetByName(const string& name) {
	Type next;
	if (!FromName(name, next)) {
		debugf("Warning: unknown weather \"%s\" in change_weather, ignored.\n", name.c_str());
		return false;
	}
	if (next != type) {
		type = next;
		changed = true;
	}
	remainingMs = DrawIntervalMs();
	return true;
}

bool Weather::ConsumeChanged() {
	bool result = changed;
	changed = false;
	return result;
}
