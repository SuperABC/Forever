# PvE非对称训练(敌人模型 + 玩家陪练模型)，设计见 train_pve2.py 开头。
# 先训好PvP模型(train_pvp.ps1，得到checkpoints_pvp\latest.pt)，再跑这个；也可以接着已有的 checkpoints_pve\latest.pt。
# 用法：
#   powershell -ExecutionPolicy Bypass -File train_pve2.ps1 -Iters 600                 从PvP(或PvE)检查点补零列热启动，训600轮
#   powershell -ExecutionPolicy Bypass -File train_pve2.ps1 -Iters 300 -Resume         接着 checkpoints_pve_enemy / checkpoints_pve_player 再训300轮
# 参数：
#   -Iters N / -Hours H   训练多少轮 / 最多多少小时(谁先到谁停)
#   -EvalEvery N          每N轮评测一次(敌人模型对5种脚本玩家+玩家模型，k=10/30/100三档)并存检查点(默认50)
#   -EvalGames N          评测每个格子打多少盘(默认128)
#   -KMax X               敌人规模倍数上限的log10(默认2=100倍)；前200轮从10倍慢慢升上去
#   -Init 路径            两个模型共同的热启动检查点
# 产物：checkpoints_pve_enemy\latest.pt(游戏里用的敌人模型)、checkpoints_pve_player\latest.pt(陪练模型)，
#       Online\public\ai_weights_pve.js + Resource\act\ai_weights_pve.bin(敌人模型)，ai_weights_pve_player.js/.bin(玩家陪练模型，网页里测平衡用)
param(
    [int]$Iters = 600,
    [double]$Hours = 0,
    [int]$EvalEvery = 50,
    [int]$EvalGames = 128,
    [double]$KMax = 2.0,
    [double]$Lr = 1.5e-4,
    [string]$Init = "",
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

$args2 = @("train_pve2.py", "--games", $Games, "--steps", $Steps, "--iters", $Iters, "--hours", $Hours, "--lr", $Lr, "--eval_every", $EvalEvery, "--eval_games", $EvalGames, "--kmax", $KMax)
if ($Resume) { $args2 += "--resume" }
if ($Init -ne "") { $args2 += @("--init", $Init) }
python @args2
Check "train_pve2.py"

python export_weights.py --pve checkpoints_pve_enemy\latest.pt
Check "export_weights.py (enemy)"
python export_weights.py --pve checkpoints_pve_player\latest.pt --tag _pve_player
Check "export_weights.py (player)"
Write-Host "done. enemy: checkpoints_pve_enemy\latest.pt  player: checkpoints_pve_player\latest.pt"
