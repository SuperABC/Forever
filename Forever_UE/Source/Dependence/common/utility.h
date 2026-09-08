#pragma once

#include <string>
#include <memory>
#include <set>
#include <mutex>
#include <codecvt>
#include <locale>
#include <exception>

#include <variant>
#include <functional>

#define COSTOM_INIT
#define COSTOM_RUNTIME

#define OBJECT_HOLDER

// Factory 暂存区预留桶容量，避免单批注册触发跨模块的桶数组扩容
#define TEMP_RESERVE_CAPACITY 64

#ifdef _DEBUG
#define MOD_TEST
#endif // DEBUG


using ValueType = std::variant<int, double, bool, std::string>;

/*
* 将ValueType转换为int（string返回0）
* @value: 要转换的变体值
* @return: 转换后的整数
*/
int ToInt(const ValueType& value);

/*
* 将ValueType转换为double（string返回0.0）
* @value: 要转换的变体值
* @return: 转换后的浮点数
*/
double ToDouble(const ValueType& value);

/*
* 将ValueType转换为bool（空string为false，其余string为true）
* @value: 要转换的变体值
* @return: 转换后的布尔值
*/
bool ToBool(const ValueType& value);

/*
* 将ValueType转换为string
* @value: 要转换的变体值
* @return: 转换后的字符串
*/
std::string ToString(const ValueType& value);

/*
* 将string解析为ValueType（自动推断int/double/bool/string）
* @s: 待解析的字符串
* @return: 推断出的变体值
*/
ValueType FromString(const std::string& s);

// 键值容器接口
class Container {
public:
	virtual ~Container() = default;

	/*
	* 获取变量值
	* @name: 变量名
	* @return: {是否存在, 值}
	*/
	virtual std::pair<bool, ValueType> GetValue(const std::string& name) const = 0;

	/*
	* 设置变量值
	* @name: 变量名
	* @value: 要设置的值
	*/
	virtual void SetValue(const std::string& name, ValueType value) = 0;
};

/*
* 调试控制台输出
* @format: 格式字符串（printf风格）
*/
void debugf(const char* format, ...);

/*
* 获取[0, range - 1]的随机整数
* @range: 随机数上界（不含）
* @return: 随机整数
*/
int GetRandom(int range);

class Time {
public:

	/*
	* 默认构造，所有字段初始化为无效时间
	*/
	Time();

	/*
	* 构造当前时间或空时间
	* @online: 为true时获取当前系统时间
	*/
	Time(bool online);

	/*
	* 按指定各字段构造时间
	* @y, mon, d, h, min, s, ms: 年/月/日/时/分/秒/毫秒
	*/
	Time(int y, int mon = 1, int d = 1, int h = 0, int min = 0, int s = 0, int ms = 0);

	/*
	* 从字符串解析时间（支持ISO 8601、中文、美国格式及纯时间格式）
	* @time: 时间字符串
	*/
	Time(std::string time);

	/*
	* 判断是否为有效时间（year > 0）
	* @return: 是否有效
	*/
	bool IsValid() const;

	/*
	* 获取年份
	* @return: 年份
	*/
	int GetYear() const;

	/*
	* 获取月份
	* @return: 月份（1-12）
	*/
	int GetMonth() const;

	/*
	* 获取日
	* @return: 日（1-31）
	*/
	int GetDay() const;

	/*
	* 获取小时
	* @return: 小时（0-23）
	*/
	int GetHour() const;

	/*
	* 获取分钟
	* @return: 分钟（0-59）
	*/
	int GetMinute() const;

	/*
	* 获取秒
	* @return: 秒（0-59）
	*/
	int GetSecond() const;

	/*
	* 获取当天总秒数（含毫秒）
	* @return: 浮点秒数
	*/
	float GetOnlySecond() const;

	/*
	* 获取毫秒
	* @return: 毫秒（0-999）
	*/
	int GetMillisecond() const;

	/*
	* 设置年份
	* @y: 年份
	*/
	void SetYear(int y);

	/*
	* 设置月份
	* @m: 月份（1-12）
	*/
	void SetMonth(int m);

	/*
	* 设置日
	* @d: 日（1-31）
	*/
	void SetDay(int d);

	/*
	* 设置小时
	* @h: 小时（0-23）
	*/
	void SetHour(int h);

	/*
	* 设置分钟
	* @m: 分钟（0-59）
	*/
	void SetMinute(int m);

	/*
	* 设置秒
	* @s: 秒（0-59）
	*/
	void SetSecond(int s);

	/*
	* 设置毫秒
	* @ms: 毫秒（0-999）
	*/
	void SetMillisecond(int ms);

	/*
	* 同时设置年月日
	* @y, m, d: 年/月/日
	*/
	void SetDate(int y, int m, int d);

	/*
	* 同时设置时分秒毫秒
	* @h, m, s, ms: 时/分/秒/毫秒
	*/
	void SetTime(int h, int m, int s, int ms = 0);

	/*
	* 将时间设置为当前系统时间
	*/
	void SetToCurrentTime();

	/*
	* 增加指定年数
	* @years: 年数（可为负）
	*/
	void AddYears(int years);

