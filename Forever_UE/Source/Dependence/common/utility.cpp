#include "utility.h"
#include "error.h"

#include <ctime>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <random>
#include <regex>
#include <cstdarg>
#include <cstdio>
#include <vector>

#include <windows.h>


using namespace std;

void debugf(const char* format, ...) {
	va_list args;
	va_start(args, format);
	int len = _vscprintf(format, args);
	vector<char> buffer(len + 1);
	vsprintf_s(buffer.data(), buffer.size(), format, args);
	OutputDebugStringA(buffer.data());
	va_end(args);
}

int GetRandom(int range) {
	// 范围无效时直接返回0
	if (range <= 0)return 0;

	mt19937 rng(random_device{}());
	uniform_int_distribution<int> dist(0, range - 1);
	int ret = dist(rng);

	return ret;
}

Time::Time() :
	year(0),
	month(1),
	day(1),
	hour(0),
	minute(0),
	second(0),
	millisecond(0) {

}

Time::Time(bool online) :
	year(0),
	month(1),
	day(1),
	hour(0),
	minute(0),
	second(0),
	millisecond(0) {
	if (online) {
		SetToCurrentTime();
	}
}

Time::Time(int y, int mon, int d, int h, int min, int s, int ms)
	: year(y), month(mon), day(d), hour(h), minute(min), second(s), millisecond(ms) {
	NormalizeTime();
}

Time::Time(string time) :
	year(0),
	month(1),
	day(1),
	hour(0),
	minute(0),
	second(0),
	millisecond(0) {
	time.erase(0, time.find_first_not_of(" \t\n\r\f\v"));
	time.erase(time.find_last_not_of(" \t\n\r\f\v") + 1);

	// 去除首尾空白后若为空串则直接返回
	if (time.empty()) {
		return;
	}

	// 尝试匹配 ISO 8601 格式: YYYY-MM-DD HH:MM:SS.sss
	regex iso_pattern(R"(^(\d{4})-(\d{1,2})-(\d{1,2})(?:[T ](\d{1,2}):(\d{1,2}):(\d{1,2})(?:\.(\d{1,3}))?)?$)");
	smatch match;

	if (regex_match(time, match, iso_pattern)) {
		year = stoi(match[1]);
		month = stoi(match[2]);
		day = stoi(match[3]);

		// 如果有时间部分
		if (match[4].matched) {
			hour = stoi(match[4]);
			minute = stoi(match[5]);
			second = stoi(match[6]);

			if (match[7].matched) {
				millisecond = stoi(match[7]);
				// 补齐毫秒位数
				if (match[7].length() == 1) millisecond *= 100;
				else if (match[7].length() == 2) millisecond *= 10;
			}
		}

		NormalizeTime();
		return;
	}

	// 尝试匹配常见的中文格式: YYYY年MM月DD日 HH:MM:SS
	regex chinese_pattern(R"(^(\d{4})年(\d{1,2})月(\d{1,2})日(?:\s+(\d{1,2}):(\d{1,2}):(\d{1,2})(?:\.(\d{1,3}))?)?$)");

	if (regex_match(time, match, chinese_pattern)) {
		year = stoi(match[1]);
		month = stoi(match[2]);
		day = stoi(match[3]);

		if (match[4].matched) {
			hour = stoi(match[4]);
			minute = stoi(match[5]);
			second = stoi(match[6]);

			if (match[7].matched) {
				millisecond = stoi(match[7]);
				if (match[7].length() == 1) millisecond *= 100;
				else if (match[7].length() == 2) millisecond *= 10;
			}
		}

		NormalizeTime();
		return;
	}

	// 尝试匹配美国格式: MM/DD/YYYY HH:MM:SS
	regex us_pattern(R"(^(\d{1,2})/(\d{1,2})/(\d{4})(?:\s+(\d{1,2}):(\d{1,2}):(\d{1,2})(?:\.(\d{1,3}))?)?$)");

	if (regex_match(time, match, us_pattern)) {
		month = stoi(match[1]);
		day = stoi(match[2]);
		year = stoi(match[3]);

		if (match[4].matched) {
			hour = stoi(match[4]);
			minute = stoi(match[5]);
			second = stoi(match[6]);

			if (match[7].matched) {
				millisecond = stoi(match[7]);
				if (match[7].length() == 1) millisecond *= 100;
				else if (match[7].length() == 2) millisecond *= 10;
			}
		}

		NormalizeTime();
		return;
	}

	// 尝试只包含时间的格式: HH:MM:SS.sss
	regex day_pattern(R"(^(\d{1,2}):(\d{1,2}):(\d{1,2})(?:\.(\d{1,3}))?$)");

	if (regex_match(time, match, day_pattern)) {
		hour = stoi(match[1]);
		minute = stoi(match[2]);
		second = stoi(match[3]);

		if (match[4].matched) {
			millisecond = stoi(match[4]);
			if (match[4].length() == 1) millisecond *= 100;
			else if (match[4].length() == 2) millisecond *= 10;
		}

		NormalizeTime();
		return;
	}

	// 如果所有格式都不匹配，抛出异常
	THROW_EXCEPTION(InvalidArgumentException, "无法解析时间字符串: " + time);
}

