#!/bin/sh
# 编译训练环境(Linux/macOS)：生成 libwxenv.so(wxenv.py会按平台找它)。Windows请用build.ps1。
# 直接编译上级目录的battle/源码(规则和系数与游戏一致)，改了battle/之后重新跑一次。
cd "$(dirname "$0")"
B=../../battle
g++ -O2 -std=c++17 -shared -fPIC -pthread -I $B -o libwxenv.so env.cpp $B/action.cpp $B/controller.cpp $B/cultivator.cpp $B/effect.cpp $B/modifier.cpp && echo "OK: libwxenv.so"
