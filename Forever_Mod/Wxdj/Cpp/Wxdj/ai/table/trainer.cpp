#define _ALLOW_KEYWORD_MACROS 1
// 通用对战AI训练框架：输入任意招式表 -> 自博弈训练 -> 输出策略文件；
// 同一份代码也提供平衡评测(m/duel/report模式)。直接编译游戏的battle/源码，规则和系数与游戏一致。
// 用法和策略文件格式见README.md。
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <random>
#include <thread>
#include <vector>
#include <array>
#include <string>
#include <map>
#include <memory>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <unordered_map>
#define private public
#include "cultivator.h"
#include "controller.h"
#include "action.h"
#undef private
using namespace std; using namespace wxdj;

static int HP=1100, ATKV=1200, MPV=1700, AGI=21, REC=42;
static int HPA=-1, ATKA=-1, MPA=-1, RECA=-1, AGIA=-1;   // overrides for fighter A (duel mode)

// ====== 状态抽象 ======
// 第一层：类别 0跳过 1积攒 2释放炸弹 3单招 4双招，条件于 state(324)；
// 第二层：具体招式(单招取1个，双招取2个)，条件于 ctx(32)=自己真气档(4)x对手上一招属性(8)。
// 没有任何"真气不够就跳过"之类的硬编码，什么时候出招、什么时候等回复由学习得到。
static const int NC=5, NS=4*3*3*3*3, NCTX=4*8;
static int stateOf(const Cultivator& me, const Cultivator& opp, int oppAcc){
  double mpf=(double)me.currentMP/me.maxMP;
  int mpb = mpf<0.088?0: mpf<0.294?1: mpf<0.588?2:3;
  int bb = me.bomb<=0?0: (double)me.bomb/me.maxMP<=0.235?1:2;
  auto vit=[](const Cultivator&c){ double h=(double)c.currentHP/c.maxHP,a=(double)c.currentATK/c.maxATK; return min(h,a); };
  double mv=vit(me), ov=vit(opp);
  int mb = mv>0.66?0: mv>0.33?1:2, ob = ov>0.66?0: ov>0.33?1:2;
  return (((mpb*3+bb)*3+mb)*3+ob)*3+min(oppAcc,2);
}
static int ctxOf(const Cultivator& me,int oppAttr){
  double mpf=(double)me.currentMP/me.maxMP;
  int mpb = mpf<0.088?0: mpf<0.294?1: mpf<0.588?2:3;
  return mpb*8+oppAttr;
}
struct State { int oppAcc=0; int oppAttr=7; };
static void observe(State& st,const Action& oppAct){
  if(oppAct.GetType()==ACTION_ACCUMULATE) st.oppAcc++;
  else if(oppAct.GetType()==ACTION_BOMB) st.oppAcc=0;
  if(oppAct.GetType()==ACTION_SINGLE||oppAct.GetType()==ACTION_DUAL) st.oppAttr=(int)oppAct.GetAttribute();
  else st.oppAttr=7;
}

struct Smart {
  int n=0;
  vector<array<double,NC>> p1;          // NS x NC
  vector<vector<double>> p2s, p2d;      // NCTX x n
  void initUniform(int nn){ n=nn; p1.assign(NS,array<double,NC>()); for(auto&p:p1)p.fill(1.0/NC);
    p2s.assign(NCTX,vector<double>(n,1.0/n)); p2d=p2s; }
};
static const vector<array<double,NC>>* g_frozenP1=nullptr;   // 非空时：第一层(类别)策略冻结为该表，只训练第二层(具体招式)
enum { ST_AI, ST_SMART };
struct Strat { int type=ST_AI; vector<double> w; const Smart* sm=nullptr; };

