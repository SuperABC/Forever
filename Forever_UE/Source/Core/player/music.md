# MusicTracks（音乐音轨）

`Player`持有一份(`Player::GetMusic()`)。按**轨道名**管理任意多条互相独立的音轨，每条轨道同一时间放一个音乐资产。

## 谁来改、谁来播

- **改**：两个入口，同一套操作(`MusicTracks::Apply(op, track, asset, loop, position)`)：
  - post handle：`{"post":"music","track":"…","op":"play|pause|resume|stop|seek","asset":"…","loop":true,"position":12.5}`，
    返回`success`或`fail`+`msg`(见`Core/common/implement.md`)；
  - change：`{"type":"music", "track", "op", "asset", "loop", "position"}`，字段都是表达式，`Player::ApplyChange`求值后调`Apply`
    (见`Dependence/story/change.md`)。
- **播**：Core只记"应该是什么样"，不发命令。UE的`UForeverMusicFrameworkComponent`每帧对照一遍：Core有它没有、或者`generation`变了
  → 从`position`开始(重新)放；`paused`变了 → 暂停/继续；Core里没了 → 停掉。播放中的实际进度、时长、状态再用`Report`写回
  (见`Source/Forever/Framework/ForeverMusicFrameworkComponent.md`)。所以`position`/`duration`/`status`是UE写的，存档拿到的是真实进度。
- **查**：post `{"post":"music status","track":"…"}`返回`asset`/`state`(loading/playing/paused/ended/error)/`position`/`duration`/`loop`。

## 操作语义

| op | 效果 |
|---|---|
| `play` | 新建或整条替换这个轨道，从0秒开始。必须给`asset`；`loop`只看这次给的值 |
| `pause` / `resume` | 只改`paused`，不动`generation`(UE原地暂停，进度不丢) |
| `stop` | 删除这个轨道 |
| `seek` | 改`position`(钳到`[0, duration]`，duration还不知道时不钳上限)，保持暂停状态，`generation`+1；已结束的轨道跳转后重新变成播放中 |

没有这个轨道(play以外)、play没给asset、op不认识 → 返回非空错误串(post返回fail，change打一条Warning)。

`asset`和对话语音同一套规则：以`/`开头是UE声音资产路径(`/Game/...`或mod插件的`/<Mod>/...`)，否则是`resource_paths`里的音频文件名
(含扩展名，wav/ogg/mp3，可以是中文名)。

不循环的轨道放完后状态是`ended`，轨道**不会**自己删掉(进度停在末尾)，谁要接着放下一首由谁决定(比如Music app)。

## generation

每次`play`/`seek`取一个新的全局递增值。UE侧只靠"和上次开始播时的generation是否相同"判断要不要重新开始，所以连续两次`play`同一首也会从头放。

## 存档

追加在`world.player`段末尾(没升版本，旧档读到段尾就当没有音轨)：轨道数，然后每条 名字/asset/loop/paused/position。
读档时每条都取新的generation，UE第一帧对照时就从存下来的进度接着放；状态先是默认的加载中，duration由UE回写。