bool Time::IsValid() const {
	return year > 0;
}

void Time::SetYear(int y) {
	year = y; Validate();
}
void Time::SetMonth(int m) {
	month = m; Validate();
}
void Time::SetDay(int d) {
	day = d; Validate();
}
void Time::SetHour(int h) {
	hour = h; Validate();
}
void Time::SetMinute(int m) {
	minute = m; Validate();
}
void Time::SetSecond(int s) {
	second = s; Validate();
}
void Time::SetMillisecond(int ms) {
	millisecond = ms; Validate();
}

void Time::SetDate(int y, int m, int d) {
	year = y; month = m; day = d;
	Validate();
}

void Time::SetTime(int h, int m, int s, int ms) {
	hour = h; minute = m; second = s; millisecond = ms;
	Validate();
}

int Time::GetYear() const {
	return year;
}
int Time::GetMonth() const {
	return month;
}
int Time::GetDay() const {
	return day;
}
int Time::GetHour() const {
	return hour;
}
int Time::GetMinute() const {
	return minute;
}
int Time::GetSecond() const {
	return second;
}
float Time::GetOnlySecond() const {
	return hour * 3600.f + minute * 60.f + second + millisecond / 1000.f;
}
int Time::GetMillisecond() const {
	return millisecond;
}

void Time::SetToCurrentTime() {
	auto now = chrono::system_clock::now();
	auto now_time = chrono::system_clock::to_time_t(now);
	auto ms = chrono::duration_cast<chrono::milliseconds>(
		now.time_since_epoch()) % 1000;

	struct tm tm;
	time_t t = time(nullptr);
	// 成功获取本地时间时写入各字段
	if (localtime_s(&tm, &t) == 0) {
		year = tm.tm_year + 1900;
		month = tm.tm_mon + 1;
		day = tm.tm_mday;
		hour = tm.tm_hour;
		minute = tm.tm_min;
		second = tm.tm_sec;
		millisecond = static_cast<int>(ms.count());
	}
}

void Time::AddYears(int years) {
	year += years;
}

void Time::AddMonths(int months) {
	month += months;
	// 向前进位：月份超过12时年份加一
	while (month > 12) {
		month -= 12;
		year++;
	}
	// 向后进位：月份小于1时年份减一
	while (month < 1) {
		month += 12;
		year--;
	}
}

void Time::AddDays(int days) {
	day += days;
	// 天数超出当月上限时进位到下一月
	while (day > DaysInMonth(year, month)) {
		day -= DaysInMonth(year, month);
		AddMonths(1);
	}
	// 天数小于1时借位到上一月
	while (day < 1) {
		AddMonths(-1);
		day += DaysInMonth(year, month);
	}
}

void Time::AddHours(int hours) {
	hour += hours;
	NormalizeTime();
}

void Time::AddMinutes(int minutes) {
	minute += minutes;
	NormalizeTime();
}

void Time::AddSeconds(int seconds) {
	second += seconds;
	NormalizeTime();
}

void Time::AddMilliseconds(int ms) {
	millisecond += ms;
	NormalizeTime();
}

string Time::ToString(bool showDate, bool showTime) const {
	string format;
	// 根据参数拼接日期/时间格式字符串
	if (showDate) format += "YYYY-MM-DD";
	if (showDate && showTime) format += " ";
	if (showTime) format += "HH:mm:ss.zzz";
	return Format(format);
}

