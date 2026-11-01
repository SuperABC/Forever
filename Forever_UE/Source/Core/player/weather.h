#pragma once

#include <string>

// 全局天气状态，由Player持有(和游戏时钟同域)。只管"现在是什么天气"和"什么时候自动换"，
// 表现(云/雨雪/风/光照)全在UE层的UForeverWeatherFrameworkComponent。详见weather.md。
class Weather {
	// 存档：PlayerSaveCodec直接读写私有字段(player_save.md)。
	friend class PlayerSaveCodec;
public:
	enum class Type {
		Sunny,
		Cloudy,
		Overcast,
		LightRain,
		HeavyRain,
		LightSnow,
		HeavySnow,
	};

	Weather();

	// 天气名 <-> 枚举。名字是脚本/事件里用的字符串：sunny/cloudy/overcast/light_rain/heavy_rain/
	// light_snow/heavy_snow。FromName认不出返回false且不改out。
	static const char* ToName(Type type);
	static bool FromName(const std::string& name, Type& out);

	Type GetType() const { return type; }
	const char* GetName() const { return ToName(type); }

	// 推进自动换天气的倒计时，到点就随机换一次天气并重抽下一次的间隔。
	// @deltaMs: 本次推进的游戏毫秒数
	// @month: 当前游戏月份(1-12)，决定能不能下雪/下雨
	void Tick(long long deltaMs, int month);

	// 脚本强制换天气(change_weather)：名字合法才生效，同时重置自动换天气的倒计时(重新抽一个间隔)。
	// 和当前天气相同也算一次"换"(倒计时照样重置，但不标记变化)。名字不合法返回false。
	bool SetByName(const std::string& name);

	// 自上次取走之后天气变过就返回true并清标记。UE层每帧调用，据此广播weather_change事件。
	bool ConsumeChanged();

private:
	// 按月份和权重随机抽一个和当前不同的天气。
	Type PickRandom(int month) const;
	// 下一次自动换天气的间隔(游戏毫秒)：均值2天、标准差半天的正态分布，下限3个游戏小时。
	static long long DrawIntervalMs();

	Type type = Type::Sunny;
	// 距下一次自动换天气还剩多少游戏毫秒
	long long remainingMs = 0;
	bool changed = false;
};
