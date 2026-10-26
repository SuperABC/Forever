# 五行斗诀 联机对战（测试版）

打开链接就自动匹配：已经有人在等就直接配对，否则等待；每回合双方都出完招才结算；一局结束后选"重新开始 / 退出"。
全部跑在 **Cloudflare 免费计划**上（静态页面 + WebSocket 匹配转发），不用租服务器。离线版（`../Local/wxdj_simulator.html` 等）完全没动，只被读取。

## 结构
```
public/index.html      页面（精简版界面）
public/online.js       联机流程 + 界面逻辑（手写）
public/engine.js       战斗引擎+默认招式表【生成物，勿手改】
src/index.js           Worker 入口 + Durable Object "Lobby"（WebSocket Hibernation）
src/lobby_core.js      匹配/转发/重开/退出逻辑（纯逻辑，Worker 和本地测试页共用）
tools/build_engine.py  从离线版抽出引擎生成 public/engine.js
tools/test_harness.html 本地测试页：在页面里用 lobby_core.js 模拟服务端，两个 iframe 各是一个玩家
wrangler.jsonc         Cloudflare 配置（assets 目录、Durable Object 绑定、SQLite 迁移）
```

## 工作原理
- 服务端不含战斗规则，只做：配对（先到的等待=座位0，后到的=座位1）、攒齐双方出招后**同时**下发（先出的一方看不到对方的招）、重开/退出/断线通知、核对双方每回合的状态摘要（不一致时在战报里提示，仅诊断用）。
- 双方浏览器各自运行同一份引擎（`engine.js`），用服务端下发的同一个随机种子（mulberry32）按座位顺序结算，所以结果一致。引擎里战斗下标固定为 `[座位0, 座位1]`，界面里"我"永远在左边。
- 双方都用 4-6 阶位标准属性和默认招式表（离线版内置的 xiaohua.txt），出招界面和离线版一致（滑块选第几式）。
- 取舍：因为是各自结算，技术上可以通过改代码作弊。测试阶段可以接受；以后要排行榜再把引擎搬到服务端结算。

## 同步离线版的规则/招式表
离线版改了规则或招式表（先跑 `../Local/sync_actions.py` 同步它内置的招式表）后，重新生成引擎再部署：
```
python tools/build_engine.py
```

## 本地测试（不需要 Node）
在本目录起一个静态服务器，打开测试页：
```
python -m http.server 8780
# 浏览器打开 http://127.0.0.1:8780/tools/test_harness.html
```
页面里有两个玩家 iframe：自动配对；"自动对打一整局"会让双方随机出招并逐回合核对两边状态；"连打20局"连续对打并测试重新开始。

## 部署到 Cloudflare（一次性）
需要：Node.js LTS（本机已装 v24；刚装完的终端要重开才有 PATH，或把 Node 目录（C:\Program Files\nodejs）加进 PATH）和一个免费的 Cloudflare 账号。
```
cd Forever_Mod/Wxdj/Tools/Online
npm install
npx wrangler login          # 浏览器里登录 Cloudflare
npm run dev                 # 本地完整测试：打开 http://localhost:8787 ，用两个浏览器标签页互相匹配（已用这个方式实测过）
npm run deploy              # 部署；账号子域是 wxdj，地址固定为 https://wxdj-online.wxdj.workers.dev（脚本结束时会再打印一遍）
```
把这个链接发给对方，两人各自打开就会自动匹配。想换名字改 `wrangler.jsonc` 里的 `name`。

## 免费额度
Workers 免费计划每天 10 万次请求；WebSocket 消息按约 20:1 折算成请求，Durable Object 使用 Hibernation API，连接空闲时不计时长，测试规模远低于上限。

## 协议（JSON over WebSocket）
客户端→服务端：`act{a}` / `again` / `quit` / `sync{r,h}` / `ping`；服务端→客户端：`waiting` / `matched{seat,seed}` / `round{acts}` / `restart{seed}` / `opp_quit` / `desync{r}` / `pong`。
出招描述 `a`：`{t:'skip'|'acc'|'bomb'}`、`{t:'single',i}`、`{t:'dual',i,j}`（`i/j` 是招式表下标）。
