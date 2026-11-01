# weather.h / weather.cpp

## 职责

全局天气状态，`Player`持有(`Player::GetWeather()`)，和游戏时钟同域。只管"现在是什么天气"和"什么时候自动换"，
**表现(云/光照/风/雨雪)全在UE层**，见`Forever/Framework/ForeverWeatherFrameworkComponent.md`。

## 天气名

`sunny`/`cloudy`/`overcast`/`light_rain`/`heavy_rain`/`light_snow`/`heavy_snow`(晴/多云/阴/小雨/大雨/小雪/大雪)，
`Weather::ToName`/`FromName`在字符串和枚举之间转换。脚本里的`change_weather`/`weather_change`都用这些字符串。

## 自动换天气

- `Player::Tick`按`deltaMs`(游戏毫秒)推进`Weather::Tick(deltaMs, month)`：倒计时到0就随机换一次并重抽间隔。
- **间隔**=`GetRandomNormal(2天, 半天)`的游戏分钟数，下限3个游戏小时(防止负数/过密)。一次Tick最多换一次。
- **随机选择**按权重(晴40/多云25/阴15/小12/大8)抽，抽到和当前相同就重抽(最多8次，再兜底顺延)。"小/大"在冬季(12~2月)
  是雪，其它月份是雨——冬天不下雨、其它季节不下雪。**脚本强制换天气不受季节限制**。
- 初始天气晴天，倒计时是新抽的一个间隔。

## 脚本强制换天气

`change_weather`(`ChangeWeatherChange`，字段`weather`是表达式)由`Player::ApplyChange`处理(Core就认得，不用UE层拦截)：
`Weather::SetByName`校验名字(非法打警告忽略)→设置→**重置倒计时**(重新抽一个间隔)。和当前相同的天气不标记变化，但倒计时照样重置。

## 变化通知

`Weather::ConsumeChanged()`取走"变了"标记。`AForeverFrameworkActor::Tick`每帧调用，变了就
`UForeverStoryFrameworkComponent::BroadcastWeatherChange`广播`weather_change`事件(自动换和脚本换走同一条路)，
再把当前天气交给`UForeverWeatherFrameworkComponent::UpdateWeather`。**读档不广播**(读档恢复不走ConsumeChanged的"变了")。

## 存档

`world.player`段末尾追加：天气类型(U8)+距下次自动换天气的剩余游戏毫秒(F64)。更早的存档没有它，读到末尾就当没有
(晴天+新抽的倒计时)。**没有升段版本号、没写迁移**。

## 依赖关系

- 依赖：`common/utility.h`(`GetRandom`/`GetRandomNormal`/`debugf`)。
- 被谁依赖：`player/player.{h,cpp}`、`player/player_save.cpp`、`Forever/Framework/ForeverFrameworkActor.cpp`、
  `Forever/Framework/ForeverWeatherFrameworkComponent.cpp`(`Forever.SetWeather`控制台命令)。