string Time::Format(const string& format) const {
	ostringstream oss;
	for (size_t i = 0; i < format.size(); i++) {
		if (format[i] == 'Y') {
			if (format.substr(i, 4) == "YYYY") {
				oss << setw(4) << setfill('0') << year;
				i += 3;
			}
			else if (format.substr(i, 2) == "YY") {
				oss << setw(2) << setfill('0') << (year % 100);
				i += 1;
			}
		}
		else if (format[i] == 'M') {
			if (format.substr(i, 2) == "MM") {
				oss << setw(2) << setfill('0') << month;
				i += 1;
			}
		}
		else if (format[i] == 'D') {
			if (format.substr(i, 2) == "DD") {
				oss << setw(2) << setfill('0') << day;
				i += 1;
			}
		}
		else if (format[i] == 'H') {
			if (format.substr(i, 2) == "HH") {
				oss << setw(2) << setfill('0') << hour;
				i += 1;
			}
		}
		else if (format[i] == 'm') {
			if (format.substr(i, 2) == "mm") {
				oss << setw(2) << setfill('0') << minute;
				i += 1;
			}
		}
		else if (format[i] == 's') {
			if (format.substr(i, 2) == "ss") {
				oss << setw(2) << setfill('0') << second;
				i += 1;
			}
		}
		else if (format[i] == 'z') {
			if (format.substr(i, 3) == "zzz") {
				oss << setw(3) << setfill('0') << millisecond;
				i += 2;
			}
		}
		else {
			oss << format[i];
		}
	}
	return oss.str();
}

bool Time::operator==(const Time& other) const {
	return year == other.year &&
		month == other.month &&
		day == other.day &&
		hour == other.hour &&
		minute == other.minute &&
		second == other.second &&
		millisecond == other.millisecond;
}

bool Time::operator<(const Time& other) const {
	if (year != other.year) return year < other.year;
	if (month != other.month) return month < other.month;
	if (day != other.day) return day < other.day;
	if (hour != other.hour) return hour < other.hour;
	if (minute != other.minute) return minute < other.minute;
	if (second != other.second) return second < other.second;
	return millisecond < other.millisecond;
}

bool Time::operator>(const Time& other) const { return other < *this; }
bool Time::operator<=(const Time& other) const { return !(*this > other); }
bool Time::operator>=(const Time& other) const { return !(*this < other); }
bool Time::operator!=(const Time& other) const { return !(*this == other); }

Time Time::operator+(const Time& other) const {
	Time result = *this;
	result.AddYears(other.year);
	result.AddMonths(other.month - 1);
	result.AddDays(other.day - 1);
	result.AddHours(other.hour);
	result.AddMinutes(other.minute);
	result.AddSeconds(other.second);
	result.AddMilliseconds(other.millisecond);
	return result;
}

Time Time::operator-(const Time& other) const {
	Time result = *this;
	result.AddYears(-other.year);
	result.AddMonths(-(other.month - 1));
	result.AddDays(-(other.day - 1));
	result.AddHours(-other.hour);
	result.AddMinutes(-other.minute);
	result.AddSeconds(-other.second);
	result.AddMilliseconds(-other.millisecond);
	return result;
}

Time& Time::operator+=(const Time& other) {
	*this = *this + other;
	return *this;
}

Time& Time::operator-=(const Time& other) {
	*this = *this - other;
	return *this;
}

double Time::DifferenceInSeconds(const Time& other) const {
	if (*this == other) return 0.0;

	const Time* earlier = this;
	const Time* later = &other;
	bool inverted = false;
	// 确保 earlier <= later，记录是否发生了反转
	if (*this > other) {
		earlier = &other;
		later = this;
		inverted = true;
	}

	int yearDays = 0;
	// 若跨年则先累计年份间的总天数
	if (earlier->year != later->year) {
		yearDays = DaysBetweenYears(earlier->year, later->year);
	}

	int earlierDayOfYear = earlier->OrdinalDate();
	int laterDayOfYear = later->OrdinalDate();

	int dayDifference = laterDayOfYear - earlierDayOfYear + yearDays;

	int secondsEarlier = earlier->hour * 3600 + earlier->minute * 60 + earlier->second;
	int secondsLater = later->hour * 3600 + later->minute * 60 + later->second;

	double totalSeconds = dayDifference * 86400.0 + (secondsLater - secondsEarlier);

	totalSeconds += (later->millisecond - earlier->millisecond) / 1000.0;

	return inverted ? totalSeconds : -totalSeconds;
}

