/* Arena Clash - Clash Royale-style real-time card battler for PSP (pspsdk). 480x272 software render. */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#ifdef PSP
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
PSP_MODULE_INFO("ArenaClash", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
#endif
#define SW 480
#define SH 272
#define AY 224
#define RGB(r,g,b) (0xFF000000u|((b)<<16)|((g)<<8)|(r))
typedef uint32_t u32;
enum { BL = 1, BR = 2, BU = 4, BD = 8, BX = 16, BQ = 32, BE = 64, BS = 128, BO = 256 };

typedef struct { int cost, hp, dmg, bo; float rng, spd; int cd, n; u32 col; } Card;
static const Card C[16] = {  /* cost hp dmg bldgOnly range speed cooldown count color; n==0 -> spell */
  {3,90,11,0,12,.6f,40,1,RGB(190,190,200)}, {3,45,8,0,64,.6f,35,1,RGB(60,200,90)},
  {5,320,20,1,12,.35f,60,1,RGB(150,100,50)}, {3,24,6,0,9,.8f,30,3,RGB(230,200,40)},
  {4,110,14,1,12,1.1f,45,1,RGB(210,90,200)}, {4,55,24,0,72,.5f,70,1,RGB(60,120,230)},
  {2,35,7,0,9,1.2f,25,1,RGB(240,140,40)}, {4,0,90,0,38,0,0,0,RGB(255,80,30)},
  {6,650,28,1,14,.3f,70,1,RGB(120,120,150)}, {5,40,30,0,110,.45f,90,1,RGB(40,160,160)},
  {5,24,6,0,9,.8f,30,5,RGB(200,200,60)}, {3,60,16,0,10,1.f,30,1,RGB(70,70,90)},
  {3,40,30,0,40,.6f,80,1,RGB(200,60,60)}, {4,200,15,0,12,.5f,40,1,RGB(100,160,220)},
  {2,0,45,0,22,0,0,0,RGB(255,230,80)}, {5,150,12,0,50,.4f,8,1,RGB(230,120,60)}};
static const char *N[16] = {"SOLDIER","BOWMAN","BRUTE","SWARM","RIDER","MAGE","SCOUT","BLAST","TITAN","SNIPER","HORDE","NINJA","BOMBER","GUARD","BOLT","INFERNO"};

typedef struct { float x, y; int hp, t, k, cd, on; } U;
typedef struct { float x, y; int hp, t, king, cd; } T;
static U u[128];
static T tw[6];                       /* 0-2 player (princess, princess, king), 3-5 enemy */
static float el[2];
static int q[2][8], tick, over, aicd, sel, cx, cy, flash, fx, fy, prev, cr[2], res, fr, scr, mi, arena, gold, owned[16], deck[8], arOwn[3], gi, dslot, gf;
static u32 fb[SW * SH], bg[SW * AY], rs = 12345;

static int rnd(int n) { rs = rs * 1664525u + 1013904223u; return (rs >> 16) % n; }
static float D(float a, float b, float c, float d) { return sqrtf((a-c)*(a-c) + (b-d)*(b-d)); }
static void rect(int x, int y, int w, int h, u32 c) {
  for (int j = y; j < y + h; j++) if (j >= 0 && j < SH) for (int i = x; i < x + w; i++) if (i >= 0 && i < SW) fb[j*SW + i] = c;
}
static void box(int x, int y, int w, int h, u32 c) { rect(x,y,w,1,c); rect(x,y+h-1,w,1,c); rect(x,y,1,h,c); rect(x+w-1,y,1,h,c); }
static const uint16_t G[10] = {0x7B6F,0x2C97,0x73E7,0x73CF,0x5BC9,0x79CF,0x79EF,0x7249,0x7BEF,0x7BCF};
static void num(int x, int y, int n, int s, u32 c) {
  char b[12]; int l = snprintf(b, 12, "%d", n);
  for (int k = 0; k < l; k++) for (int r = 0; r < 5; r++) for (int cc = 0; cc < 3; cc++)
    if ((G[b[k]-'0'] >> (14 - r*3 - cc)) & 1) rect(x + (k*4 + cc)*s, y + r*s, s, s, c);
}

static void mix(int *a, int n) { for (int i = n-1; i > 0; i--) { int j = rnd(i+1), t = a[i]; a[i] = a[j]; a[j] = t; } }
#ifdef PSP
#define SAVE "ms0:/PSP/GAME/ArenaClash/save.bin"
#else
#define SAVE "save.bin"
#endif
static void save(void) { FILE *f = fopen(SAVE, "wb"); if (!f) return; fwrite(&gold,4,1,f); fwrite(owned,4,16,f); fwrite(deck,4,8,f); fwrite(&arena,4,1,f); fwrite(arOwn,4,3,f); fclose(f); }
static void load(void) {
  for (int i = 0; i < 16; i++) owned[i] = i < 8; for (int i = 0; i < 8; i++) deck[i] = i; arOwn[0] = 1; gold = 100;
  FILE *f = fopen(SAVE, "rb"); if (!f) return;
  if (fread(&gold,4,1,f) && fread(owned,4,16,f) && fread(deck,4,8,f) && fread(&arena,4,1,f) && fread(arOwn,4,3,f)) {} fclose(f);
  if (arena < 0 || arena > 2 || !arOwn[arena]) arena = 0;
}
static const uint16_t L[26] = {0x2BED,0x6BAE,0x3923,0x6B6E,0x79A7,0x79A4,0x396B,0x5BED,0x7497,0x126A,0x5BAD,0x4927,0x5FED,0x6B6D,0x2B6A,0x6BA4,0x2B7B,0x6BAD,0x388E,0x7492,0x5B6F,0x5B6A,0x5BFD,0x5AAD,0x5A92,0x72A7};
static void txt(int x, int y, const char *s, int sc, u32 c) {
  for (int k = 0; s[k]; k++) { int g = 0; if (s[k] >= '0' && s[k] <= '9') g = G[s[k]-'0']; else if (s[k] >= 'A' && s[k] <= 'Z') g = L[s[k]-'A'];
    if (g) for (int r = 0; r < 5; r++) for (int cc = 0; cc < 3; cc++) if ((g >> (14 - r*3 - cc)) & 1) rect(x + (k*4 + cc)*sc, y + r*sc, sc, sc, c); }
}
static void icon(int x, int y, int k, int s, u32 col) {   /* 8x8 pixel-art sprite scaled by s */
  if (!C[k].n) { for (int i = 0; i < 3; i++) rect(x + i*s, y + i*s, (8-2*i)*s, (8-2*i)*s, i == 1 ? 0xFFFFFFFFu : col); return; }
  rect(x, y, 8*s, 8*s, 0xFF303030u); rect(x+s, y+s, 6*s, 6*s, col);
  rect(x+2*s, y+2*s, s, s, 0xFFFFFFFFu); rect(x+5*s, y+2*s, s, s, 0xFFFFFFFFu);
  if (C[k].rng > 30) rect(x+s, y+5*s, 6*s, s, 0xFFFFFFFFu);
  if (C[k].bo) rect(x+2*s, y, 4*s, s, RGB(255,220,60));
}
static const u32 AC[3][4] = {{RGB(58,125,46),RGB(50,112,40),RGB(50,110,200),RGB(140,100,60)},
  {RGB(222,190,120),RGB(206,172,100),RGB(60,150,190),RGB(120,80,50)},
  {RGB(225,235,245),RGB(200,215,232),RGB(120,180,230),RGB(160,130,100)}};
static const char *AN[3] = {"GRASSLAND", "DESERT", "FROZEN"};
static void mkbg(void) {   /* procedural arena texture: checker tiles + noise + river waves + plank bridges */
  const u32 *p = AC[arena];
  for (int y = 0; y < AY; y++) for (int x = 0; x < SW; x++) {
    u32 c = (((x >> 4) + (y >> 4)) & 1) ? p[0] : p[1]; int h = (int)((((uint32_t)x * 73856093u) ^ ((uint32_t)y * 19349663u)) >> 9) & 3;
    if (h == 0) c -= 0x060606; else if (h == 1) c += 0x030303;
    if (x >= 232 && x < 248) { c = p[2]; if (((x + y/3) & 7) < 2) c += 0x101010; }
    bg[y*SW + x] = c;
  }
  for (int i = 0; i < 2; i++) for (int y = (i ? 164 : 60) - 12; y < (i ? 164 : 60) + 12; y++) for (int x = 228; x < 252; x++) bg[y*SW + x] = (x & 3) ? p[3] : p[3] - 0x181818;
}

static void reset(void) {
  float px[3] = {80, 80, 28}, py[3] = {60, 164, 112};
  memset(u, 0, sizeof u);
  for (int s = 0; s < 2; s++) for (int i = 0; i < 3; i++)
    tw[s*3+i] = (T){s ? SW - px[i] : px[i], py[i], i == 2 ? 560 : 340, s, i == 2, 0};
  el[0] = el[1] = 5; tick = over = 0; aicd = 120; sel = 0; cx = 120; cy = 112; flash = 0; res = 0;
  rs += gf * 7919u; { int b[16]; memcpy(q[0], deck, 32); mix(q[0], 8); for (int i = 0; i < 16; i++) b[i] = i; mix(b, 16); memcpy(q[1], b, 32); }
}

static int play(int s, int slot, float x, float y) {
  int k = q[s][slot]; const Card *c = &C[k];
  if (el[s] < c->cost) return 0;
  el[s] -= c->cost;
  if (!c->n) {
    flash = 10; fx = (int)x; fy = (int)y; fr = (int)c->rng;
    for (int i = 0; i < 128; i++) if (u[i].on && u[i].t != s && D(u[i].x, u[i].y, x, y) < c->rng && (u[i].hp -= c->dmg) <= 0) u[i].on = 0;
    for (int i = 0; i < 3; i++) { T *t = &tw[(1-s)*3+i]; if (t->hp > 0 && D(t->x, t->y, x, y) < c->rng + 12) t->hp -= c->dmg / 3; }
  } else for (int j = 0; j < c->n; j++) for (int i = 0; i < 128; i++) if (!u[i].on) { u[i] = (U){x + j*7 - 7, y + (j&1)*7, c->hp, s, k, 0, 1}; break; }
  int n = q[s][slot]; q[s][slot] = q[s][4]; memmove(&q[s][4], &q[s][5], 3*sizeof(int)); q[s][7] = n;
  return 1;
}

static void step(int in) {
  if (over) { if (in & (BS|BX) & ~prev) scr = 0; return; }
  tick++;
  float rate = tick > 7200 ? 1.f/84 : 1.f/168;      /* double elixir in the last minute */
  for (int s = 0; s < 2; s++) { el[s] += rate; if (el[s] > 10) el[s] = 10; }
  if (in & BL) cx -= 2; if (in & BR) cx += 2; if (in & BU) cy -= 2; if (in & BD) cy += 2;
  if (cx < 0) cx = 0; if (cx >= SW) cx = SW-1; if (cy < 0) cy = 0; if (cy >= AY) cy = AY-1;
  if (in & BQ & ~prev) sel = (sel + 3) % 4;
  if (in & BE & ~prev) sel = (sel + 1) % 4;
  if (in & BX & ~prev) { if (!C[q[0][sel]].n || cx < 236) play(0, sel, cx, cy); }
  if (--aicd <= 0) {                                   /* enemy AI */
    int i = rnd(4), k = q[1][i]; aicd = 30;
    if (el[1] >= C[k].cost) {
      if (!C[k].n) { int j = rnd(128); for (int n = 0; n < 128; n++) { U *a = &u[(j+n)&127]; if (a->on && a->t == 0) { play(1, i, a->x, a->y); break; } } }
      else play(1, i, 260 + rnd(180), (rnd(2) ? 60 : 164) + rnd(30) - 15);
      aicd = 90 + rnd(120);
    }
  }
  for (int i = 0; i < 128; i++) {                      /* units */
    U *a = &u[i]; if (!a->on) continue;
    const Card *c = &C[a->k];
    float best = 1e9f; int kind = 0, id = 0;
    if (!c->bo) for (int j = 0; j < 128; j++) if (u[j].on && u[j].t != a->t) { float d = D(a->x, a->y, u[j].x, u[j].y); if (d < best && d < 70) { best = d; kind = 1; id = j; } }
    if (!kind) for (int j = 0; j < 3; j++) { T *t = &tw[(1-a->t)*3+j]; if (t->hp > 0) { float d = D(a->x, a->y, t->x, t->y) - 14; if (d < best) { best = d; kind = 2; id = j; } } }
    if (!kind) continue;
    float gx, gy;
    if (kind == 1) { gx = u[id].x; gy = u[id].y; } else { gx = tw[(1-a->t)*3+id].x; gy = tw[(1-a->t)*3+id].y; }
    if (a->cd > 0) a->cd--;
    if (best <= c->rng) {
      if (a->cd <= 0) { a->cd = c->cd; if (kind == 1) { if ((u[id].hp -= c->dmg) <= 0) u[id].on = 0; } else tw[(1-a->t)*3+id].hp -= c->dmg; }
    } else {
      if ((a->x < 236 && gx > 244) || (a->x > 244 && gx < 236)) { gx = 240; gy = a->y < 112 ? 60 : 164; }   /* use a bridge */
      float d = D(a->x, a->y, gx, gy);
      if (d > .5f) { a->x += (gx - a->x) / d * c->spd; a->y += (gy - a->y) / d * c->spd; }
    }
  }
  for (int i = 0; i < 6; i++) {                        /* tower fire */
    T *t = &tw[i]; if (t->hp <= 0) continue;
    if (t->cd > 0) { t->cd--; continue; }
    int bj = -1; float bd = 90;
    for (int j = 0; j < 128; j++) if (u[j].on && u[j].t != t->t) { float d = D(t->x, t->y, u[j].x, u[j].y); if (d < bd) { bd = d; bj = j; } }
    if (bj >= 0) { t->cd = 40; if ((u[bj].hp -= t->king ? 16 : 12) <= 0) u[bj].on = 0; }
  }
  if (flash) flash--;
  int hs[2] = {0, 0}; cr[0] = cr[1] = 0;
  for (int s = 0; s < 2; s++) for (int i = 0; i < 3; i++) { if (tw[s*3+i].hp <= 0) cr[1-s] += i == 2 ? 3 : 1; else hs[s] += tw[s*3+i].hp; }
  if (tw[2].hp <= 0 || tw[5].hp <= 0 || tick >= 10800) {
    over = 1;
    res = cr[0] > cr[1] ? 1 : cr[0] < cr[1] ? -1 : hs[0] > hs[1] ? 1 : hs[0] < hs[1] ? -1 : 0;
    gold += res > 0 ? 60 : res < 0 ? 10 : 25; save();
  }
}

static void draw(void) {
  u32 W_ = RGB(255,255,255);
  memcpy(fb, bg, sizeof bg); rect(0, AY, SW, SH-AY, RGB(30,30,40));
  for (int i = 0; i < 6; i++) {
    T *t = &tw[i]; int s = t->king ? 28 : 22, x = (int)t->x, y = (int)t->y;
    if (t->hp <= 0) { rect(x-6, y-6, 12, 12, RGB(80,80,80)); continue; }
    rect(x-s/2, y-s/2, s, s, t->t ? RGB(210,50,50) : RGB(50,100,220)); box(x-s/2, y-s/2, s, s, W_); for (int b = 6; b < s; b += 6) rect(x-s/2, y-s/2+b, s, 1, 0xFF202020u);
    rect(x-12, y-s/2-5, 24 * t->hp / (t->king ? 560 : 340), 3, RGB(80,255,80));
  }
  for (int i = 0; i < 128; i++) if (u[i].on) {
    U *a = &u[i]; int x = (int)a->x, y = (int)a->y;
    rect(x-4, y-4, 8, 8, a->t ? RGB(210,50,50) : RGB(50,100,220)); rect(x-3, y-3, 6, 6, C[a->k].col); rect(x-2, y-2, 1, 1, W_); rect(x+1, y-2, 1, 1, W_);
    rect(x-4, y-7, 8 * a->hp / C[a->k].hp, 1, RGB(80,255,80));
  }
  if (flash) box(fx-fr, fy-fr, 2*fr, 2*fr, RGB(255,200,50));
  if (!over) { if (!C[q[0][sel]].n) { int r_ = (int)C[q[0][sel]].rng; box(cx-r_, cy-r_, 2*r_, 2*r_, W_); } else { rect(cx-5, cy, 11, 1, W_); rect(cx, cy-5, 1, 11, W_); } }
  for (int i = 0; i < 10; i++) rect(6 + i*14, AY+36, 12, 10, i < (int)el[0] ? RGB(200,60,220) : RGB(70,40,80));
  num(6, AY+6, (int)el[0], 4, RGB(220,120,240));
  icon(104, AY+18, q[0][4], 2, C[q[0][4]].col);
  for (int i = 0; i < 4; i++) {
    const Card *c = &C[q[0][i]]; int x = 150 + i*48, y = AY+4; u32 col = c->col;
    if (c->cost > el[0]) col = ((col >> 1) & 0x7F7F7F) | 0xFF000000u;
    rect(x, y, 44, 40, RGB(50,50,60)); rect(x+4, y+4, 36, 22, RGB(36,36,46)); icon(x+14, y+6, q[0][i], 2, col); num(x+4, y+29, c->cost, 2, RGB(220,120,240));
    box(x, y, 44, 40, i == sel ? W_ : RGB(90,90,100)); if (i == sel) box(x+1, y+1, 42, 38, W_);
  }
  int sec = (10800 - tick) / 60; if (sec < 0) sec = 0;
  num(SW/2-24, 4, sec/60, 2, W_); rect(SW/2-14, 7, 2, 2, W_); rect(SW/2-14, 11, 2, 2, W_);
  num(SW/2-10, 4, sec%60/10, 2, W_); num(SW/2-2, 4, sec%10, 2, W_);
  if (tick > 7200) rect(SW/2-30, 16, 60, 2, RGB(200,60,220));
  num(6, 4, cr[0], 3, RGB(80,140,255)); num(SW-20, 4, cr[1], 3, RGB(255,90,90));
  if (over) {
    rect(120, 90, 240, 60, res > 0 ? RGB(40,160,60) : res < 0 ? RGB(190,40,40) : RGB(120,120,120));
    { const char *m = res > 0 ? "WIN" : res < 0 ? "LOSE" : "DRAW"; txt(SW/2 - (int)strlen(m)*8, 96, m, 4, W_); num(SW/2-26, 124, cr[0], 3, W_); rect(SW/2-4, 130, 8, 3, W_); num(SW/2+14, 124, cr[1], 3, W_); }
  }
}

static void start(void) { scr = 1; reset(); mkbg(); }
static void ui(int in) {
  int e = in & ~prev, shop = scr == 3; u32 W_ = 0xFFFFFFFFu, Y_ = RGB(255,220,60);
  for (int y = 0; y < SH; y++) rect(0, y, SW, 1, RGB(20 + y/8, 24 + y/10, 52 + y/4));
  txt(SW-130, 6, "GOLD", 2, Y_); num(SW-90, 6, gold, 2, Y_);
  if (scr == 0) {
    const char *it[4] = {"BATTLE", "CARDS", "SHOP", "ARENA"};
    if (e & BU) mi = (mi + 3) % 4;
    if (e & BD) mi = (mi + 1) % 4;
    if (mi == 3 && (e & (BL|BR))) for (int n = 0; n < 3; n++) { arena = (arena + ((e & BR) ? 1 : 2)) % 3; if (arOwn[arena]) break; }
    if (e & BX) { if (mi == 0) { start(); return; } if (mi < 3) { scr = mi + 1; gi = 0; return; } }
    txt(SW/2-132, 30, "ARENA CLASH", 6, W_);
    for (int i = 0; i < 4; i++) { int y = 100 + i*34, x = SW/2-100; rect(x, y, 200, 28, i == mi ? RGB(70,80,160) : RGB(40,44,80)); box(x, y, 200, 28, i == mi ? W_ : RGB(90,90,120)); txt(x+12, y+9, it[i], 2, W_); if (i == 3) txt(x+80, y+9, AN[arena], 2, Y_); }
    return;
  }
  int nI = shop ? 19 : 16;
  if (e & (BO|BS)) { scr = 0; return; }
  if (e & BL) gi = (gi + nI - 1) % nI;
  if (e & BR) gi = (gi + 1) % nI;
  if (e & BU) gi = gi >= 16 ? 8 : gi >= 8 ? gi - 8 : gi;
  if (e & BD) gi = gi < 8 ? gi + 8 : (shop && gi < 16 ? 16 : gi);
  if (!shop) { if (e & BQ) dslot = (dslot + 7) % 8; if (e & BE) dslot = (dslot + 1) % 8; }
  if (e & BX) {
    if (gi < 16) {
      if (!shop && owned[gi]) { int d = 0; for (int i = 0; i < 8; i++) if (deck[i] == gi) d = 1; if (!d) { deck[dslot] = gi; dslot = (dslot + 1) % 8; save(); } }
      else if (shop && !owned[gi] && gold >= C[gi].cost*80) { gold -= C[gi].cost*80; owned[gi] = 1; save(); }
    } else if (shop && !arOwn[gi-16] && gold >= (gi-16)*300) { gold -= (gi-16)*300; arOwn[gi-16] = 1; save(); }
  }
  txt(8, 6, shop ? "SHOP" : "CARDS", 3, W_);
  int y0 = shop ? 30 : 100;
  if (!shop) for (int i = 0; i < 8; i++) { int x = 16 + i*56; rect(x, 36, 50, 50, i == dslot ? Y_ : RGB(60,60,90)); rect(x+2, 38, 46, 46, RGB(30,30,50)); icon(x+13, 49, deck[i], 3, C[deck[i]].col); }
  for (int i = 0; i < 16; i++) {
    int x = 16 + (i%8)*56, y = y0 + (i/8)*66; u32 col = owned[i] ? C[i].col : ((C[i].col >> 1) & 0x7F7F7F) | 0xFF000000u;
    rect(x, y, 50, 58, RGB(40,44,80)); box(x, y, 50, 58, i == gi ? W_ : RGB(90,90,120)); if (i == gi) box(x+1, y+1, 48, 56, W_);
    icon(x+13, y+4, i, 3, col); txt(x+2, y+32, N[i], 1, W_);
    for (int j = 0; j < 8; j++) if (!shop && deck[j] == i) rect(x+40, y+4, 6, 6, Y_);
    if (shop && !owned[i]) num(x+2, y+44, C[i].cost*80, 1, Y_); else if (shop) txt(x+2, y+44, "OWNED", 1, RGB(90,230,90)); else num(x+2, y+44, C[i].cost, 1, RGB(220,120,240));
  }
  if (shop) for (int i = 0; i < 3; i++) {
    int x = 16 + i*140, y = 170; rect(x, y, 130, 30, AC[i][0]); rect(x+60, y, 10, 30, AC[i][2]); rect(x, y+30, 130, 20, RGB(40,44,80));
    box(x, y, 130, 50, gi == 16+i ? W_ : RGB(90,90,120)); txt(x+4, y+36, AN[i], 1, W_);
    if (arOwn[i]) txt(x+80, y+36, "OWNED", 1, RGB(90,230,90)); else num(x+80, y+36, i*300, 1, Y_);
  }
  if (gi < 16) { txt(8, 246, N[gi], 2, W_); txt(130, 246, "HP", 2, W_); num(160, 246, C[gi].hp, 2, W_); txt(230, 246, "DMG", 2, W_); num(272, 246, C[gi].dmg, 2, W_); txt(330, 246, "COST", 2, W_); num(386, 246, C[gi].cost, 2, W_); }
  else txt(8, 246, AN[gi-16], 2, W_);
}
static void frame(int in) { gf++; if (scr == 1) { step(in); draw(); } else ui(in); }
#ifdef PSP
static int exitcb(int a, int b, void *c) { sceKernelExitGame(); return 0; }
static int cbt(SceSize a, void *b) { sceKernelRegisterExitCallback(sceKernelCreateCallback("exit", exitcb, NULL)); sceKernelSleepThreadCB(); return 0; }
int main(void) {
  int th = sceKernelCreateThread("cb", cbt, 0x11, 0xFA0, 0, NULL); if (th >= 0) sceKernelStartThread(th, 0, NULL);
  sceDisplaySetMode(0, SW, SH); sceCtrlSetSamplingCycle(0); sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
  load(); SceCtrlData p;
  for (;;) {
    sceCtrlPeekBufferPositive(&p, 1); int in = 0;
    if ((p.Buttons & PSP_CTRL_LEFT)  || p.Lx < 64)  in |= BL;
    if ((p.Buttons & PSP_CTRL_RIGHT) || p.Lx > 192) in |= BR;
    if ((p.Buttons & PSP_CTRL_UP)    || p.Ly < 64)  in |= BU;
    if ((p.Buttons & PSP_CTRL_DOWN)  || p.Ly > 192) in |= BD;
    if (p.Buttons & PSP_CTRL_CROSS) in |= BX;
    if (p.Buttons & PSP_CTRL_LTRIGGER) in |= BQ;
    if (p.Buttons & PSP_CTRL_RTRIGGER) in |= BE;
    if (p.Buttons & PSP_CTRL_START) in |= BS;
    if (p.Buttons & PSP_CTRL_CIRCLE) in |= BO;
    frame(in); prev = in;
    sceDisplayWaitVblankStart();
    u32 *v = (u32 *)0x44000000; for (int y = 0; y < SH; y++) memcpy(v + y*512, fb + y*SW, SW*4);
    sceDisplaySetFrameBuf((void *)0x44000000, 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
  }
}
#else
int main(void) {  /* headless PC smoke test: scripted player vs AI */
  load(); for (int f = 0; f < 800; f++) { scr = f / 200; frame((f % 3 ? BR : BD) | (f % 7 == 0 ? BX : 0) | (f % 11 == 0 ? BQ : 0)); prev = 0; } gold = 50; start();
  for (int f = 0; f < 11000 && !over; f++) { int in = (f % 40 < 2 ? BR : 0) | (f % 97 < 2 ? BX : 0) | (f % 300 == 0 ? BE : 0); step(in); prev = in; }
  printf("tick=%d over=%d crowns %d-%d result=%d\n", tick, over, cr[0], cr[1], res);
  return 0;
}
#endif