static int pickW(const vector<double>& w, mt19937_64& g){
  double s=0; for(double x:w) s+=x;
  double r=uniform_real_distribution<double>(0,s)(g);
  for(size_t i=0;i<w.size();++i){ r-=w[i]; if(r<0) return (int)i; }
  return (int)w.size()-1;
}
static int pickP(const array<double,NC>& p, mt19937_64& g){
  double r=uniform_real_distribution<double>(0,1)(g);
  for(int i=0;i<NC;++i){ r-=p[i]; if(r<0) return i; }
  return NC-1;
}
struct Choice { int cls=0,i=-1,j=-1,s=0,ctx=0; };
static shared_ptr<Action> build(Cultivator& c,const Choice& ch){
  switch(ch.cls){
  case 0: return make_shared<SkipAction>();
  case 1: return make_shared<AccumulateAction>();
  case 2: return make_shared<BombAction>();
  case 3: return c.GetAction(ch.i);
  default: return c.GetAction(ch.i,ch.j);
  }
}
static Choice sample(const Smart& sm,const Cultivator& me,const Cultivator& opp,const State& st,mt19937_64& g){
  Choice ch; ch.s=stateOf(me,opp,st.oppAcc); ch.ctx=ctxOf(me,st.oppAttr);
  ch.cls=pickP(sm.p1[ch.s],g);
  if(ch.cls==3) ch.i=pickW(sm.p2s[ch.ctx],g);
  else if(ch.cls==4){ ch.i=pickW(sm.p2d[ch.ctx],g); ch.j=pickW(sm.p2d[ch.ctx],g); }
  return ch;
}
static shared_ptr<Action> choose(Cultivator& c,const Cultivator& opp,const Strat& s,const State& st,mt19937_64& g){
  int n=(int)c.GetActions().size();
  if(s.type==ST_AI){   // 游戏里敌方AI的原样随机策略
    if(uniform_int_distribution<int>(0,3)(g)){
      vector<double> w=s.w; w.push_back(3); w.push_back(1); w.push_back(2);
      int k=pickW(w,g);
      if(k<n) return c.GetAction(k);
      if(k==n) return make_shared<AccumulateAction>();
      if(k==n+1) return make_shared<BombAction>();
      return make_shared<SkipAction>();
    }
    return c.GetAction(pickW(s.w,g),pickW(s.w,g));
  }
  return build(c,sample(*s.sm,c,opp,st,g));
}
static double loss(const Cultivator& c){
  if(!c.IsAlive()) return 1.0;
  double h=(double)c.currentHP/c.maxHP, a=(double)c.currentATK/c.maxATK;
  double l=1.0-(h+a)/2.0; return l<0?0:l;
}
static double fight(const Cultivator& tmplA,const Cultivator& tmplB,const Strat& sa,const Strat& sb,int T,mt19937_64& g,int* rounds=nullptr,int* outcome=nullptr){
  auto a=make_shared<Cultivator>(tmplA), b=make_shared<Cultivator>(tmplB);
  Controller ct({a,b}); State xa,xb; int r=0;
  for(;(T<=0||r<T) && r<120 && a->IsAlive() && b->IsAlive();++r){
    ct.StartTurn();
    auto x=choose(*a,*b,sa,xa,g), y=choose(*b,*a,sb,xb,g);
    ct.ActionTurn({{x,1},{y,0}}); ct.CheckTurn({{x,1},{y,0}}); ct.EndTurn();
    ct.logs.clear(); ct.mark=0;
    observe(xa,*y); observe(xb,*x);
  }
  if(rounds) *rounds=r;
  if(outcome) *outcome = (a->IsAlive()&&!b->IsAlive())?1 : (!a->IsAlive()&&b->IsAlive())?-1 : 0;
  return loss(*b)-loss(*a);
}
static double run(const Cultivator& tmplA,const Cultivator& tmplB,const Strat& sa,const Strat& sb,int N,int T,int threads,unsigned seed){
  vector<double> part(threads,0.0); vector<thread> th;
  for(int t=0;t<threads;++t) th.emplace_back([&,t]{
    mt19937_64 g(seed*1000003ULL+t*7919ULL+12345); double s=0; int cnt=N/threads;
    for(int i=0;i<cnt;++i) s+=fight(tmplA,tmplB,sa,sb,T,g);
    part[t]=s/cnt; });
  for(auto&x:th) x.join();
  double s=0; for(double x:part) s+=x; return s/threads;
}

