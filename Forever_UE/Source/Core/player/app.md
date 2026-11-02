# app.h / app.cpp

## 职责

`App`：手机里的一个App实例，**薄包装类**，持有`AppMod*`+`AppFactory*`，方法(`Init`/`Loop`/`Back`/`Refresh`)直接转发给mod。

## 关键设计

- 照抄`Core/player/puzzle.h`的写法：构造时`factory->CreateApp(id)`拿到mod，析构时`factory->DestroyApp(mod)`，保证跨DLL的`new`/`delete`成对。
- 由`Phone::AppEntry`独占持有，多个App实例互不干扰；**画布共享**，所有App用`Phone`持有的同一块`Canvas`，`App`只是转发，不持有画布所有权。
- 禁止默认构造。

## 依赖关系

- 依赖：`player/app_mod.h`、`player/app_factory.h`。
- 被谁依赖：`Core/player/phone.h`；具体App在`Source/Basic/player/app_basic.*`。

## 待办/后续阶段

- 无。
