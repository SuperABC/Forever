# PvE模型训练：从PvP模型热启动(输入层补零列)，双方带随机的五行属性/暴击/特性，特征在PvP维数后面追加。
# 先训好PvP模型(train_pvp.ps1，得到checkpoints_pvp\latest.pt)，再跑这个。检查点单独存在checkpoints_pve\，不会覆盖PvP的。
# 用法：
#   powershell -ExecutionPolicy Bypass -File train_pve.ps1 -Iters 600                  训练600轮，从checkpoints_pvp\latest.pt出发
#   powershell -ExecutionPolicy Bypass -File train_pve.ps1 -Iters 300 -Resume          接着checkpoints_pve\latest.pt再训300轮
#   powershell -ExecutionPolicy Bypass -File train_pve.ps1 -Iters 600 -EvalEvery 100 -EvalGames 512
# 参数：
#   -Iters N         这次训练多少轮(默认600；续训时是"再训N轮")
#   -Hours H         最多训练多少小时，0=不限(默认0)；和-Iters谁先到谁停
#   -EvalEvery N     训练中每N轮评测一次(对四种脚本机器人，双方配置一样)并存检查点(默认50)
#   -EvalGames N     训练中每次评测：对每种机器人、每个席位打多少盘(默认256)
#   -Lr X            学习率(默认1.5e-4，热启动微调用比从零训练小的值)
#   -Init 路径       热启动用的PvP检查点(默认checkpoints_pvp\latest.pt)
#   -Games / -Steps  每轮采样的对局数/每盘最多回合数(默认1024/80)
# 产物：checkpoints_pve\latest.pt、Online\public\ai_weights_pve.js 和 Resource\act\ai_weights_pve.bin
param(
    [int]$Iters = 600,
    [double]$Hours = 0,
    [int]$EvalEvery = 50,
    [int]$EvalGames = 256,
    [double]$Lr = 1.5e-4,
    [string]$Init = "checkpoints_pvp\latest.pt",
    [int]$Games = 1024,
    [int]$Steps = 80,
    [switch]$Resume,
    [switch]$SkipBuild,
    [switch]$SkipSmoke
)
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $here
$env:PYTHONIOENCODING = "utf-8"
# Python按UTF-8输出，PowerShell默认按系统代码页(GBK)解码管道，中文会变乱码，所以输入输出编码都设成UTF-8
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

function Check($what) { if ($LASTEXITCODE -ne 0) { Write-Host "FAILED: $what"; exit 1 } }

if (-not $SkipBuild) {
    powershell -ExecutionPolicy Bypass -File build.ps1
    Check "build.ps1"
}
if (-not $SkipSmoke) {
    python smoke_test.py
    Check "smoke_test.py"
}

if ($Resume) {
    python train.py --mode pve --resume --games $Games --steps $Steps --pool 3000 --iters $Iters --hours $Hours --lr $Lr --eval_every $EvalEvery --eval_games $EvalGames
}
else {
    if (-not (Test-Path $Init)) { Write-Host "找不到PvP检查点 $Init：先跑 train_pvp.ps1，或用 -Init 指定(检查点是Git LFS文件，记得 git lfs pull)"; exit 1 }
    python train.py --mode pve --init $Init --games $Games --steps $Steps --pool 3000 --iters $Iters --hours $Hours --lr $Lr --eval_every $EvalEvery --eval_games $EvalGames
}
Check "train.py"

python export_weights.py --pve checkpoints_pve\latest.pt
Check "export_weights.py --pve"
Write-Host "done. checkpoints_pve\latest.pt"