// ====== 训练：虚拟对弈(对手=最近K个策略快照的随机混合)的表格式MC控制 ======
struct Q {
  int n;
  vector<array<double,NC>> q1; vector<vector<double>> q2s,q2d; vector<double> v;   // v: 每个state的平均回报(基线)
  Q(int nn):n(nn),q1(NS),q2s(NCTX,vector<double>(nn,0.0)),q2d(NCTX,vector<double>(nn,0.0)),v(NS,0.0){ for(auto&a:q1)a.fill(0.0); }
};
struct Acc { vector<array<double,NC>> s1,c1; vector<vector<double>> s2s,c2s,s2d,c2d; vector<double> vs,vc;
  Acc(int n):s1(NS),c1(NS),s2s(NCTX,vector<double>(n,0.0)),c2s(NCTX,vector<double>(n,0.0)),s2d(NCTX,vector<double>(n,0.0)),c2d(NCTX,vector<double>(n,0.0)),vs(NS,0.0),vc(NS,0.0){
    for(auto&a:s1)a.fill(0.0); for(auto&a:c1)a.fill(0.0);} };

static void softmaxRow(const double* q,int len,double tau,double* out){
  double mx=-1e18; for(int i=0;i<len;++i) mx=max(mx,q[i]); double z=0;
  for(int i=0;i<len;++i){ out[i]=exp((q[i]-mx)/tau); z+=out[i]; } for(int i=0;i<len;++i) out[i]/=z;
}
static Smart snapshot(const Q& q,double tau1,double tau2){
  Smart sm; sm.initUniform(q.n);
  for(int s=0;s<NS;++s) softmaxRow(q.q1[s].data(),NC,tau1,sm.p1[s].data());
  for(int c=0;c<NCTX;++c){ softmaxRow(q.q2s[c].data(),q.n,tau2,sm.p2s[c].data()); softmaxRow(q.q2d[c].data(),q.n,tau2,sm.p2d[c].data()); }
  return sm;
}
static int egreedy(const double* q,int len,double eps,mt19937_64& g){
  if(uniform_real_distribution<double>(0,1)(g)<eps) return uniform_int_distribution<int>(0,len-1)(g);
  int b=0; for(int i=1;i<len;++i) if(q[i]>q[b]) b=i; return b;
}
static void episode(const Cultivator& tmpl,const Smart& opp,const Q& q,double eps,Acc& acc,mt19937_64& g){
  auto a=make_shared<Cultivator>(tmpl), b=make_shared<Cultivator>(tmpl);
  Controller ct({a,b}); State xa,xb;
  struct Rec{Choice ch;double r;}; vector<Rec> recs; double la=0,lb=0;
  for(int r=0;r<120 && a->IsAlive() && b->IsAlive();++r){
    ct.StartTurn();
    Choice ch; ch.s=stateOf(*a,*b,xa.oppAcc); ch.ctx=ctxOf(*a,xa.oppAttr);
    if(g_frozenP1) ch.cls=pickP((*g_frozenP1)[ch.s],g); else ch.cls=egreedy(q.q1[ch.s].data(),NC,eps,g);
    if(ch.cls==3) ch.i=egreedy(q.q2s[ch.ctx].data(),q.n,eps,g);
    else if(ch.cls==4){ ch.i=egreedy(q.q2d[ch.ctx].data(),q.n,eps,g); ch.j=egreedy(q.q2d[ch.ctx].data(),q.n,eps,g); }
    auto x=build(*a,ch);
    auto y=build(*b,sample(opp,*b,*a,xb,g));
    ct.ActionTurn({{x,1},{y,0}}); ct.CheckTurn({{x,1},{y,0}}); ct.EndTurn();
    ct.logs.clear(); ct.mark=0; observe(xa,*y); observe(xb,*x);
    double na=loss(*a), nb=loss(*b);
    recs.push_back({ch,(nb-lb)-(na-la)}); la=na; lb=nb;
  }
  double G=0;
  for(int k=(int)recs.size()-1;k>=0;--k){
    G+=recs[k].r; const Choice& ch=recs[k].ch;
    acc.vs[ch.s]+=G; acc.vc[ch.s]+=1;
    acc.s1[ch.s][ch.cls]+=G; acc.c1[ch.s][ch.cls]+=1;
    double adv=G-q.v[ch.s];           // 第二层用相对基线的优势，降低方差
    if(ch.cls==3){ acc.s2s[ch.ctx][ch.i]+=adv; acc.c2s[ch.ctx][ch.i]+=1; }
    else if(ch.cls==4){ acc.s2d[ch.ctx][ch.i]+=adv; acc.c2d[ch.ctx][ch.i]+=1; acc.s2d[ch.ctx][ch.j]+=adv; acc.c2d[ch.ctx][ch.j]+=1; }
  }
}
struct TrainCfg { int iters=60, epi=28000, K=20, threads=14; double tau1=0.04, tau2=0.01, eps=0.12; unsigned seed=1; };
static Smart train(const Cultivator& tmpl,const TrainCfg& cfg,Smart* init=nullptr){
  int n=(int)tmpl.actions.size(); Q q(n);
  if(init){   // 热启动：softmax策略 p = exp(Q/tau)/Z  =>  取 Q = tau*ln p
    for(int s=0;s<NS;++s) for(int c=0;c<NC;++c) q.q1[s][c]=cfg.tau1*log(max(init->p1[s][c],1e-6));
    for(int c=0;c<NCTX;++c) for(int i=0;i<n;++i){ q.q2s[c][i]=cfg.tau2*log(max(init->p2s[c][i],1e-6)); q.q2d[c][i]=cfg.tau2*log(max(init->p2d[c][i],1e-6)); }
  }
  vector<Smart> pop; { Smart u; u.initUniform(n); pop.push_back(init?*init:u); }
  for(int it=0;it<cfg.iters;++it){
    vector<Acc> accs; for(int t=0;t<cfg.threads;++t) accs.emplace_back(n);
    vector<thread> th; int from=max(0,(int)pop.size()-cfg.K);
    for(int t=0;t<cfg.threads;++t) th.emplace_back([&,t]{
      mt19937_64 g(cfg.seed*7777ULL+it*131ULL+t*17ULL+1);
      for(int e=0;e<cfg.epi/cfg.threads;++e){
        const Smart& opp=pop[uniform_int_distribution<int>(from,(int)pop.size()-1)(g)];
        episode(tmpl,opp,q,cfg.eps,accs[t],g); } });
    for(auto&x:th) x.join();
    for(int s=0;s<NS;++s){
      double vs=0,vc=0; for(auto&a:accs){ vs+=a.vs[s]; vc+=a.vc[s]; }
      if(vc>0) q.v[s]=0.7*q.v[s]+0.3*(vs/vc);
      for(int c=0;c<NC && !g_frozenP1;++c){ double sum=0,cnt=0; for(auto&a:accs){ sum+=a.s1[s][c]; cnt+=a.c1[s][c]; }
        if(cnt>0){ double al=cnt>200?0.4:0.2; q.q1[s][c]=(1-al)*q.q1[s][c]+al*(sum/cnt); } }
    }
    for(int c=0;c<NCTX;++c) for(int i=0;i<n;++i){
      double sum=0,cnt=0; for(auto&a:accs){ sum+=a.s2s[c][i]; cnt+=a.c2s[c][i]; }
      if(cnt>0) q.q2s[c][i]=0.85*q.q2s[c][i]+0.15*(sum/cnt);
      sum=0;cnt=0; for(auto&a:accs){ sum+=a.s2d[c][i]; cnt+=a.c2d[c][i]; }
      if(cnt>0) q.q2d[c][i]=0.85*q.q2d[c][i]+0.15*(sum/cnt);
    }
    pop.push_back(snapshot(q,cfg.tau1,cfg.tau2));
    if(it%10==9) fprintf(stderr,"iter %d\n",it+1);
  }
  // 最终策略 = 最近K个快照的概率平均
  Smart avg; avg.initUniform(n); int from=max(1,(int)pop.size()-cfg.K), cnt=0;
  for(auto&p:avg.p1) p.fill(0.0); for(auto&r:avg.p2s) fill(r.begin(),r.end(),0.0); for(auto&r:avg.p2d) fill(r.begin(),r.end(),0.0);
  for(int k=from;k<(int)pop.size();++k){ ++cnt;
    for(int s=0;s<NS;++s) for(int c=0;c<NC;++c) avg.p1[s][c]+=pop[k].p1[s][c];
    for(int c=0;c<NCTX;++c) for(int i=0;i<n;++i){ avg.p2s[c][i]+=pop[k].p2s[c][i]; avg.p2d[c][i]+=pop[k].p2d[c][i]; } }
  for(auto&p:avg.p1) for(double&x:p) x/=cnt;
  for(auto&r:avg.p2s) for(double&x:r) x/=cnt; for(auto&r:avg.p2d) for(double&x:r) x/=cnt;
  return avg;
}
static void savePolicy(const Smart& sm,const string& path){
  ofstream f(path); f<<sm.n<<"\n";
  for(int s=0;s<NS;++s){ for(int c=0;c<NC;++c) f<<sm.p1[s][c]<<" "; f<<"\n"; }
  for(int c=0;c<NCTX;++c){ for(int i=0;i<sm.n;++i) f<<sm.p2s[c][i]<<" "; f<<"\n"; }
  for(int c=0;c<NCTX;++c){ for(int i=0;i<sm.n;++i) f<<sm.p2d[c][i]<<" "; f<<"\n"; }
}
// 加载策略；若招式数n与当前表不同，第一层(类别)沿用，第二层(具体招式)重置为均匀
static bool loadPolicy(const string& path,int n,Smart& sm){
  ifstream f(path); if(!f) return false; int fn; f>>fn; sm.initUniform(n);
  for(int s=0;s<NS;++s) for(int c=0;c<NC;++c) if(!(f>>sm.p1[s][c])) return false;
  vector<vector<double>> a(NCTX,vector<double>(fn)), b=a;
  for(int c=0;c<NCTX;++c) for(int i=0;i<fn;++i) f>>a[c][i];
  for(int c=0;c<NCTX;++c) for(int i=0;i<fn;++i) f>>b[c][i];
  if(fn==n){ sm.p2s=a; sm.p2d=b; }
  return true;
}
// 用策略自对弈统计：类别使用率、每个招式的使用次数(单招+双招)、战斗时长
static void report(const Cultivator& tmpl,const Smart& sm,int games,unsigned seed,const Strat& vsAI,int threads){
  int n=(int)tmpl.actions.size(); mt19937_64 g(seed+99);
  double cls[NC]={0}, use[256]={0}, tot=0, rounds=0;
  for(int i=0;i<games;++i){
    auto a=make_shared<Cultivator>(tmpl), b=make_shared<Cultivator>(tmpl); Controller ct({a,b}); State xa,xb; int r=0;
    for(;r<120 && a->IsAlive() && b->IsAlive();++r){ ct.StartTurn();
      Choice ca=sample(sm,*a,*b,xa,g), cb=sample(sm,*b,*a,xb,g);
      for(const Choice* c:{&ca,&cb}){ cls[c->cls]++; tot++; if(c->i>=0) use[c->i]++; if(c->j>=0) use[c->j]++; }
      auto x=build(*a,ca), y=build(*b,cb);
      ct.ActionTurn({{x,1},{y,0}}); ct.CheckTurn({{x,1},{y,0}}); ct.EndTurn(); ct.logs.clear(); ct.mark=0; observe(xa,*y); observe(xb,*x); }
    rounds+=r; }
  printf("selfplay rounds %.2f  class: skip %.3f acc %.3f bomb %.3f single %.3f dual %.3f\n",rounds/games,cls[0]/tot,cls[1]/tot,cls[2]/tot,cls[3]/tot,cls[4]/tot);
  double us=0; for(int i=0;i<n;++i) us+=use[i];
  printf("action usage:"); for(int i=0;i<n;++i) printf(" %d=%.3f",i,us>0?use[i]/us:0); printf("\n");
  Strat s; s.type=ST_SMART; s.sm=&sm;
  printf("vs AI score %.3f\n", run(tmpl,tmpl,s,vsAI,20000,0,threads,seed+5));
}
int main(int argc,char**argv){
  if(argc<7){ fprintf(stderr,"usage: sim3 table mode N T B threads [seed] [key=val...]\n"); return 1; }
  string path=argv[1], mode=argv[2]; int N=atoi(argv[3]), T=atoi(argv[4]); double B=atof(argv[5]); int threads=atoi(argv[6]);
  string policyFile="policy.txt", outFile="policy.txt", initFile; int freeze1=0; TrainCfg cfg; cfg.threads=threads; unsigned seed=1;
  for(int i=7;i<argc;++i){
    string a=argv[i]; size_t e=a.find('=');
    if(e==string::npos){ seed=atoi(a.c_str()); cfg.seed=seed; continue; }
    string k=a.substr(0,e), v=a.substr(e+1);
    if(k=="freeze1") freeze1=atoi(v.c_str()); else if(k=="init") initFile=v; else if(k=="hp") HP=atoi(v.c_str()); else if(k=="atk") ATKV=atoi(v.c_str()); else if(k=="mp") MPV=atoi(v.c_str());
    else if(k=="agi") AGI=atoi(v.c_str()); else if(k=="rec") REC=atoi(v.c_str());
    else if(k=="hpA") HPA=atoi(v.c_str()); else if(k=="atkA") ATKA=atoi(v.c_str()); else if(k=="mpA") MPA=atoi(v.c_str());
    else if(k=="recA") RECA=atoi(v.c_str()); else if(k=="agiA") AGIA=atoi(v.c_str());
    else if(k=="policy") policyFile=v; else if(k=="out") outFile=v; else if(k=="iters") cfg.iters=atoi(v.c_str());
    else if(k=="epi") cfg.epi=atoi(v.c_str()); else if(k=="K") cfg.K=atoi(v.c_str());
    else if(k=="tau1") cfg.tau1=atof(v.c_str()); else if(k=="tau2") cfg.tau2=atof(v.c_str());
  }
  Cultivator tmpl("T",path,HP,ATKV,MPV,AGI,REC);
  int n=(int)tmpl.actions.size();
  Strat uni; uni.type=ST_AI; uni.w.assign(n,1.0);
  if(mode=="learn"){
    Smart init0; Smart* pi=nullptr;
    if(!initFile.empty()){ if(loadPolicy(initFile,n,init0)){ pi=&init0; if(freeze1) g_frozenP1=&init0.p1; } }
    Smart sm=train(tmpl,cfg,pi); savePolicy(sm,outFile); report(tmpl,sm,4000,seed,uni,threads);
  } else if(mode=="report"){
    Smart sm; if(!loadPolicy(policyFile,n,sm)){ fprintf(stderr,"no policy\n"); return 2; } report(tmpl,sm,4000,seed,uni,threads);
  } else if(mode=="m"){      // 在智能策略下，对每个招式做"权重提升"导数测试
    Smart base; if(!loadPolicy(policyFile,n,base)){ fprintf(stderr,"no policy\n"); return 2; }
    Strat u; u.type=ST_SMART; u.sm=&base;
    printf("n=%d\nuniform %.4f\n",n,run(tmpl,tmpl,u,u,N,T,threads,seed));
    for(int x=0;x<n;++x){
      Smart s1=base;
      for(int c=0;c<NCTX;++c){ s1.p2s[c][x]*=B; s1.p2d[c][x]*=B; double z=0; for(double w:s1.p2s[c]) z+=w; for(double&w:s1.p2s[c]) w/=z;
        z=0; for(double w:s1.p2d[c]) z+=w; for(double&w:s1.p2d[c]) w/=z; }
      Strat b1; b1.type=ST_SMART; b1.sm=&s1;
      printf("m %d %s %.4f\n",x,tmpl.actions[x].name.c_str(),run(tmpl,tmpl,b1,u,N,T,threads,seed+x+1)); }
  } else if(mode=="duel"){
    // fighter A (overridden stats) vs fighter B (base stats), both using the same policy; prints A's advantage
    Smart base; if(!loadPolicy(policyFile,n,base)){ fprintf(stderr,"no policy\n"); return 2; }
    Strat u; u.type=ST_SMART; u.sm=&base;
    Cultivator tA("A",path,HPA>0?HPA:HP,ATKA>0?ATKA:ATKV,MPA>0?MPA:MPV,AGIA>0?AGIA:AGI,RECA>0?RECA:REC);
    vector<double> sc(threads,0); vector<double> wn(threads,0), ls(threads,0), rd(threads,0); vector<thread> th;
    for(int t=0;t<threads;++t) th.emplace_back([&,t]{
      mt19937_64 g(seed*1000003ULL+t*7919ULL+777); int cnt=N/threads;
      for(int i=0;i<cnt;++i){ int r=0,o=0; sc[t]+=fight(tA,tmpl,u,u,T,g,&r,&o); rd[t]+=r; if(o>0) wn[t]++; else if(o<0) ls[t]++; } });
    for(auto&x:th) x.join();
    double S=0,W=0,L=0,R=0; int cnt=(N/threads)*threads; for(int t=0;t<threads;++t){ S+=sc[t]; W+=wn[t]; L+=ls[t]; R+=rd[t]; }
    printf("duel score %.4f win %.4f loss %.4f draw %.4f rounds %.2f\n",S/cnt,W/cnt,L/cnt,1.0-(W+L)/cnt,R/cnt);
  }
  return 0;
}
