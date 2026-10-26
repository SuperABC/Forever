"""通用对战模型：Perceiver式的小Transformer。

输入(都是特征，不含招式编号，所以换招式文件也能用)：
  - 自己的全部招式、对手的全部招式(每个招式一个特征向量，见features.h)
  - 当前局面(双方状态)、最近HK回合的出招记录
结构：一组可学习的"潜变量"依次读取所有输入(交叉注意力)，再互相交流；
然后每个自己的招式读取潜变量得到带上下文的招式表示，用指针网络给每个招式打分。
输出分三层：招类(跳过/积攒/炸弹/单招/双招) -> 第一个招式 -> (双招)第二个招式(必须同式)。
"""
import torch
import torch.nn as nn
import torch.nn.functional as F

import wxenv

NEG = -1e9


class MLP(nn.Module):
    def __init__(self, i, h, o):
        super().__init__()
        self.net = nn.Sequential(nn.Linear(i, h), nn.GELU(), nn.Linear(h, o))

    def forward(self, x):
        return self.net(x)


class Block(nn.Module):
    """潜变量读取输入 -> 潜变量自注意力 -> 前馈"""

    def __init__(self, d, heads):
        super().__init__()
        self.n1, self.n2, self.n3, self.nkv = nn.LayerNorm(d), nn.LayerNorm(d), nn.LayerNorm(d), nn.LayerNorm(d)
        self.cross = nn.MultiheadAttention(d, heads, batch_first=True)
        self.self_attn = nn.MultiheadAttention(d, heads, batch_first=True)
        self.ffn = nn.Sequential(nn.Linear(d, d * 2), nn.GELU(), nn.Linear(d * 2, d))

    def forward(self, lat, kv, kv_pad):
        kvn = self.nkv(kv)
        lat = lat + self.cross(self.n1(lat), kvn, kvn, key_padding_mask=kv_pad, need_weights=False)[0]
        x = self.n2(lat)
        lat = lat + self.self_attn(x, x, x, need_weights=False)[0]
        return lat + self.ffn(self.n3(lat))


