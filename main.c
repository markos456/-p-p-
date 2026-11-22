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
enum { BL = 1, BR = 2, BU = 4, BD = 8, BX = 16, BQ = 32, BE = 64, BS = 128 };

typedef struct { int cost, hp, dmg, bo; float rng, spd; int cd, n; u32 col; } Card;
static const Card C[8] = {  /* cost hp dmg bldgOnly range speed cooldown count color */
  {3, 90, 11, 0, 12, .6f, 40, 1, RGB(190,190,200)},  /* Soldier */
  {3, 45,  8, 0, 64, .6f, 35, 1, RGB(60,200,90)},    /* Bowman  */
  {5,320, 20, 1, 12, .35f,60, 1, RGB(150,100,50)},   /* Brute (towers only) */
  {3, 24,  6, 0,  9, .8f, 30, 3, RGB(230,200,40)},   /* Swarm x3 */
  {4,110, 14, 1, 12, 1.1f,45, 1, RGB(210,90,200)},   /* Rider (towers only, fast) */
  {4, 55, 24, 0, 72, .5f, 70, 1, RGB(60,120,230)},   /* Mage */
  {2, 35,  7, 0,  9, 1.2f,25, 1, RGB(240,140,40)},   /* Scout */
  {4,  0, 90, 0, 38, 0,    0, 0, RGB(255,80,30)}};   /* Blast (spell) */

typedef struct { float x, y; int hp, t, k, cd, on; } U;
typedef struct { float x, y; int hp, t, king, cd; } T;
static U u[128];
static T tw[6];                       /* 0-2 player (princess, princess, king), 3-5 enemy */
static float el[2];
static int q[2][8], tick, over, aicd, sel, cx, cy, flash, fx, fy, prev, cr[2], res;
static u32 fb[SW * SH], rs = 12345;

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

static void shuf(int *a) { for (int i = 0; i < 8; i++) a[i] = i; for (int i = 7; i > 0; i--) { int j = rnd(i+1), t = a[i]; a[i] = a[j]; a[j] = t; } }
static void reset(void) {
  float px[3] = {80, 80, 28}, py[3] = {60, 164, 112};
  memset(u, 0, sizeof u);
  for (int s = 0; s < 2; s++) for (int i = 0; i < 3; i++)
    tw[s*3+i] = (T){s ? SW - px[i] : px[i], py[i], i == 2 ? 560 : 340, s, i == 2, 0};
  el[0] = el[1] = 5; tick = over = 0; aicd = 120; sel = 0; cx = 120; cy = 112; flash = 0; res = 0;
  shuf(q[0]); shuf(q[1]);
}

static int play(int s, int slot, float x, float y) {
  int k = q[s][slot]; const Card *c = &C[k];
  if (el[s] < c->cost) return 0;
  el[s] -= c->cost;
  if (k == 7) {
    flash = 10; fx = (int)x; fy = (int)y;
    for (int i = 0; i < 128; i++) if (u[i].on && u[i].t != s && D(u[i].x, u[i].y, x, y) < c->rng && (u[i].hp -= c->dmg) <= 0) u[i].on = 0;
    for (int i = 0; i < 3; i++) { T *t = &tw[(1-s)*3+i]; if (t->hp > 0 && D(t->x, t->y, x, y) < c->rng + 12) t->hp -= c->dmg / 3; }
  } else for (int j = 0; j < c->n; j++) for (int i = 0; i < 128; i++) if (!u[i].on) { u[i] = (U){x + j*7 - 7, y + (j&1)*7, c->hp, s, k, 0, 1}; break; }
  int n = q[s][slot]; q[s][slot] = q[s][4]; memmove(&q[s][4], &q[s][5], 3*sizeof(int)); q[s][7] = n;
  return 1;
}

