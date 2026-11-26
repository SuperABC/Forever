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

#ifdef _DEBUG
#define MOD_TEST
#endif // DEBUG


using ValueType = std::variant<int, double, bool, std::string>;

// 下面几个转换函数的非数值/非bool分支都有各自的既定取值(不是"未定义行为"，调用方可以依赖)：
// ToInt/ToDouble对string分支返回0，ToBool对空string返回false、非空string返回true。
int ToInt(const ValueType& value);
double ToDouble(const ValueType& value);
bool ToBool(const ValueType& value);
std::string ToString(const ValueType& value);
// Container::FindValue返回的是指针。指针能隐式转成bool再构造出ValueType，所以ToInt(指针)能编译过、结果恒为1；
// 这里把指针版本直接禁掉，忘了写*会在编译期报错。
int ToInt(const ValueType* value) = delete;
double ToDouble(const ValueType* value) = delete;
bool ToBool(const ValueType* value) = delete;
std::string ToString(const ValueType* value) = delete;

// 自动推断字面量类型(bool/int/double/string)解析成ValueType，脚本里写时间/数值字面量时用。
ValueType FromString(const std::string& s);

// 键值容器接口。实现在宿主(Script)，mod通过虚函数调用，所以ValueType不能按值过这条边界：
// 值是长字符串时按值传/按值返回就是"一侧分配、另一侧释放"(CONVENTIONS.md第4节)。
class Container {
public:
	virtual ~Container() = default;

	// 返回容器自己持有的那个值的指针，没有这个变量返回nullptr。只读；要留着用就自己拷一份(拷贝发生在调用方这一侧)。
	// 指针在这个变量被删掉之前有效，但同名SetValue之后指向的是新值。
	virtual const ValueType* FindValue(const std::string& name) const = 0;
	// 容器自己拷一份value存起来。
	virtual void SetValue(const std::string& name, const ValueType& value) = 0;
};

// ---- 日志(设计见utility.md"debugf") ----
// Dependence/Core/Basic和各mod里不带UE的代码统一用debugf打日志。它只管格式化，文本交给当前的输出函数(sink)：
// 默认写标准错误(CoreTest/ModTest这类控制台程序直接能看到)，宿主可以用SetDebugSink换掉(UE里接到UE_LOG，进Forever.log)。
enum class DebugLevel : int { Log = 0, Warning = 1, Error = 2 };
// text是一条已经格式化好的消息(调用方习惯以换行结尾)。普通函数指针，可以跨DLL传，见mod_concepts.h的BindModHost。
using DebugSink = void(*)(DebugLevel level, const char* text);
// 换输出函数；传nullptr恢复默认(写标准错误)。只影响调用方所在模块自己那份Dependence。
void SetDebugSink(DebugSink sink);
DebugSink GetDebugSink();
void debugf(DebugLevel level, const char* format, ...);

// [0, range-1]的随机整数，range<=0时返回0。
int GetRandom(int range);

// 正态分布随机浮点数——和GetRandom(int)同样风格，每次调用现场构造引擎，不做静态/线程局部
// 优化。
float GetRandomNormal(float mean, float stddev);

class Time {
public:
	Time(); // 所有字段初始化为无效时间(year=0)
	Time(bool online); // online=true时直接取当前系统时间
	Time(int y, int mon = 1, int d = 1, int h = 0, int min = 0, int s = 0, int ms = 0);

	// 支持ISO 8601、中文(YYYY年MM月DD日)、美式(MM/DD/YYYY)及纯时间等多种字符串格式，
	// 自动识别，供story脚本里写时间字面量时随意选一种熟悉的格式，见utility.md。
	Time(std::string time);

	bool IsValid() const; // year > 0

	int GetYear() const;
	int GetMonth() const; // 1-12
	int GetDay() const; // 1-31
	int GetHour() const; // 0-23
	int GetMinute() const; // 0-59
	int GetSecond() const; // 0-59
	float GetOnlySecond() const; // 当天总秒数(含毫秒)
	int GetMillisecond() const; // 0-999

	void SetYear(int y);
	void SetMonth(int m);
	void SetDay(int d);
	void SetHour(int h);
	void SetMinute(int m);
	void SetSecond(int s);
	void SetMillisecond(int ms);
	void SetDate(int y, int m, int d);
	void SetTime(int h, int m, int s, int ms = 0);
	void SetToCurrentTime();

	// 下面几个Add*都按对应单位累加、自动向上一级进位(可传负数向下借位)。
	void AddYears(int years);
	void AddMonths(int months);
	void AddDays(int days);
	void AddHours(int hours);
	void AddMinutes(int minutes);
	void AddSeconds(int seconds);
	void AddMilliseconds(int ms);

	std::string ToString(bool showDate = true, bool showTime = true) const;

	// format支持YYYY/MM/DD/HH/mm/ss/zzz占位符。
	std::string Format(const std::string& format) const;

	bool operator==(const Time& other) const;
	bool operator<(const Time& other) const;
	bool operator>(const Time& other) const;
	bool operator<=(const Time& other) const;
	bool operator>=(const Time& other) const;
	bool operator!=(const Time& other) const;
	Time operator+(const Time& other) const; // 各字段分别累加
	Time operator-(const Time& other) const; // 各字段分别相减
	Time& operator+=(const Time& other);
	Time& operator-=(const Time& other);

	// other比this晚返回正数；唯一调用方Traffic::Tick依赖这个方向累加elapsedSeconds，
	// 符号反了会导致它永远不前进，原因见utility.cpp里DifferenceInSeconds实现处的注释和utility.md。
	double DifferenceInSeconds(const Time& other) const;

	bool IsLeapYear() const;
	int DayOfWeek() const; // 0=周日
	std::string DayOfWeekName() const;

	static int DaysInMonth(int year, int month);
	static int DaysInYear(int year); // 365或366
	static int DaysBetweenYears(int startYear, int endYear);
	static int DaysBetween(const Time& start, const Time& end);

private:
	int year;
	int month; // 1-12
	int day; // 1-31
	int hour; // 0-23
	int minute; // 0-59
	int second; // 0-59
	int millisecond; // 0-999

	void Validate() const; // 各字段范围不合法时抛OutOfRangeException/InvalidArgumentException
	void NormalizeTime(); // 字段进位/借位归一化(比如second=65归一化成minute+1,second=5)
	int OrdinalDate() const; // 当年第几天
};

// 在[begin,end]范围内随机采样一天，返回值的时间部分(时/分/秒/毫秒)归零到当天0点。
Time GetRandom(Time begin, Time end);

// 倒计时计数器
class Counter {
public:
	Counter(int count);

	// 递减一次，归零(或已经是0)时返回true。
	bool Count();

private:
	int num;
};