bool Time::IsLeapYear() const {
	return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int Time::DayOfWeek() const {
	int m = month;
	int y = year;
	// Zeller公式：1月和2月当作上一年的13、14月处理
	if (m < 3) {
		m += 12;
		y--;
	}
	int q = day;
	int k = y % 100;
	int j = y / 100;
	int h = (q + 13 * (m + 1) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
	return (h + 6) % 7;
}

string Time::DayOfWeekName() const {
	static const string names[] = {
		"Sunday", "Monday", "Tuesday", "Wednesday",
		"Thursday", "Friday", "Saturday"
	};
	return names[DayOfWeek()];
}

void Time::Validate() const {
	// 校验月份范围
	if (month < 1 || month > 12)
		THROW_EXCEPTION(OutOfRangeException, "Month must be between 1-12.");
	// 校验日期范围
	if (day < 1 || day > DaysInMonth(year, month))
		THROW_EXCEPTION(OutOfRangeException, "Invalid day for given month and year.");
	// 校验小时范围
	if (hour < 0 || hour > 23)
		THROW_EXCEPTION(OutOfRangeException, "Hour must be between 0-23.");
	// 校验分钟范围
	if (minute < 0 || minute > 59)
		THROW_EXCEPTION(OutOfRangeException, "Minute must be between 0-59.");
	// 校验秒数范围
	if (second < 0 || second > 59)
		THROW_EXCEPTION(OutOfRangeException, "Second must be between 0-59.");
	// 校验毫秒范围
	if (millisecond < 0 || millisecond > 999)
		THROW_EXCEPTION(OutOfRangeException, "Millisecond must be between 0-999.");
}

int Time::DaysInMonth(int year, int month) {
	static const int days[] = {
		31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
	};
	if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
		return 29;
	}
	return days[month - 1];
}

int Time::DaysInYear(int year) {
	return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) ? 366 : 365;
}

int Time::DaysBetweenYears(int startYear, int endYear) {
	// 起始年大于结束年时递归反转计算
	if (startYear > endYear) return -DaysBetweenYears(endYear, startYear);

	int totalDays = 0;
	for (int year = startYear; year < endYear; year++) {
		totalDays += DaysInYear(year);
	}
	return totalDays;
}

int Time::DaysBetween(const Time& start, const Time& end) {
	// 确保开始日期不晚于结束日期
	if (start > end) {
		return -DaysBetween(end, start);
	}

	// 计算年份差的天数
	int totalDays = 0;
	for (int year = start.GetYear(); year < end.GetYear(); year++) {
		totalDays += Time::DaysInYear(year);
	}

	// 计算开始日期在当年的天数
	Time startYearStart(start.GetYear(), 1, 1);
	int startDays = (start - startYearStart).GetDay();

	// 计算结束日期在当年的天数
	Time endYearStart(end.GetYear(), 1, 1);
	int endDays = (end - endYearStart).GetDay();

	return totalDays + endDays - startDays;
}

void Time::NormalizeTime() {
	// 毫秒进位/借位到秒
	while (millisecond >= 1000) {
		millisecond -= 1000;
		second++;
	}
	while (millisecond < 0) {
		millisecond += 1000;
		second--;
	}

	// 秒进位/借位到分钟
	while (second >= 60) {
		second -= 60;
		minute++;
	}
	while (second < 0) {
		second += 60;
		minute--;
	}

	// 分钟进位/借位到小时
	while (minute >= 60) {
		minute -= 60;
		hour++;
	}
	while (minute < 0) {
		minute += 60;
		hour--;
	}

	// 小时进位/借位到天
	while (hour >= 24) {
		hour -= 24;
		day++;
	}
	while (hour < 0) {
		hour += 24;
		day--;
	}

	// 月份进位/借位到年
	while (month > 12) {
		month -= 12;
		year++;
	}
	while (month < 1) {
		month += 12;
		year--;
	}

	// 天数超出当月天数时进位到下一月
	while (day > DaysInMonth(year, month)) {
		day -= DaysInMonth(year, month);
		month++;
		// 月份满12时进位到年
		if (month > 12) {
			month = 1;
			year++;
		}
	}
	// 天数小于1时借位到上一月
	while (day < 1) {
		month--;
		// 月份小于1时借位到年
		if (month < 1) {
			month = 12;
			year--;
		}
		day += DaysInMonth(year, month);
	}

	Validate();
}

int Time::OrdinalDate() const {
	int days = day;
	for (int m = 1; m < month; m++) {
		days += DaysInMonth(year, m);
	}
	return days;
}

Time GetRandom(Time begin, Time end) {
	// 矫正起始与结束时间
	if (begin > end) {
		swap(begin, end);
	}
	else if (begin == end) {
		return begin;
	}

	// 将时间归一化到当天0点
	begin.SetTime(0, 0, 0, 0);
	end.SetTime(0, 0, 0, 0);

	// 计算总天数
	int totalDays = Time::DaysBetween(begin, end) + 1;

	// 生成随机偏移天数
	int randomDays = GetRandom(totalDays);

	// 创建结果时间
	Time result = begin;
	result.AddDays(randomDays);

	return result;
}

Counter::Counter(int count) : num(count) {

}