static void step(int in) {
  if (over) { if (in & BS & ~prev) reset(); return; }
  tick++;
  float rate = tick > 7200 ? 1.f/84 : 1.f/168;      /* double elixir in the last minute */
  for (int s = 0; s < 2; s++) { el[s] += rate; if (el[s] > 10) el[s] = 10; }
  if (in & BL) cx -= 2; if (in & BR) cx += 2; if (in & BU) cy -= 2; if (in & BD) cy += 2;
  if (cx < 0) cx = 0; if (cx >= SW) cx = SW-1; if (cy < 0) cy = 0; if (cy >= AY) cy = AY-1;
  if (in & BQ & ~prev) sel = (sel + 3) % 4;
  if (in & BE & ~prev) sel = (sel + 1) % 4;
  if (in & BX & ~prev) { if (q[0][sel] == 7 || cx < 236) play(0, sel, cx, cy); }
  if (--aicd <= 0) {                                   /* enemy AI */
    int i = rnd(4), k = q[1][i]; aicd = 30;
    if (el[1] >= C[k].cost) {
      if (k == 7) { int j = rnd(128); for (int n = 0; n < 128; n++) { U *a = &u[(j+n)&127]; if (a->on && a->t == 0) { play(1, i, a->x, a->y); break; } } }
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
  }
}

static void draw(void) {
  u32 W_ = RGB(255,255,255);
  rect(0, 0, SW, AY, RGB(58,125,46)); rect(0, AY, SW, SH-AY, RGB(30,30,40));
  rect(232, 0, 16, AY, RGB(50,110,200));
  for (int i = 0; i < 2; i++) rect(228, (i ? 164 : 60) - 12, 24, 24, RGB(140,100,60));
  for (int i = 0; i < 6; i++) {
    T *t = &tw[i]; int s = t->king ? 28 : 22, x = (int)t->x, y = (int)t->y;
    if (t->hp <= 0) { rect(x-6, y-6, 12, 12, RGB(80,80,80)); continue; }
    rect(x-s/2, y-s/2, s, s, t->t ? RGB(210,50,50) : RGB(50,100,220)); box(x-s/2, y-s/2, s, s, W_);
    rect(x-12, y-s/2-5, 24 * t->hp / (t->king ? 560 : 340), 3, RGB(80,255,80));
  }
  for (int i = 0; i < 128; i++) if (u[i].on) {
    U *a = &u[i]; int x = (int)a->x, y = (int)a->y;
    rect(x-4, y-4, 8, 8, a->t ? RGB(210,50,50) : RGB(50,100,220)); rect(x-3, y-3, 6, 6, C[a->k].col);
    rect(x-4, y-7, 8 * a->hp / C[a->k].hp, 1, RGB(80,255,80));
  }
  if (flash) box(fx-38, fy-38, 76, 76, RGB(255,200,50));
  if (!over) { if (q[0][sel] == 7) box(cx-38, cy-38, 76, 76, W_); else { rect(cx-5, cy, 11, 1, W_); rect(cx, cy-5, 1, 11, W_); } }
  for (int i = 0; i < 10; i++) rect(6 + i*14, AY+36, 12, 10, i < (int)el[0] ? RGB(200,60,220) : RGB(70,40,80));
  num(6, AY+6, (int)el[0], 4, RGB(220,120,240));
  rect(100, AY+16, 30, 26, C[q[0][4]].col);
  for (int i = 0; i < 4; i++) {
    const Card *c = &C[q[0][i]]; int x = 150 + i*48, y = AY+4; u32 col = c->col;
    if (c->cost > el[0]) col = ((col >> 1) & 0x7F7F7F) | 0xFF000000u;
    rect(x, y, 44, 40, RGB(50,50,60)); rect(x+4, y+4, 36, 22, col); num(x+4, y+29, c->cost, 2, RGB(220,120,240));
    box(x, y, 44, 40, i == sel ? W_ : RGB(90,90,100)); if (i == sel) box(x+1, y+1, 42, 38, W_);
  }
  int sec = (10800 - tick) / 60; if (sec < 0) sec = 0;
  num(SW/2-24, 4, sec/60, 2, W_); rect(SW/2-14, 7, 2, 2, W_); rect(SW/2-14, 11, 2, 2, W_);
  num(SW/2-10, 4, sec%60/10, 2, W_); num(SW/2-2, 4, sec%10, 2, W_);
  if (tick > 7200) rect(SW/2-30, 16, 60, 2, RGB(200,60,220));
  num(6, 4, cr[0], 3, RGB(80,140,255)); num(SW-20, 4, cr[1], 3, RGB(255,90,90));
  if (over) {
    rect(120, 90, 240, 60, res > 0 ? RGB(40,160,60) : res < 0 ? RGB(190,40,40) : RGB(120,120,120));
    num(190, 105, cr[0], 6, W_); rect(236, 120, 8, 4, W_); num(264, 105, cr[1], 6, W_);
  }
}

#ifdef PSP
static int exitcb(int a, int b, void *c) { sceKernelExitGame(); return 0; }
static int cbt(SceSize a, void *b) { sceKernelRegisterExitCallback(sceKernelCreateCallback("exit", exitcb, NULL)); sceKernelSleepThreadCB(); return 0; }
int main(void) {
  int th = sceKernelCreateThread("cb", cbt, 0x11, 0xFA0, 0, NULL); if (th >= 0) sceKernelStartThread(th, 0, NULL);
  sceDisplaySetMode(0, SW, SH); sceCtrlSetSamplingCycle(0); sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
  reset(); SceCtrlData p;
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
    step(in); prev = in; draw();
    sceDisplayWaitVblankStart();
    u32 *v = (u32 *)0x44000000; for (int y = 0; y < SH; y++) memcpy(v + y*512, fb + y*SW, SW*4);
    sceDisplaySetFrameBuf((void *)0x44000000, 512, PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
  }
}
#else
int main(void) {  /* headless PC smoke test: scripted player vs AI */
  reset();
  for (int f = 0; f < 11000 && !over; f++) { int in = (f % 40 < 2 ? BR : 0) | (f % 97 < 2 ? BX : 0) | (f % 300 == 0 ? BE : 0); step(in); prev = in; draw(); }
  printf("tick=%d over=%d crowns %d-%d result=%d\n", tick, over, cr[0], cr[1], res);
  return 0;
}
#endif
