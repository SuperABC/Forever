# ModWeb

把真实的 C++ puzzle / app 逻辑(`Source/Basic/player` 的俄罗斯方块、bounce、notes)加上游戏里的 `Phone` 外壳(`Source/Core/player/phone.cpp`)用 Emscripten 编译成 WebAssembly,打包成**单个离线 html**(`dist/forever_mods.html`,约 300 KB,wasm 以 base64 内嵌),拷到手机直接打开即可,不需要服务器或宿主电脑。

## 构建

```
cd Forever_UE\Tools\ModWeb
.emsdk\emsdk install latest      (只需一次;.emsdk 是 git clone https://github.com/emscripten-core/emsdk,已被 .gitignore 忽略)
.emsdk\emsdk activate latest
build_web.bat                    # 产出 dist\forever_mods.html
```

## 使用

- 手机/浏览器打开 `forever_mods.html`,首页选"手机"(App:bounce / notes)或"俄罗斯方块"/"任意键"。
- 手机界面 480x640 居中等比缩放,底部返回/Home/任务三按钮就是 Canvas 里画的那一排;puzzle 铺满整个屏幕。右上角 ☰ 回到菜单。
- 触摸/鼠标即可完整操作;桌面浏览器另支持键盘(方便调试)。

## 结构

- `web_main.cpp`:导出 C 接口(`web_setup / web_start_puzzle / web_start_phone / web_frame / web_key / web_mouse_* / web_pixels`)并静态注册 Basic 的 mod(网页版不走 DLL)。新增 mod 到网页版时在这里加注册和源文件(`build_web.bat`)。
- `../ModHost/host_core.cpp`:和原生 ModHost 共用的宿主(Puzzle / Phone 驱动)。
- `Dependence/player/canvas.cpp`:`#ifdef __EMSCRIPTEN__` 分支用浏览器 2D canvas 画文字(字体 Microsoft YaHei → PingFang SC → Noto Sans CJK 回退),其余绘图仍是同一份 CPU 光栅代码。
- `index.template.html` + `make_html.py`:页面模板,把 emcc 产出的 js 内联进去。
- `Dependence/common/utility.cpp` 依赖 windows.h,网页版不编,`GetRandom` 在 `web_main.cpp` 里提供。

## 注意

- 手机上字体来自设备本身,和 Windows 下的微软雅黑字宽不完全一致。
- 改了 `Basic` 里的 puzzle/app 之后需要重新跑 `build_web.bat` 并重新拷贝 html。
