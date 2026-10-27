# PvP模型重训(从零)：编译环境 -> 冒烟测试 -> 训练 -> 导出权重 -> 评估各式。
# 用法(在有GPU的机器上，本目录下)：
#   powershell -ExecutionPolicy Bypass -File train_pvp.ps1 -Iters 2000                训练2000轮
#   powershell -ExecutionPolicy Bypass -File train_pvp.ps1 -Iters 1000 -Resume        接着checkpoints_pvp\latest.pt再训1000轮
#   powershell -ExecutionPolicy Bypass -File train_pvp.ps1 -Iters 2000 -EvalEvery 100 -EvalGames 512 -ReportGames 200
# 参数：
#   -Iters N         这次训练多少轮(默认2000；续训时是"再训N轮")
#   -Hours H         最多训练多少小时，0=不限(默认0)；和-Iters谁先到谁停
#   -EvalEvery N     训练中每N轮评测一次(对四种脚本机器人)并存检查点(默认50)
#   -EvalGames N     训练中每次评测：对每种机器人、每个席位打多少盘(默认256，越大越准越慢)
#   -ReportGames N   训练结束后招式强度报告(eval_report.py)每个对局组合打多少盘(默认100)
#   -Games / -Steps  每轮采样的对局数/每盘最多回合数(默认1024/80)
# 产物：checkpoints_pvp\latest.pt(检查点)、Online\public\ai_weights_pvp.js 和 Resource\act\ai_weights_pvp.bin(导出的权重)、report_<式>.txt(强度评估)
param(
    [int]$Iters = 2000,
    [double]$Hours = 0,
    [int]$EvalEvery = 50,
    [int]$EvalGames = 256,
    [int]$ReportGames = 100,
    [int]$Games = 1024,
    [int]$Steps = 80,
    [switch]$Resume,
    [switch]$SkipBuild,
    [switch]$SkipSmoke,
    [switch]$SkipEval
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

$resumeArg = @()
if ($Resume) { $resumeArg = @("--resume") }
python train.py --games $Games --steps $Steps --pool 3000 --iters $Iters --hours $Hours --eval_every $EvalEvery --eval_games $EvalGames @resumeArg
Check "train.py"

python export_weights.py checkpoints_pvp\latest.pt
Check "export_weights.py"

if (-not $SkipEval) {
    # 评估报告：-W ignore 去掉Python警告；这一段不让stderr里的东西触发"Stop"(PowerShell把2>&1合并的stderr当错误)
    $ErrorActionPreference = "Continue"
    foreach ($f in 1, 5, 10, 15, 20) {
        python -W ignore eval_report.py checkpoints_pvp\latest.pt $f --games $ReportGames 2>&1 | ForEach-Object { "$_" } | Where-Object { $_ -notmatch "Warning|torch.load" } | Out-File -Encoding utf8 "report_$f.txt"
    }
    python make_report_all.py
    $ErrorActionPreference = "Stop"
}
Write-Host "done. checkpoints_pvp\latest.pt, reports: report_*.txt"
