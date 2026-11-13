// Cloudflare Worker 入口 + 一个全局的 Durable Object(Lobby)：
//   /ws        -> 升级为 WebSocket，交给 Lobby 做匹配与转发(逻辑在 lobby_core.js)
//   其它路径   -> 静态页面(public/，由 wrangler 的 assets 托管)
import { DurableObject } from 'cloudflare:workers';
import { onOpen, onMessage, onClose } from './lobby_core.js';

export class Lobby extends DurableObject {
  constructor(ctx, env) {
    super(ctx, env);
    // 客户端的心跳 {"t":"ping"} 由平台直接回 pong，不唤醒 Durable Object(休眠期间不计时长)
    this.ctx.setWebSocketAutoResponse(new WebSocketRequestResponsePair('{"t":"ping"}', '{"t":"pong"}'));
    this.core = {
      all: () => this.ctx.getWebSockets().filter(ws => ws.readyState === 1),
      getAtt: ws => ws.deserializeAttachment() || {},
      setAtt: (ws, o) => { try { ws.serializeAttachment(o); } catch (e) { /* 连接已关闭 */ } },
      send: (ws, obj) => { try { ws.send(JSON.stringify(obj)); } catch (e) { /* 对端已断开 */ } },
      close: ws => { try { ws.close(1000, 'bye'); } catch (e) { /* 已关闭 */ } },
      rand: Math.random,
    };
  }

  async fetch(request) {
    if (request.headers.get('Upgrade') !== 'websocket') return new Response('expected websocket', { status: 426 });
    const pair = new WebSocketPair();
    const [client, server] = Object.values(pair);
    this.ctx.acceptWebSocket(server);          // 使用 Hibernation API：连接空闲时 DO 可休眠
    server.serializeAttachment({ state: 'new' });
    onOpen(this.core, server);
    return new Response(null, { status: 101, webSocket: client });
  }

  async webSocketMessage(ws, data) {
    let msg = null;
    try { msg = JSON.parse(typeof data === 'string' ? data : new TextDecoder().decode(data)); } catch (e) { return; }
    onMessage(this.core, ws, msg);
  }

  async webSocketClose(ws) {
    onClose(this.core, ws);
    try { ws.close(); } catch (e) { /* 已关闭 */ }
  }

  async webSocketError(ws) {
    onClose(this.core, ws);
  }
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    if (url.pathname === '/ws') {
      // 所有玩家进同一个 Lobby 实例：先到的等待，后到的配对
      const id = env.LOBBY.idFromName('main');
      return env.LOBBY.get(id).fetch(request);
    }
    return env.ASSETS.fetch(request);
  },
};