class Policy(nn.Module):
    def __init__(self, d=128, latents=16, heads=4, blocks=3):
        super().__init__()
        self.d = d
        self.move_enc = MLP(wxenv.FM, d, d)
        self.role = nn.Parameter(torch.zeros(2, d))
        self.state_enc = MLP(wxenv.SD, d, d)
        self.hist_enc = MLP(wxenv.HD, d, d)
        self.hist_pos = nn.Parameter(torch.zeros(wxenv.HK, d))
        self.kind = nn.Parameter(torch.zeros(3, d))      # 局面 / 记录 / 招式 三种输入的标记
        self.latents = nn.Parameter(torch.randn(latents, d) * 0.02)
        self.blocks = nn.ModuleList([Block(d, heads) for _ in range(blocks)])
        # 招式读取潜变量，得到带上下文的表示
        self.ctx_norm_q, self.ctx_norm_kv = nn.LayerNorm(d), nn.LayerNorm(d)
        self.ctx_attn = nn.MultiheadAttention(d, heads, batch_first=True)
        self.ctx_ffn = nn.Sequential(nn.LayerNorm(d), nn.Linear(d, d * 2), nn.GELU(), nn.Linear(d * 2, d))
        self.out_norm = nn.LayerNorm(d)
        self.value = MLP(d, d, 1)
        self.cls_head = MLP(d, d, 5)
        self.q1, self.k1 = nn.Linear(d, d), nn.Linear(d, d)
        self.q2, self.k2 = nn.Linear(d, d), nn.Linear(d, d)
        self.pick_emb = nn.Linear(d, d)

    def move_embed(self, moves):
        """招式特征(...,FM) -> 招式向量(...,d)。招式在一盘对局里不变，采样时可以提前算好缓存。"""
        return self.move_enc(moves) + self.kind[2]

    def encode(self, state, hist, es, eo, mask_s, mask_o):
        """state(B,SD) hist(B,HK,HD) es/eo(B,M,d)=move_embed的结果 mask_s/mask_o(B,M)=该位置有招式。返回 h(B,d), ctx(B,M,d)"""
        B = state.shape[0]
        st = self.state_enc(state).unsqueeze(1) + self.kind[0]
        hs = self.hist_enc(hist) + self.hist_pos + self.kind[1]
        hist_valid = hist[:, :, 0] > 0.5
        es = es + self.role[0]
        eo = eo + self.role[1]
        kv = torch.cat([st, hs, es, eo], dim=1)
        pad = torch.cat([torch.zeros(B, 1, dtype=torch.bool, device=state.device), ~hist_valid, ~mask_s, ~mask_o], dim=1)
        lat = self.latents.unsqueeze(0).expand(B, -1, -1)
        for blk in self.blocks:
            lat = blk(lat, kv, pad)
        lat = self.out_norm(lat)
        h = lat.mean(1) + st.squeeze(1)
        q = self.ctx_norm_q(es)
        ctx = es + self.ctx_attn(q, self.ctx_norm_kv(lat), self.ctx_norm_kv(lat), need_weights=False)[0]
        ctx = ctx + self.ctx_ffn(ctx)
        return h, ctx

    def first_logits(self, h, ctx, mask_s):
        logits = (torch.einsum('bd,bmd->bm', self.q1(h), self.k1(ctx)) / self.d ** 0.5).float()
        return logits.masked_fill(~mask_s, NEG)

    def second_logits(self, h, ctx, pick, mask2):
        """pick(B,) 第一个招式的下标，mask2(B,M) 允许的第二个招式"""
        emb = ctx[torch.arange(ctx.shape[0], device=ctx.device), pick]
        q = self.q2(h + self.pick_emb(emb))
        logits = (torch.einsum('bd,bmd->bm', q, self.k2(ctx)) / self.d ** 0.5).float()
        return logits.masked_fill(~mask2, NEG)

    @staticmethod
    def same_form_mask(form_s, mask_s, pick):
        """第二招必须和第一招同式；招式名里没有"第X式"(form=-1)不受限制。"""
        f1 = form_s[torch.arange(form_s.shape[0], device=form_s.device), pick].unsqueeze(1)
        return mask_s & ((form_s == f1) | (f1 < 0) | (form_s < 0))

    def forward(self, state, hist, es, eo, mask_s, mask_o, form_s):
        h, ctx = self.encode(state, hist, es, eo, mask_s, mask_o)
        return h, ctx, self.value(h).squeeze(-1).float(), self.cls_head(h).float(), self.first_logits(h, ctx, mask_s).float()

    @torch.no_grad()
    def act(self, state, hist, es, eo, mask_s, mask_o, form_s, greedy=False):
        h, ctx, value, cls_logits, l1 = self.forward(state, hist, es, eo, mask_s, mask_o, form_s)
        pick = (lambda lg: lg.argmax(-1)) if greedy else (lambda lg: torch.distributions.Categorical(logits=lg).sample())
        cls = pick(cls_logits)
        m1 = pick(l1)
        m2 = pick(self.second_logits(h, ctx, m1, self.same_form_mask(form_s, mask_s, m1)))
        lp = self.logprob(cls_logits, l1, cls, m1, None, h, ctx, m2, form_s, mask_s)[0]
        return cls, m1, m2, lp, value

    def logprob(self, cls_logits, l1, cls, m1, _unused, h, ctx, m2, form_s, mask_s):
        lp_cls = F.log_softmax(cls_logits, -1)
        lp1 = F.log_softmax(l1, -1)
        l2 = self.second_logits(h, ctx, m1, self.same_form_mask(form_s, mask_s, m1))
        lp2 = F.log_softmax(l2, -1)
        idx = torch.arange(cls.shape[0], device=cls.device)
        lp = lp_cls[idx, cls] + (cls >= 3).float() * lp1[idx, m1] + (cls == 4).float() * lp2[idx, m2]
        p_cls = lp_cls.exp()
        ent1 = -(lp1.exp() * lp1.clamp(min=-30)).sum(-1)
        ent2 = -(lp2.exp() * lp2.clamp(min=-30)).sum(-1)
        ent = -(p_cls * lp_cls).sum(-1) + (p_cls[:, 3] + p_cls[:, 4]) * ent1 + p_cls[:, 4] * ent2
        return lp, ent


def param_count(model):
    return sum(p.numel() for p in model.parameters())
