# AvatarGen（市民头像批量生成）

用 DiceBear 的 avataaars 风格（Pablo Stanley，"Free for personal and commercial use"）程序化拼装卡通头像，输出 128x128 PNG：

```
Forever_UE/Resource/Avatars/male/a023_01234.png      (a + 年龄3位 _ 编号5位)
Forever_UE/Resource/Avatars/female/a067_08123.png
```

男女靠限定发型/胡须区分，肤色/发色/衣服/表情/配饰/背景色在各自集合内按 `性别-编号` 的种子随机，**同一个编号永远生成同一张图**。
游戏里由 `UForeverAvatarSubsystem` 按需读取，见 `Source/Forever/Player/ForeverAvatarSubsystem.md`。

## 用法

需要 Node.js（`winget install OpenJS.NodeJS.LTS`），首次先装依赖：

```
cd Forever_UE/Tools/AvatarGen
npm install
```

每张图带年龄标注，按年龄段（3-5、6-12、13-17、18-29、30-44、45-59、60-74、75-90）分配张数，游戏里按市民年龄去对应性别、年龄段的集合里挑。年龄越小头像画得越小/越稚气，年龄越大越多灰发、眼镜。`BANDS` 的边界要和 `ForeverAvatarSubsystem.cpp` 的 `kBandMax` 保持一致。

出样图（每个年龄段每个性别 N 张拼成对比图，左男右女、每行一个年龄段，不写 Resource）：

```
node generate.mjs --sample 5 --sheet sample.png
```

全量生成（默认每性别 10000 张，约几分钟；输出目录默认 `../../Resource/Avatars`）：

```
node generate.mjs
node generate.mjs --count 5000 --size 128 --out ../../Resource/Avatars
```

调画风就改 `generate.mjs` 里的部件集合（`FEMALE_TOP`/`MALE_TOP`/`SKIN`/`HAIR` 等；重复项=提高权重）。
文件名格式 `a{年龄}_{编号}.png` 不能改，游戏靠它解析年龄；全量生成会先清空 male/female 目录再写。

## 不进 git

`Resource/Avatars/`（约 200MB 生成物）和 `node_modules/` 都在 `.gitignore` 里，只提交脚本；换机器后按上面命令重新生成。
