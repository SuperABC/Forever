# EconSim

不接UE的独立经济模拟器,只通过Core的接口生成城市并模拟市民和组织。

## 构建与运行
1. 先编`Source/Framework.sln`(Release|x64),得到`x64/Release/`下的`Core.lib`/`Dependence.lib`/`Basic.dll`。
2. 编`EconSim.vcxproj`(Release|x64),得到`x64/Release/EconSim.exe`。
3. 运行:`EconSim.exe config.json [地图边长] [天数/月数] [report [报表路径]]`。
   `Basic.dll`经`config.json`的`dll_paths`被`Registry`按真实流程加载;`config.json`里的路径是本机绝对路径,换机器要改。

**新增文件时必须同步加进`EconSim.vcxproj`和`.filters`**。

## 目录
- 代码:`main.cpp`/`report.*`/`EconSim.vcxproj`/`config.json`。
- 报告:统一放`reports/`(已在仓库根`.gitignore`里忽略,不进git),建议用`EconSim.exe config.json 1024 24 report reports/xxx.txt`,目录不存在时程序会自动创建。
