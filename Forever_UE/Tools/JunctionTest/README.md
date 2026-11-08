# JunctionTest

`Core/traffic/junction.h` 的单测控制台程序：手工摆一个十字路口（4 条腿、各 1 进 1 出车道 + 西腿横道），不需要 Map/Registry。

构建（没有加进 Framework.sln）：

```
MSBuild Tools/JunctionTest/JunctionTest.vcxproj -p:Configuration=Release -p:Platform=x64
x64/Release/JunctionTest.exe
```

返回值 = 失败检查数。用例：冲突表（交叉/平行/分叉/汇入/横道/FindCrosswalk）、互斥+粘性+饿死、先来先走、人优先于更晚的车、租约过期、出口检查与预留、最长连续开放、随机压力（每步检查不变量并排空）。
