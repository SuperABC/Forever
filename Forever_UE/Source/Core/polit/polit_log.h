#pragma once

#include "class.h"

#include <functional>
#include <string>


// 政治域日志：Core里所有政治事件只调PolitLog，由调用方决定落点(CoreTest写stdout/polit.log，UE接UE_LOG)。
// 每行格式 "[polit] <游戏时间> <类别> key=value ..."，机器可grep。没设sink时丢弃。见polit_log.md。
using PolitLogSink = std::function<void(const std::string&)>;

void SetPolitLogSink(PolitLogSink sink);
bool HasPolitLogSink();

void PolitLog(const Time& now, const std::string& category, const std::string& body);

// 日志里的小数统一格式(保留3位)。
std::string PolitNum(double value);