bool Counter::Count() {
	return --num <= 0;
}

int ToInt(const ValueType& value) {
	return visit([](const auto& v) -> int {
		using T = decay_t<decltype(v)>;
		if constexpr (is_same_v<T, int>) return v;
		else if constexpr (is_same_v<T, double>) return static_cast<int>(v);
		else if constexpr (is_same_v<T, bool>) return v ? 1 : 0;
		else return 0;
	}, value);
}

double ToDouble(const ValueType& value) {
	return visit([](const auto& v) -> double {
		using T = decay_t<decltype(v)>;
		if constexpr (is_same_v<T, int>) return static_cast<double>(v);
		else if constexpr (is_same_v<T, double>) return v;
		else if constexpr (is_same_v<T, bool>) return v ? 1.0 : 0.0;
		else return 0.0;
	}, value);
}

bool ToBool(const ValueType& value) {
	return visit([](const auto& v) -> bool {
		using T = decay_t<decltype(v)>;
		if constexpr (is_same_v<T, int>) return v != 0;
		else if constexpr (is_same_v<T, double>) return v != 0.0;
		else if constexpr (is_same_v<T, bool>) return v;
		else return !v.empty();
	}, value);
}

string ToString(const ValueType& value) {
	return visit([](const auto& v) -> string {
		using T = decay_t<decltype(v)>;
		if constexpr (is_same_v<T, int>) {
			return to_string(v);
		}
		else if constexpr (is_same_v<T, double>) {
			string str = to_string(v);
			size_t dot_pos = str.find('.');
			if (dot_pos != string::npos) {
				size_t last_non_zero = str.find_last_not_of('0');
				if (last_non_zero != string::npos && last_non_zero > dot_pos) {
					if (str[last_non_zero] == '.') {
						str = str.substr(0, last_non_zero);
					}
					else {
						str = str.substr(0, last_non_zero + 1);
					}
				}
			}
			return str;
		}
		else if constexpr (is_same_v<T, bool>) {
			return v ? "true" : "false";
		}
		else if constexpr (is_same_v<T, string>) {
			return v;
		}
		else {
			return "unknown_type";
		}
	}, value);
}

ValueType FromString(const string& s) {
	// 空字符串直接返回空string
	if (s.empty()) {
		return string("");
	}

	// 识别布尔字面量
	if (s == "true") {
		return true;
	}

	if (s == "false") {
		return false;
	}

	// 识别引号包裹的字符串字面量
	if (s.length() >= 2 &&
		((s.front() == '"' && s.back() == '"') ||
			(s.front() == '\'' && s.back() == '\''))) {
		return s.substr(1, s.length() - 2);
	}

	// 含非ASCII字符时直接返回字符串
	for (char c : s) {
		if (static_cast<unsigned char>(c) > 127) {
			return s;
		}
	}

	try {
		size_t pos;
		int int_val = stoi(s, &pos);
		if (pos == s.length()) {
			return int_val;
		}
	}
	catch (const exception&) {}

	try {
		size_t pos;
		double double_val = stod(s, &pos);
		if (pos == s.length()) {
			return double_val;
		}
	}
	catch (const exception&) {}

	// 判断是否纯数字字符串
	bool is_all_digit = !s.empty();
	for (char c : s) {
		if (!isdigit(static_cast<unsigned char>(c))) {
			is_all_digit = false;
			break;
		}
	}

	// 纯数字：尝试解析为int，过长或前导零则保留为string
	if (is_all_digit) {
		if ((s.length() > 1 && s[0] == '0') || s.length() > 10) {
			return s;
		}
		try {
			return stoi(s);
		}
		catch (const exception&) {
			return s;
		}
	}

	// 数字开头：尝试识别浮点数格式
	if (!s.empty() && isdigit(static_cast<unsigned char>(s[0]))) {
		bool has_non_digit = false;
		bool has_dot = false;
		bool has_exponent = false;

		for (size_t i = 0; i < s.length(); i++) {
			char c = s[i];
			if (!isdigit(static_cast<unsigned char>(c))) {
				if (c == '.' && !has_dot) {
					has_dot = true;
				}
				else if ((c == 'e' || c == 'E') && !has_exponent) {
					has_exponent = true;
					if (i + 1 < s.length() && (s[i + 1] == '+' || s[i + 1] == '-')) {
						i++;
					}
				}
				else {
					has_non_digit = true;
					break;
				}
			}
		}

		if (!has_non_digit && (has_dot || has_exponent)) {
			try {
				return stod(s);
			}
			catch (const exception&) {}
		}
	}

	return s;
}