	/*
	* 增加指定月数（自动进位）
	* @months: 月数（可为负）
	*/
	void AddMonths(int months);

	/*
	* 增加指定天数（自动进位）
	* @days: 天数（可为负）
	*/
	void AddDays(int days);

	/*
	* 增加指定小时数（自动进位）
	* @hours: 小时数（可为负）
	*/
	void AddHours(int hours);

	/*
	* 增加指定分钟数（自动进位）
	* @minutes: 分钟数（可为负）
	*/
	void AddMinutes(int minutes);

	/*
	* 增加指定秒数（自动进位）
	* @seconds: 秒数（可为负）
	*/
	void AddSeconds(int seconds);

	/*
	* 增加指定毫秒数（自动进位）
	* @ms: 毫秒数（可为负）
	*/
	void AddMilliseconds(int ms);

	/*
	* 输出时间字符串
	* @showDate, showTime: 是否输出日期部分, 是否输出时间部分
	* @return: 格式化后的时间字符串
	*/
	std::string ToString(bool showDate = true, bool showTime = true) const;

	/*
	* 按格式字符串输出时间（支持YYYY/MM/DD/HH/mm/ss/zzz占位符）
	* @format: 格式字符串
	* @return: 格式化后的时间字符串
	*/
	std::string Format(const std::string& format) const;

	/*
	* 时间相等比较
	* @other: 另一时间
	* @return: 是否相等
	*/
	bool operator==(const Time& other) const;

	/*
	* 时间小于比较
	* @other: 另一时间
	* @return: 是否小于
	*/
	bool operator<(const Time& other) const;

	/*
	* 时间大于比较
	* @other: 另一时间
	* @return: 是否大于
	*/
	bool operator>(const Time& other) const;

	/*
	* 时间小于等于比较
	* @other: 另一时间
	* @return: 是否小于等于
	*/
	bool operator<=(const Time& other) const;

	/*
	* 时间大于等于比较
	* @other: 另一时间
	* @return: 是否大于等于
	*/
	bool operator>=(const Time& other) const;

	/*
	* 时间不等比较
	* @other: 另一时间
	* @return: 是否不等
	*/
	bool operator!=(const Time& other) const;

	/*
	* 时间相加（各字段累加）
	* @other: 另一时间
	* @return: 相加结果
	*/
	Time operator+(const Time& other) const;

	/*
	* 时间相减（各字段相减）
	* @other: 另一时间
	* @return: 相减结果
	*/
	Time operator-(const Time& other) const;

	/*
	* 时间累加赋值
	* @other: 另一时间
	* @return: 自身引用
	*/
	Time& operator+=(const Time& other);

	/*
	* 时间累减赋值
	* @other: 另一时间
	* @return: 自身引用
	*/
	Time& operator-=(const Time& other);

	/*
	* 计算与另一时间相差的秒数（正数表示other更晚）
	* @other: 另一时间
	* @return: 差值秒数
	*/
	double DifferenceInSeconds(const Time& other) const;

	/*
	* 判断当前年份是否为闰年
	* @return: 是否闰年
	*/
	bool IsLeapYear() const;

	/*
	* 获取星期几（0=周日）
	* @return: 星期索引
	*/
	int DayOfWeek() const;

	/*
	* 获取星期几的英文名称
	* @return: 英文星期名
	*/
	std::string DayOfWeekName() const;

	/*
	* 获取指定年月的天数
	* @year, month: 年份, 月份
	* @return: 该月天数
	*/
	static int DaysInMonth(int year, int month);

	/*
	* 获取指定年份的总天数（365或366）
	* @year: 年份
	* @return: 该年天数
	*/
	static int DaysInYear(int year);

	/*
	* 计算两年份之间的总天数
	* @startYear, endYear: 起始年份, 结束年份
	* @return: 天数差
	*/
	static int DaysBetweenYears(int startYear, int endYear);

	/*
	* 计算两个时间之间的天数差
	* @start, end: 起始时间, 结束时间
	* @return: 天数差
	*/
	static int DaysBetween(const Time& start, const Time& end);

private:

	// 年份
	int year;

	// 月份（1-12）
	int month;

	// 日（1-31）
	int day;

	// 小时（0-23）
	int hour;

	// 分钟（0-59）
	int minute;

	// 秒（0-59）
	int second;

	// 毫秒（0-999）
	int millisecond;

	/*
	* 检查各项数值范围是否合法
	*/
	void Validate() const;

	/*
	* 对各字段进行进位规范化
	*/
	void NormalizeTime();

	/*
	* 计算当前为当年第几天
	* @return: 年内序数日
	*/
	int OrdinalDate() const;
};

/*
* 在指定时间范围内随机采样一天
* @begin, end: 起始时间, 结束时间
* @return: 随机采样的时间（时间部分归零到0点）
*/
Time GetRandom(Time begin, Time end);

// 计数器
class Counter {
public:

	/*
	* 构造计数器
	* @count: 初始计数值
	*/
	Counter(int count);

	/*
	* 递减计数并判断是否到达零
	* @return: 计数器归零返回true
	*/
	bool Count();

private:

	// 剩余计数
	int num;
};
