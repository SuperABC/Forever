# ForeverAvatarSubsystem（市民头像加载）

市民头像是磁盘上的 128x128 PNG，运行时按需读成 `UTexture2D` 贴到头像方块上，**不导入成 UE 资产**。

## 文件布局

`Resource/Avatars/male/00000.png … 09999.png`、`Resource/Avatars/female/…`，由 `Tools/AvatarGen` 生成（生成物不进 git，见该目录 README）。
`Resource/` 打包时是 junction（`pak.bat`），放进去自动随包，不需要 cook/NonUFS 配置。
**每张图的年龄标注在文件名里**：`a{年龄:3位}_{编号:5位}.png`，如 `male/a023_01234.png`。子系统启动时扫描每个性别目录，按文件名里的年龄建索引（不读图），所以不需要清单文件。
脚本指定的自定义头像放在同目录下任意子路径，如 `Resource/Avatars/custom/hero.png`（路径里不允许 `..`）。

## 选哪一张

`GetCitizenAvatar(citizen)`：
1. `Citizen::GetAvatar()` 非空 → 读 `Resource/Avatars/<avatar>.png`；以 `/` 开头则当作UE资产对象路径（如mod插件里的 `/Test/Yuanshen/kong.kong`），用 `LoadObject<UTexture2D>` 加载；找不到打警告并回退；
2. 自动 → 在**该性别、该年龄段**的头像集合里挑：年龄 = 当前游戏年份（`Player::GetTime()`，没有时退回 `Populace::GetCurrentYear()`）− 出生年，落到年龄段（3-5、6-12、13-17、18-29、30-44、45-59、60-74、75+，边界必须和 `Tools/AvatarGen/generate.mjs` 的 `BANDS` 一致，代码里是 `kBandMax`），再用 `Citizen::GetAvatarIndex() % 段内张数` 取一张。`avatarIndex` 在 Core 里随机并随存档保存，所以同一个市民读档前后头像不变；年龄在段内增长时头像不变，跨段才换（"长大了"）。该段没有图时取年龄最接近的一张。
没有头像目录/文件读取失败 → 返回 nullptr，调用方（`ACitizenElement::Init`）保持角色默认头像。

## 缓存

LRU，最多 512 张。市民按距离流式生成/销毁，附近市民反复进出范围时命中缓存。贴图被角色的 `avatarTexture`（UPROPERTY）引用着，缓存淘汰不会让正在用的贴图消失，只是之后再取要重新读盘。失败过的路径记在 `missing` 里不再重试。
读盘 + 解码用 `FImageUtils::ImportBufferAsTexture2D`，单张 128x128 开销很小，在 `ACitizenElement::Init` 里同步完成。
