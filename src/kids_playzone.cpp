#include "kids_playzone.h"
#include "audio.h"
#include <GL/freeglut.h>
#include <cmath>
#include <cstdlib>
#include <cstdio>

// =====================================================================
//  HANABI - KIDS' PLAY ZONE
//  Three festival stalls: Goldfish Scooping, Ring Toss, Target Game.
//  World coordinates: 0..1000 (x), 0..600 (y).
// =====================================================================

namespace
{
const float PI = 3.14159265f;

// stall centres
const float SC_FISH   = 175.0f;
const float SC_RING   = 500.0f;
const float SC_TARGET = 825.0f;

// ----------------------------------------------------------------
// small helpers
// ----------------------------------------------------------------
float frand(float a, float b) { return a + (b - a) * (std::rand() / (float)RAND_MAX); }
float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
float mixf(float a, float b, float t) { return a + (b - a) * t; }

void rect(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f)
{
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
        glVertex2f(x, y); glVertex2f(x + w, y);
        glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}

// bottom colour -> top colour
void gradRect(float x, float y, float w, float h,
              float r1, float g1, float b1, float r2, float g2, float b2)
{
    glBegin(GL_QUADS);
        glColor3f(r1, g1, b1); glVertex2f(x, y); glVertex2f(x + w, y);
        glColor3f(r2, g2, b2); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}

void disc(float x, float y, float rx, float ry, float r, float g, float b, float a = 1.0f)
{
    glColor4f(r, g, b, a);
    glBegin(GL_TRIANGLE_FAN);
        glVertex2f(x, y);
        for (int i = 0; i <= 40; i++)
        {
            float t = 2.0f * PI * i / 40.0f;
            glVertex2f(x + cosf(t) * rx, y + sinf(t) * ry);
        }
    glEnd();
}

void glowDisc(float x, float y, float rad, float r, float g, float b, float a)
{
    glBegin(GL_TRIANGLE_FAN);
        glColor4f(r, g, b, a); glVertex2f(x, y);
        glColor4f(r, g, b, 0.0f);
        for (int i = 0; i <= 32; i++)
        {
            float t = 2.0f * PI * i / 32.0f;
            glVertex2f(x + cosf(t) * rad, y + sinf(t) * rad);
        }
    glEnd();
}

void ellipseRing(float x, float y, float rx, float ry, float th,
                 float r, float g, float b, float a = 1.0f)
{
    glColor4f(r, g, b, a);
    glBegin(GL_TRIANGLE_STRIP);
        for (int i = 0; i <= 40; i++)
        {
            float t = 2.0f * PI * i / 40.0f;
            glVertex2f(x + cosf(t) * rx, y + sinf(t) * ry);
            glVertex2f(x + cosf(t) * (rx - th), y + sinf(t) * (ry - th * 0.6f));
        }
    glEnd();
}

void tri(float x1, float y1, float x2, float y2, float x3, float y3,
         float r, float g, float b, float a = 1.0f)
{
    glColor4f(r, g, b, a);
    glBegin(GL_TRIANGLES);
        glVertex2f(x1, y1); glVertex2f(x2, y2); glVertex2f(x3, y3);
    glEnd();
}

void line(float x1, float y1, float x2, float y2, float w,
          float r, float g, float b, float a = 1.0f)
{
    glLineWidth(w);
    glColor4f(r, g, b, a);
    glBegin(GL_LINES);
        glVertex2f(x1, y1); glVertex2f(x2, y2);
    glEnd();
}

float textWidth(void* font, const char* s)
{
    return (float)glutBitmapLength(font, (const unsigned char*)s);
}

void textAt(float x, float y, const char* s, void* font,
            float r, float g, float b, float a = 1.0f)
{
    glColor4f(r, g, b, a);
    glRasterPos2f(x, y);
    for (int i = 0; s[i]; i++) glutBitmapCharacter(font, s[i]);
}

void textC(float cx, float y, const char* s, void* font,
           float r, float g, float b, float a = 1.0f)
{
    textAt(cx - textWidth(font, s) * 0.5f, y, s, font, r, g, b, a);
}

// ----------------------------------------------------------------
// state
// ----------------------------------------------------------------
float T = 0.0f, lastT = -100.0f;

float mouseX = 0, mouseY = 0, lastMove = -100.0f;

// ---- goldfish
struct Fish
{
    float cx, cy, ax, ay, sp, ph, size;
    float r, g, b;
    float catchT;            // <0 swimming, otherwise seconds since scooped
    float x, y;
    int   dir;
};
struct TankRipple { bool act; float x, y, age; };

Fish       fish[6];
TankRipple tankRip[10];
float poiX = 175, poiY = 420;
int   demoFish = 0;
float demoTimer = 1.5f;
int   caughtCount = 0;

// ---- ring toss
struct RingFly  { bool act; float t, dur, x0, y0, x1, y1; bool hit; float r, g, b; };
struct RingRest { bool act; float x, y, age; bool hit; float r, g, b; };

const float PEG_X[5] = { 410, 455, 500, 545, 590 };
RingFly  flying[6];
RingRest resting[24];
float ringTimer = 1.0f;
int   ringHits = 0;

// ---- target game
struct Dart { bool act; float t, dur, x0, y0, x1, y1; int hitIdx; };
Dart  darts[4];
float tgtHit[3];
float dartTimer = 1.2f;
int   targetHits = 0;
float aimX = SC_TARGET, aimY = 330;

const float RING_COL[5][3] = {
    {0.95f, 0.25f, 0.30f}, {1.0f, 0.85f, 0.2f}, {0.25f, 0.8f, 0.45f},
    {0.3f, 0.6f, 1.0f},    {1.0f, 0.55f, 0.85f}
};

// ---- the three kids who play the games (seen from behind, facing their stall)
const float KID_FISH_X = 135.0f, KID_RING_X = 500.0f, KID_TGT_X = 825.0f;
const float KID_FEET = 66.0f, KID_SCALE = 1.5f;
const float FISH_HAND_X = KID_FISH_X + 26.0f, FISH_HAND_Y = 172.0f;

float armT = -1.0f;                 // ring-throwing arm animation (<0 = idle)
bool  pendThrow = false;            // a ring is waiting to leave the kid's hand
float pendX = 0.0f;
bool  pendHit = false;
float heldCol[3] = { 0.95f, 0.25f, 0.30f };
float gunKick = -1.0f;              // seconds since the toy gun fired (<0 = idle)

void ringHand(float& hx, float& hy)
{
    float u = armT < 0.0f ? 0.0f : clamp01(armT / 0.5f);
    hx = KID_RING_X + 20.0f + 4.0f * sinf(PI * u);
    hy = 158.0f + 48.0f * sinf(PI * u);
}

void gunHand(float& hx, float& hy)
{
    float k = gunKick >= 0.0f ? 5.0f * expf(-gunKick * 25.0f) : 0.0f;
    hx = KID_TGT_X + 20.0f;
    hy = 152.0f - k;
}

void initState()
{
    const float fcx[6] = { 115, 235, 175, 150, 215, 185 };
    const float fcy[6] = { 322, 308, 290, 300, 326, 332 };
    const float fax[6] = { 35, 25, 45, 30, 25, 18 };
    const float fsp[6] = { 0.7f, 0.9f, 0.55f, 1.0f, 0.8f, 0.65f };
    const float fph[6] = { 0.0f, 1.7f, 3.1f, 4.4f, 2.2f, 5.3f };
    const float fsz[6] = { 1.15f, 1.0f, 1.25f, 0.9f, 1.0f, 1.1f };
    const float col[6][3] = {
        {1.0f, 0.50f, 0.10f}, {0.95f, 0.20f, 0.15f}, {1.0f, 0.80f, 0.15f},
        {1.0f, 0.88f, 0.78f}, {0.13f, 0.10f, 0.16f}, {1.0f, 0.55f, 0.60f}
    };
    for (int i = 0; i < 6; i++)
    {
        Fish& f = fish[i];
        f.cx = fcx[i]; f.cy = fcy[i]; f.ax = fax[i]; f.ay = 8.0f;
        f.sp = fsp[i]; f.ph = fph[i]; f.size = fsz[i];
        f.r = col[i][0]; f.g = col[i][1]; f.b = col[i][2];
        f.catchT = -1.0f; f.x = f.cx; f.y = f.cy; f.dir = 1;
    }
    for (int i = 0; i < 10; i++) tankRip[i].act = false;
    for (int i = 0; i < 6; i++)  flying[i].act = false;
    for (int i = 0; i < 24; i++) resting[i].act = false;
    for (int i = 0; i < 4; i++)  darts[i].act = false;
    for (int i = 0; i < 3; i++)  tgtHit[i] = -1.0f;

    poiX = SC_FISH; poiY = 430;
    demoFish = 0; demoTimer = 1.5f; caughtCount = 0;
    ringTimer = 1.0f; ringHits = 0;
    dartTimer = 1.2f; targetHits = 0;
    armT = -1.0f; pendThrow = false; gunKick = -1.0f;
    heldCol[0] = RING_COL[0][0]; heldCol[1] = RING_COL[0][1]; heldCol[2] = RING_COL[0][2];
}

// ----------------------------------------------------------------
// goldfish logic
// ----------------------------------------------------------------
const float TANK_L = 65, TANK_R = 285, TANK_B = 262, TANK_T = 350;

void addTankRipple(float x, float y)
{
    for (int i = 0; i < 10; i++)
        if (!tankRip[i].act) { tankRip[i].act = true; tankRip[i].x = x; tankRip[i].y = y; tankRip[i].age = 0; return; }
}

void catchFish(int i)
{
    if (fish[i].catchT >= 0.0f) return;
    fish[i].catchT = 0.0f;
    caughtCount++;
    addTankRipple(fish[i].x, TANK_T - 4);
    audioPlay(SFX_SPLASH, 0.5f);
    audioPlay(SFX_DING, 0.45f);
}

void updateFish(float dt)
{
    for (int i = 0; i < 6; i++)
    {
        Fish& f = fish[i];
        float ang = f.sp * T + f.ph;
        f.x = f.cx + f.ax * sinf(ang);
        f.y = f.cy + f.ay * sinf(ang * 1.7f + 1.0f);
        f.dir = cosf(ang) >= 0.0f ? 1 : -1;

        if (f.catchT >= 0.0f)
        {
            f.catchT += dt;
            float u = f.catchT / 1.1f;
            if (u >= 1.0f)
            {
                f.catchT = -1.0f;
                addTankRipple(f.x, TANK_T - 4);
                audioPlay(SFX_SPLASH, 0.35f);
            }
            else f.y += 58.0f * sinf(PI * u);
        }
    }

    for (int i = 0; i < 10; i++)
        if (tankRip[i].act) { tankRip[i].age += dt; if (tankRip[i].age > 1.4f) tankRip[i].act = false; }

    // is the player's mouse over the goldfish stall?
    bool mouseOver = (T - lastMove < 3.0f) &&
                     mouseX > 30 && mouseX < 320 && mouseY > 150 && mouseY < 470;

    float tx, ty;
    if (mouseOver) { tx = mouseX; ty = mouseY; }
    else
    {
        tx = fish[demoFish].x; ty = fish[demoFish].y + 6;
        demoTimer -= dt;
        float dx = poiX - tx, dy = poiY - ty;
        if (demoTimer <= 0.0f && dx * dx + dy * dy < 28.0f * 28.0f && fish[demoFish].catchT < 0.0f)
        {
            catchFish(demoFish);
            demoFish = (demoFish + 1 + std::rand() % 5) % 6;
            demoTimer = frand(2.2f, 3.2f);
        }
    }
    float k = clamp01(dt * 6.0f);
    poiX += (tx - poiX) * k;
    poiY += (ty - poiY) * k;
}

// ----------------------------------------------------------------
// ring toss logic
// ----------------------------------------------------------------
void throwRing(float targetX, bool hit)
{
    if (armT >= 0.0f || pendThrow) return;       // the kid is still busy
    pendThrow = true; pendX = targetX; pendHit = hit;
    armT = 0.0f;
}

void updateRings(float dt)
{
    // kid's throwing arm: the ring is released at the top of the swing
    if (armT >= 0.0f)
    {
        armT += dt;
        if (pendThrow && armT >= 0.2f)
        {
            float hx, hy;
            ringHand(hx, hy);
            for (int i = 0; i < 6; i++)
            {
                if (flying[i].act) continue;
                RingFly& r = flying[i];
                r.act = true; r.t = 0; r.dur = 0.8f;
                r.x0 = hx; r.y0 = hy;
                r.x1 = pendX; r.y1 = pendHit ? 292.0f : 272.0f;
                r.hit = pendHit;
                r.r = heldCol[0]; r.g = heldCol[1]; r.b = heldCol[2];
                audioPlay(SFX_POP, 0.35f);
                break;
            }
            pendThrow = false;
        }
        if (armT > 0.75f)                         // arm back down, kid picks a new ring
        {
            armT = -1.0f;
            int c = std::rand() % 5;
            heldCol[0] = RING_COL[c][0]; heldCol[1] = RING_COL[c][1]; heldCol[2] = RING_COL[c][2];
        }
    }

    for (int i = 0; i < 6; i++)
    {
        RingFly& r = flying[i];
        if (!r.act) continue;
        r.t += dt;
        if (r.t >= r.dur)
        {
            r.act = false;
            for (int k = 0; k < 24; k++)
            {
                if (resting[k].act) continue;
                resting[k].act = true; resting[k].x = r.x1; resting[k].y = r.y1;
                resting[k].age = 0; resting[k].hit = r.hit;
                resting[k].r = r.r; resting[k].g = r.g; resting[k].b = r.b;
                break;
            }
            if (r.hit) { ringHits++; audioPlay(SFX_DING, 0.55f); }
            else       { audioPlay(SFX_THUD, 0.5f); }
        }
    }
    for (int k = 0; k < 24; k++)
        if (resting[k].act) { resting[k].age += dt; if (resting[k].age > 6.0f) resting[k].act = false; }

    ringTimer -= dt;
    if (ringTimer <= 0.0f)
    {
        int p = std::rand() % 5;
        bool hit = frand(0, 1) < 0.6f;
        throwRing(hit ? PEG_X[p] : PEG_X[p] + (std::rand() % 2 ? 22.0f : -22.0f), hit);
        ringTimer = frand(1.1f, 1.9f);
    }
}

// ----------------------------------------------------------------
// target game logic
// ----------------------------------------------------------------
float targetX(int i) { return SC_TARGET + (i - 1) * 75.0f; }
float targetY(int i)
{
    const float base[3] = { 326.0f, 346.0f, 326.0f };
    return base[i] + 4.0f * sinf(T * 1.5f + i * 1.3f);
}

void shootDart(float x, float y, int hitIdx)
{
    for (int i = 0; i < 4; i++)
    {
        if (darts[i].act) continue;
        aimX = x; aimY = y;
        float hx, hy;
        gunHand(hx, hy);
        float dx = x - hx, dy = y - hy;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 1.0f) len = 1.0f;
        Dart& d = darts[i];
        d.act = true; d.t = 0; d.dur = 0.38f;
        d.x0 = hx + dx / len * 30.0f;            // muzzle of the gun in the kid's hand
        d.y0 = hy + dy / len * 30.0f;
        d.x1 = x; d.y1 = y; d.hitIdx = hitIdx;
        gunKick = 0.0f;
        audioPlay(SFX_POP, 0.45f);
        return;
    }
}

void updateTargets(float dt)
{
    if (gunKick >= 0.0f) { gunKick += dt; if (gunKick > 0.4f) gunKick = -1.0f; }
    for (int i = 0; i < 4; i++)
    {
        Dart& d = darts[i];
        if (!d.act) continue;
        d.t += dt;
        if (d.t >= d.dur)
        {
            d.act = false;
            if (d.hitIdx >= 0)
            {
                tgtHit[d.hitIdx] = 0.0f;
                targetHits++;
                audioPlay(SFX_DING, 0.5f);
            }
        }
    }
    for (int i = 0; i < 3; i++)
        if (tgtHit[i] >= 0.0f) { tgtHit[i] += dt; if (tgtHit[i] > 1.0f) tgtHit[i] = -1.0f; }

    dartTimer -= dt;
    if (dartTimer <= 0.0f)
    {
        int t = std::rand() % 3;
        if (frand(0, 1) < 0.75f) shootDart(targetX(t), targetY(t), t);
        else                     shootDart(targetX(t) + frand(-45, 45), targetY(t) + 42, -1);
        dartTimer = frand(1.5f, 2.4f);
    }
}

// ----------------------------------------------------------------
// mouse handling
// ----------------------------------------------------------------
void handleClick(float x, float y)
{
    if (y < 130 || y > 420) return;

    if (x > 355 && x < 645)               // ring toss
    {
        int best = 0;
        for (int i = 1; i < 5; i++)
            if (fabsf(PEG_X[i] - x) < fabsf(PEG_X[best] - x)) best = i;
        bool hit = fabsf(PEG_X[best] - x) < 22.0f;
        float lx = hit ? PEG_X[best] : (x < 395 ? 395.0f : (x > 605 ? 605.0f : x));
        throwRing(lx, hit);
    }
    else if (x > 680 && x < 970)          // target game
    {
        int best = -1; float bd = 34.0f;
        for (int i = 0; i < 3; i++)
        {
            float dx = x - targetX(i), dy = y - targetY(i);
            float d = sqrtf(dx * dx + dy * dy);
            if (d < bd) { bd = d; best = i; }
        }
        if (best >= 0) shootDart(targetX(best), targetY(best), best);
        else           shootDart(x, y, -1);
    }
    else if (x > 30 && x < 320)           // goldfish
    {
        int best = -1; float bd = 38.0f;
        for (int i = 0; i < 6; i++)
        {
            float dx = x - fish[i].x, dy = y - fish[i].y;
            float d = sqrtf(dx * dx + dy * dy);
            if (d < bd && fish[i].catchT < 0.0f) { bd = d; best = i; }
        }
        if (best >= 0) catchFish(best);
        else if (x > TANK_L && x < TANK_R && y > TANK_B && y < TANK_T)
        {
            addTankRipple(x, TANK_T - 4);
            audioPlay(SFX_SPLASH, 0.25f);
        }
    }
}

// ----------------------------------------------------------------
// drawing
// ----------------------------------------------------------------
float stringY(float x) { return 592.0f - 34.0f * sinf(PI * x / 1000.0f); }

void lantern(float x, float s, float r, float g, float b, float ph)
{
    float y = stringY(x) - 30.0f * s;
    glowDisc(x, y, 48.0f * s, 1.0f, 0.72f, 0.30f, 0.22f + 0.07f * sinf(T * 3.0f + ph));
    line(x, y + 15 * s, x, y + 30 * s, 1.5f, 0.1f, 0.08f, 0.1f);
    rect(x - 6 * s, y + 15 * s, 12 * s, 4 * s, 0.12f, 0.10f, 0.10f);
    disc(x, y, 13 * s, 17 * s, r, g, b);
    disc(x - 4 * s, y + 4 * s, 4 * s, 9 * s, 1.0f, 1.0f, 1.0f, 0.2f);
    for (int k = -2; k <= 2; k++)
    {
        float yy = y + k * 6.5f * s;
        float u = (k * 6.5f) / 17.0f;
        float w = 13.0f * s * sqrtf(1.0f - u * u);
        line(x - w, yy, x + w, yy, 1.0f, r * 0.55f, g * 0.45f, b * 0.45f, 0.7f);
    }
    rect(x - 6 * s, y - 19 * s, 12 * s, 4 * s, 0.12f, 0.10f, 0.10f);
    line(x, y - 19 * s, x, y - 29 * s, 1.5f, 0.95f, 0.8f, 0.3f);
}

void drawBackground()
{
    // night sky: violet at the top, warm magenta glow near the horizon
    gradRect(0, 150, 1000, 450, 0.70f, 0.32f, 0.48f, 0.10f, 0.06f, 0.30f);
    // moon with soft halo (matches the Festival Street night sky)
    glowDisc(440, 538, 120, 1.0f, 0.92f, 0.75f, 0.18f);
    disc(440, 538, 27, 27, 1.0f, 0.95f, 0.72f);

    // stars
    glEnable(GL_POINT_SMOOTH);
    glPointSize(2.0f);
    glBegin(GL_POINTS);
    for (int i = 0; i < 60; i++)
    {
        float sx = fmodf(i * 173.3f + 17.0f, 990.0f) + 5.0f;
        float sy = 360.0f + fmodf(i * 97.1f + i * i * 0.7f, 235.0f);
        float a = 0.45f + 0.45f * sinf(T * (1.2f + (i % 4) * 0.5f) + i);
        glColor4f(1.0f, 0.95f, 0.9f, a);
        glVertex2f(sx, sy);
    }
    glEnd();

    // soft bokeh lights between the stalls
    for (int i = 0; i < 14; i++)
    {
        float bx = fmodf(i * 191.7f + 60.0f, 1000.0f);
        float by = 290.0f + fmodf(i * 67.3f, 150.0f);
        float rad = 18.0f + (i % 4) * 8.0f;
        float a = 0.10f + 0.05f * sinf(T * 1.3f + i);
        if (i % 3 == 0)      glowDisc(bx, by, rad, 1.0f, 0.55f, 0.35f, a);
        else if (i % 3 == 1) glowDisc(bx, by, rad, 1.0f, 0.85f, 0.45f, a);
        else                 glowDisc(bx, by, rad, 0.9f, 0.45f, 0.75f, a);
    }

    // ground: purple cobblestone street in perspective
    gradRect(0, 0, 1000, 175, 0.17f, 0.10f, 0.22f, 0.42f, 0.27f, 0.38f);
    for (int i = -8; i <= 8; i++)
        line(500 + i * 28.0f, 175, 500 + i * 190.0f, 0, 1.0f, 0.62f, 0.45f, 0.55f, 0.35f);
    for (int k = 1; k <= 6; k++)
    {
        float yy = 175.0f - 175.0f * powf(k / 7.0f, 1.7f);
        line(0, yy, 1000, yy, 1.0f, 0.62f, 0.45f, 0.55f, 0.30f);
    }

    // warm light pools on the ground
    glowDisc(SC_FISH,   150, 190, 1.0f, 0.65f, 0.35f, 0.26f);
    glowDisc(SC_RING,   150, 190, 1.0f, 0.65f, 0.35f, 0.26f);
    glowDisc(SC_TARGET, 150, 190, 1.0f, 0.65f, 0.35f, 0.26f);
}

void drawLanternString()
{
    glLineWidth(2.0f);
    glColor3f(0.1f, 0.08f, 0.1f);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= 40; i++) { float x = i * 25.0f; glVertex2f(x, stringY(x)); }
    glEnd();

    // back row: smaller warm lanterns for depth
    for (int i = 0; i < 17; i++)
    {
        float x = 25.0f + i * 59.0f;
        float y = stringY(x) - 62.0f - 14.0f * sinf(PI * x / 1000.0f) * 0.0f;
        glowDisc(x, y, 26, 1.0f, 0.65f, 0.25f, 0.30f);
        disc(x, y, 7, 10, 1.0f, 0.62f + 0.2f * (i % 2), 0.22f);
        line(x - 6, y, x + 6, y, 1.0f, 0.8f, 0.3f, 0.15f, 0.7f);
    }

    for (int i = 0; i < 11; i++)
    {
        float x = 45.0f + i * 91.0f;
        int m = i % 3;
        if (m == 0)      lantern(x, 0.95f, 0.92f, 0.18f, 0.18f, i);
        else if (m == 1) lantern(x, 0.85f, 0.98f, 0.55f, 0.12f, i);
        else             lantern(x, 0.85f, 0.95f, 0.85f, 0.50f, i);
    }
}

void stallBack(float cx)
{
    gradRect(cx - 138, 250, 276, 150, 0.33f, 0.20f, 0.13f, 0.46f, 0.29f, 0.19f);
    for (int i = 1; i < 6; i++)                       // wood planks
        line(cx - 138, 250 + i * 25.0f, cx + 138, 250 + i * 25.0f, 1.0f, 0.2f, 0.12f, 0.08f, 0.5f);
    glowDisc(cx, 330, 200, 1.0f, 0.70f, 0.30f, 0.34f);
}

void stallFront(float cx, float rr, float rg, float rb, const char* label)
{
    // posts
    rect(cx - 148, 175, 10, 230, 0.26f, 0.15f, 0.09f);
    rect(cx + 138, 175, 10, 230, 0.26f, 0.15f, 0.09f);

    // counter
    gradRect(cx - 146, 175, 292, 75, 0.55f, 0.37f, 0.22f, 0.66f, 0.46f, 0.28f);
    rect(cx - 152, 243, 304, 10, 0.78f, 0.56f, 0.34f);
    rect(cx - 152, 241, 304, 2, 0.35f, 0.22f, 0.12f);

    // sign
    rect(cx - 104, 186, 208, 38, 0.95f, 0.78f, 0.40f);
    rect(cx - 101, 189, 202, 32, 0.10f, 0.12f, 0.27f);
    textC(cx, 201, label, GLUT_BITMAP_HELVETICA_12, 1.0f, 0.95f, 0.8f);

    // roof
    glBegin(GL_QUADS);
        glColor3f(rr * 0.75f, rg * 0.75f, rb * 0.75f);
        glVertex2f(cx - 162, 398); glVertex2f(cx + 162, 398);
        glColor3f(rr, rg, rb);
        glVertex2f(cx + 126, 460); glVertex2f(cx - 126, 460);
    glEnd();
    line(cx - 144, 429, cx + 144, 429, 1.5f, rr * 0.6f, rg * 0.6f, rb * 0.6f, 0.6f);

    // striped valance
    for (int i = 0; i < 16; i++)
    {
        if (i % 2 == 0) rect(cx - 160 + i * 20.0f, 378, 20, 22, 0.97f, 0.94f, 0.86f);
        else            rect(cx - 160 + i * 20.0f, 378, 20, 22, rr * 0.9f, rg * 0.9f, rb * 0.9f);
    }
    rect(cx - 160, 376, 320, 3, 0.2f, 0.12f, 0.08f, 0.5f);
}

// ---------- fish ----------
void drawFish(const Fish& f)
{
    float s = f.size, d = (float)f.dir;
    float wag = sinf(T * 9.0f + f.ph) * 3.0f * s;
    tri(f.x - d * 8 * s, f.y, f.x - d * 21 * s, f.y + 10 * s + wag, f.x - d * 21 * s, f.y - 10 * s + wag,
        f.r * 0.9f, f.g * 0.85f, f.b * 0.85f, 0.95f);
    tri(f.x - d * 4 * s, f.y + 5 * s, f.x + d * 2 * s, f.y + 13 * s, f.x - d * 10 * s, f.y + 8 * s,
        f.r * 0.85f, f.g * 0.8f, f.b * 0.8f);
    disc(f.x, f.y, 12 * s, 7.5f * s, f.r, f.g, f.b);
    disc(f.x + d * 1 * s, f.y - 2.5f * s, 8 * s, 3.5f * s, 1.0f, 1.0f, 1.0f, 0.22f);
    disc(f.x + d * 7 * s, f.y + 2 * s, 2.6f * s, 2.6f * s, 1.0f, 1.0f, 1.0f);
    disc(f.x + d * 7.5f * s, f.y + 2 * s, 1.3f * s, 1.3f * s, 0.05f, 0.05f, 0.05f);
}

void drawPoi(float x, float y)
{
    // long bamboo handle from the kid's fist up to the paper scooper
    line(FISH_HAND_X - 6, FISH_HAND_Y - 14, x + 12, y + 10, 5.0f, 0.82f, 0.66f, 0.38f);
    line(FISH_HAND_X - 6, FISH_HAND_Y - 14, x + 12, y + 10, 1.5f, 1.0f, 0.92f, 0.62f, 0.6f);
    disc(x, y, 22, 17, 1.0f, 1.0f, 1.0f, 0.30f);
    ellipseRing(x, y, 23, 18, 3.5f, 0.95f, 0.90f, 0.78f);
    disc(FISH_HAND_X, FISH_HAND_Y, 4.4f, 4.4f, 0.98f, 0.82f, 0.70f);   // fist on the handle
}

void drawGoldfishContents()
{
    // wooden tank base
    rect(TANK_L - 8, 250, TANK_R - TANK_L + 16, 14, 0.42f, 0.26f, 0.15f);

    // water
    gradRect(TANK_L, TANK_B, TANK_R - TANK_L, TANK_T - TANK_B, 0.18f, 0.55f, 0.88f, 0.40f, 0.80f, 0.97f);

    // swaying weeds
    for (int i = 0; i < 5; i++)
    {
        float wx = TANK_L + 22 + i * 44.0f;
        glLineWidth(3.0f);
        glColor4f(0.15f, 0.65f, 0.40f, 0.8f);
        glBegin(GL_LINE_STRIP);
        for (int k = 0; k <= 6; k++)
            glVertex2f(wx + sinf(T * 1.5f + i + k * 0.6f) * 3.0f * k * 0.4f, TANK_B + k * 5.0f);
        glEnd();
    }

    // fish
    for (int i = 0; i < 6; i++) drawFish(fish[i]);

    // bubbles
    glLineWidth(1.0f);
    for (int i = 0; i < 7; i++)
    {
        float bx = TANK_L + 18 + i * 31.0f + sinf(T + i) * 3.0f;
        float by = TANK_B + 6 + fmodf(T * (14.0f + i * 3.0f) + i * 37.0f, 78.0f);
        glColor4f(0.9f, 1.0f, 1.0f, 0.7f);
        glBegin(GL_LINE_LOOP);
        for (int k = 0; k < 14; k++)
        {
            float a = 2 * PI * k / 14.0f;
            glVertex2f(bx + cosf(a) * 2.6f, by + sinf(a) * 2.6f);
        }
        glEnd();
    }

    // surface ripples
    for (int i = 0; i < 10; i++)
    {
        if (!tankRip[i].act) continue;
        float u = tankRip[i].age / 1.4f;
        float rx = 6 + u * 40.0f;
        ellipseRing(tankRip[i].x, tankRip[i].y, rx, rx * 0.22f, 1.6f, 1, 1, 1, 0.7f * (1 - u));
    }

    // glass: surface line, edges and highlight
    rect(TANK_L, TANK_T - 5, TANK_R - TANK_L, 5, 0.75f, 0.95f, 1.0f, 0.55f);
    line(TANK_L, TANK_B, TANK_L, TANK_T, 3.0f, 0.85f, 0.95f, 1.0f, 0.8f);
    line(TANK_R, TANK_B, TANK_R, TANK_T, 3.0f, 0.85f, 0.95f, 1.0f, 0.8f);
    line(TANK_L + 14, TANK_T - 12, TANK_L + 58, TANK_B + 10, 5.0f, 1, 1, 1, 0.12f);
}

void drawRingContents()
{
    float cx = SC_RING;

    // prize boxes on the back shelf
    rect(cx - 126, 318, 252, 6, 0.30f, 0.18f, 0.10f);
    const float pc[5][3] = { {0.9f,0.3f,0.4f}, {0.3f,0.7f,0.9f}, {1.0f,0.8f,0.25f}, {0.5f,0.8f,0.4f}, {0.8f,0.5f,0.9f} };
    for (int i = 0; i < 5; i++)
    {
        float bx = cx - 112 + i * 50.0f;
        rect(bx, 324, 38, 34, pc[i][0], pc[i][1], pc[i][2]);
        rect(bx + 16, 324, 6, 34, 1.0f, 0.95f, 0.8f, 0.8f);
        rect(bx, 338, 38, 6, 1.0f, 0.95f, 0.8f, 0.8f);
    }

    // peg board
    rect(cx - 128, 250, 256, 14, 0.50f, 0.32f, 0.18f);
    for (int i = 0; i < 5; i++)
    {
        float px = PEG_X[i];
        rect(px - 4, 264, 8, 42, 0.98f, 0.93f, 0.74f);
        rect(px + 1, 264, 3, 42, 0.8f, 0.72f, 0.5f);
        disc(px, 306, 5.0f, 3.5f, 0.98f, 0.93f, 0.74f);
    }

    // landed rings
    for (int k = 0; k < 24; k++)
    {
        RingRest& r = resting[k];
        if (!r.act) continue;
        float a = r.age > 4.8f ? clamp01((6.0f - r.age) / 1.2f) : 1.0f;
        float y = r.y;
        if (r.hit) y += 16.0f * (1.0f - clamp01(r.age * 5.0f)) - 12.0f;
        ellipseRing(r.x, y, 20, 8, 4.0f, r.r, r.g, r.b, a);
    }
}

void drawTarget(float x, float y, float scale)
{
    float s = scale;
    disc(x, y, 31 * s, 31 * s, 0.35f, 0.2f, 0.1f);          // rim
    disc(x, y, 29 * s, 29 * s, 0.92f, 0.13f, 0.16f);
    disc(x, y, 23 * s, 23 * s, 0.98f, 0.97f, 0.94f);
    disc(x, y, 17 * s, 17 * s, 0.92f, 0.13f, 0.16f);
    disc(x, y, 10 * s, 10 * s, 0.98f, 0.97f, 0.94f);
    disc(x, y, 4.5f * s, 4.5f * s, 0.92f, 0.13f, 0.16f);
}

void drawTargetContents()
{
    float cx = SC_TARGET;

    // plank board
    rect(cx - 124, 262, 248, 112, 0.80f, 0.62f, 0.34f);
    for (int i = 1; i < 4; i++)
        line(cx - 124, 262 + i * 28.0f, cx + 124, 262 + i * 28.0f, 1.0f, 0.5f, 0.35f, 0.18f, 0.6f);

    for (int i = 0; i < 3; i++)
    {
        float x = targetX(i), y = targetY(i);
        float s = 1.0f;
        if (tgtHit[i] >= 0.0f) s = 1.0f + 0.2f * expf(-6.0f * tgtHit[i]) * cosf(22.0f * tgtHit[i]);
        rect(x - 3, 262, 6, y - 262 - 25, 0.4f, 0.25f, 0.14f);   // stick
        drawTarget(x, y, s);

        if (tgtHit[i] >= 0.0f && tgtHit[i] < 0.6f)
        {
            float u = tgtHit[i] / 0.6f;
            ellipseRing(x, y, 30 + u * 40, 30 + u * 40, 3.0f, 1.0f, 0.9f, 0.3f, 1.0f - u);
            textC(x, y + 36 + u * 22, "HIT!", GLUT_BITMAP_HELVETICA_18, 1.0f, 0.95f, 0.4f, 1.0f - u);
        }
    }
}

void drawArm(float sx, float sy, float hx, float hy, float s, float out,
             float br, float bg, float bb)
{
    float ex = (sx + hx) * 0.5f + out * 4.5f * s;
    float ey = (sy + hy) * 0.5f - 2.0f * s;
    line(sx, sy, ex, ey, 7.0f * s, br, bg, bb);                     // sleeve
    line(ex, ey, hx, hy, 3.2f * s, 0.98f, 0.82f, 0.70f);            // forearm
    disc(hx, hy, 2.8f * s, 2.8f * s, 0.98f, 0.82f, 0.70f);          // hand
}

// A child seen from behind (facing the stall). rUp/lUp: raise that hand to (hx,hy).
void drawKidBack(float x, float y, float s,
                 float br, float bg, float bb, float orr, float og, float ob, float ph,
                 bool rUp, float rhx, float rhy, bool lUp, float lhx, float lhy)
{
    y += sinf(T * 2.2f + ph) * 1.0f * s;
    const float sk[3] = { 0.98f, 0.82f, 0.70f };

    // legs and sandals
    rect(x - 5 * s, y, 3.5f * s, 13 * s, sk[0], sk[1], sk[2]);
    rect(x + 1.5f * s, y, 3.5f * s, 13 * s, sk[0], sk[1], sk[2]);
    rect(x - 6.5f * s, y - 1 * s, 6 * s, 2.5f * s, 0.25f, 0.15f, 0.10f);
    rect(x + 0.5f * s, y - 1 * s, 6 * s, 2.5f * s, 0.25f, 0.15f, 0.10f);

    // yukata
    glColor3f(br, bg, bb);
    glBegin(GL_QUADS);
        glVertex2f(x - 10 * s, y + 13 * s); glVertex2f(x + 10 * s, y + 13 * s);
        glVertex2f(x + 7.5f * s, y + 47 * s); glVertex2f(x - 7.5f * s, y + 47 * s);
    glEnd();
    disc(x - 4 * s, y + 20 * s, 1.4f * s, 1.4f * s, 1, 1, 1, 0.5f);
    disc(x + 4 * s, y + 16 * s, 1.4f * s, 1.4f * s, 1, 1, 1, 0.5f);
    disc(x - 4 * s, y + 38 * s, 1.4f * s, 1.4f * s, 1, 1, 1, 0.5f);
    disc(x + 5 * s, y + 36 * s, 1.4f * s, 1.4f * s, 1, 1, 1, 0.5f);

    // obi with a big bow on the back
    rect(x - 8.5f * s, y + 26 * s, 17 * s, 7 * s, orr, og, ob);
    rect(x - 3.5f * s, y + 18 * s, 2.6f * s, 9 * s, orr * 0.9f, og * 0.9f, ob * 0.9f);
    rect(x + 1.0f * s, y + 18 * s, 2.6f * s, 9 * s, orr * 0.9f, og * 0.9f, ob * 0.9f);
    disc(x - 5.5f * s, y + 31 * s, 5.5f * s, 4.2f * s, orr, og, ob);
    disc(x + 5.5f * s, y + 31 * s, 5.5f * s, 4.2f * s, orr, og, ob);
    disc(x, y + 30 * s, 2.6f * s, 2.6f * s, orr * 0.7f, og * 0.7f, ob * 0.7f);

    // collar and neck
    rect(x - 5 * s, y + 45.5f * s, 10 * s, 2.2f * s, 1.0f, 1.0f, 1.0f, 0.9f);
    rect(x - 3.2f * s, y + 46.5f * s, 6.4f * s, 5 * s, sk[0], sk[1], sk[2]);

    // arms
    float rsx = x + 8.5f * s, lsx = x - 8.5f * s, sy = y + 42 * s;
    if (rUp) drawArm(rsx, sy, rhx, rhy, s, 1.0f, br, bg, bb);
    else     drawArm(rsx, sy, x + 12 * s, y + 27 * s, s, 1.0f, br, bg, bb);
    if (lUp) drawArm(lsx, sy, lhx, lhy, s, -1.0f, br, bg, bb);
    else     drawArm(lsx, sy, x - 12 * s, y + 27 * s, s, -1.0f, br, bg, bb);

    // back of the head: hair, bun and hair ornament (no face - the child looks at the game)
    disc(x, y + 58 * s, 10 * s, 9.5f * s, 0.10f, 0.07f, 0.08f);
    disc(x - 3 * s, y + 60.5f * s, 3.5f * s, 2.0f * s, 0.35f, 0.28f, 0.35f, 0.5f);
    disc(x, y + 67 * s, 4.6f * s, 4.6f * s, 0.10f, 0.07f, 0.08f);
    disc(x + 5 * s, y + 63 * s, 1.9f * s, 1.9f * s, 1.0f, 0.5f, 0.7f);
}


void blossomCluster(float cx, float cy, float sc, float seed)
{
    for (int i = 0; i < 30; i++)
    {
        float a = i * 2.399f + seed;
        float d = sqrtf((i + 1) / 30.0f) * 62.0f * sc;
        float x = cx + cosf(a) * d * 1.25f;
        float y = cy + sinf(a) * d * 0.9f;
        float rad = (15.0f + (i * 7) % 13) * sc;
        float sh = ((i * 37) % 5) / 5.0f;
        disc(x, y, rad, rad, 0.98f, 0.55f + 0.14f * sh, 0.74f + 0.1f * sh, 0.95f);
        disc(x - rad * 0.25f, y + rad * 0.25f, rad * 0.45f, rad * 0.45f, 1.0f, 0.80f, 0.90f, 0.45f);
    }
}

void drawBlossoms()
{
    // branches
    line(0, 455, 95, 525, 7.0f, 0.22f, 0.12f, 0.12f);
    line(0, 395, 70, 445, 5.0f, 0.22f, 0.12f, 0.12f);
    line(1000, 455, 905, 525, 7.0f, 0.22f, 0.12f, 0.12f);
    line(1000, 395, 930, 445, 5.0f, 0.22f, 0.12f, 0.12f);
    // pink clusters in both top corners
    for (int side = 0; side < 2; side++)
    {
        float m = side == 0 ? 1.0f : -1.0f;
        float o = side == 0 ? 0.0f : 1000.0f;
        blossomCluster(o + m * 40,  545, 1.35f, side * 2.0f);
        blossomCluster(o + m * 120, 505, 1.05f, 1.0f + side);
        blossomCluster(o + m * 25,  440, 1.10f, 2.0f + side);
        blossomCluster(o + m * 150, 572, 0.90f, 3.0f + side);
        blossomCluster(o + m * 70,  400, 0.80f, 4.0f + side);
    }
}

void drawPetals()
{
    for (int i = 0; i < 30; i++)
    {
        float px = fmodf(i * 131.0f + T * (10.0f + (i % 5) * 4.0f) + sinf(T + i) * 30.0f, 1000.0f);
        float py = 600.0f - fmodf(i * 53.0f + T * (22.0f + (i % 4) * 6.0f), 600.0f);
        disc(px, py, 4.0f, 2.2f, 1.0f, 0.72f, 0.84f, 0.85f);
    }
}

void drawVisitors()
{
    // children watching the games (backs to us, one cheering with a raised hand)
    drawKidBack(255, 62, 1.5f, 0.28f, 0.33f, 0.72f, 0.95f, 0.95f, 0.98f, 1.5f,
                true, 255 + 22, 162 + 8 * sinf(T * 5.0f), false, 0, 0);
    drawKidBack(405, 70, 1.5f, 1.0f, 0.60f, 0.10f, 0.15f, 0.10f, 0.10f, 2.8f,
                false, 0, 0, false, 0, 0);
    drawKidBack(610, 64, 1.5f, 0.10f, 0.62f, 0.55f, 1.0f, 0.50f, 0.30f, 4.0f,
                false, 0, 0, true, 610 - 22, 162 + 8 * sinf(T * 5.0f + 1.0f));
    drawKidBack(740, 68, 1.5f, 0.55f, 0.20f, 0.70f, 1.0f, 0.85f, 0.25f, 5.1f,
                false, 0, 0, false, 0, 0);
    drawKidBack(930, 62, 1.5f, 0.95f, 0.45f, 0.65f, 0.98f, 0.98f, 0.98f, 0.7f,
                false, 0, 0, false, 0, 0);

    float hx, hy;

    // goldfish kid: holds the bamboo handle (the scooper itself is drawn by drawPoi)
    drawKidBack(KID_FISH_X, KID_FEET, KID_SCALE, 0.92f, 0.20f, 0.45f, 1.0f, 0.85f, 0.25f, 0.0f,
                true, FISH_HAND_X, FISH_HAND_Y, false, 0, 0);

    // ring-toss kid: holds a ring, swings, and the ring leaves the hand
    ringHand(hx, hy);
    drawKidBack(KID_RING_X, KID_FEET, KID_SCALE, 0.95f, 0.78f, 0.25f, 0.55f, 0.15f, 0.35f, 2.0f,
                true, hx, hy, false, 0, 0);
    if (pendThrow || armT < 0.0f)
        ellipseRing(hx + 2, hy + 5, 17, 7, 3.6f, heldCol[0], heldCol[1], heldCol[2]);

    // target kid: holds the toy cork gun; the ball leaves its muzzle
    gunHand(hx, hy);
    drawKidBack(KID_TGT_X, KID_FEET, KID_SCALE, 0.55f, 0.27f, 0.72f, 1.0f, 0.85f, 0.25f, 3.5f,
                true, hx, hy, false, 0, 0);
    float dx = aimX - hx, dy = aimY - hy;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1.0f) len = 1.0f;
    dx /= len; dy /= len;
    float mx = hx + dx * 30.0f, my = hy + dy * 30.0f;
    line(hx - dx * 8, hy - dy * 8, mx, my, 9.0f, 0.18f, 0.18f, 0.22f);
    line(hx, hy, mx, my, 3.0f, 0.45f, 0.45f, 0.50f);
    disc(mx, my, 4.6f, 4.6f, 0.92f, 0.72f, 0.35f);
    disc(hx, hy, 4.6f, 4.6f, 0.98f, 0.82f, 0.70f);                  // fist on the grip
    if (gunKick >= 0.0f && gunKick < 0.1f)
        glowDisc(mx, my, 24, 1.0f, 0.9f, 0.4f, 0.8f * (1.0f - gunKick / 0.1f));
}

void drawRingsInFlight()
{
    for (int i = 0; i < 6; i++)
    {
        RingFly& r = flying[i];
        if (!r.act) continue;
        float u = r.t / r.dur;
        float x = mixf(r.x0, r.x1, u);
        float y = mixf(r.y0, r.y1, u) + 115.0f * sinf(PI * u);
        float sc = mixf(1.5f, 1.0f, u);
        float ry = (6.0f + 5.0f * sinf(PI * u)) * sc;
        ellipseRing(x, y, 20 * sc, ry, 4.5f * sc, r.r, r.g, r.b);
    }
}

void drawDarts()
{
    for (int i = 0; i < 4; i++)
    {
        Dart& d = darts[i];
        if (!d.act) continue;
        float u = d.t / d.dur;
        for (int k = 6; k >= 0; k--)
        {
            float uu = u - k * 0.045f;
            if (uu < 0.0f) continue;
            float x = mixf(d.x0, d.x1, uu);
            float y = mixf(d.y0, d.y1, uu) + 14.0f * sinf(PI * uu);
            float a = 1.0f - k / 7.0f;
            disc(x, y, 5.0f - k * 0.5f, 5.0f - k * 0.5f, 1.0f, 0.88f, 0.2f, a);
        }
    }
}

void drawFrontLayer()
{
    // bottom information strip
    rect(0, 0, 1000, 34, 0.15f, 0.10f, 0.13f, 0.95f);
    line(0, 34, 1000, 34, 2.0f, 0.95f, 0.72f, 0.35f);

    char buf[64];
    std::sprintf(buf, "Goldfish caught: %d", caughtCount);
    textC(SC_FISH, 11, buf, GLUT_BITMAP_HELVETICA_12, 1.0f, 0.93f, 0.75f);
    std::sprintf(buf, "Rings on pegs: %d", ringHits);
    textC(SC_RING, 11, buf, GLUT_BITMAP_HELVETICA_12, 1.0f, 0.93f, 0.75f);
    std::sprintf(buf, "Targets hit: %d", targetHits);
    textC(SC_TARGET, 11, buf, GLUT_BITMAP_HELVETICA_12, 1.0f, 0.93f, 0.75f);

    // title
    textC(502, 481, "KIDS' PLAY ZONE", GLUT_BITMAP_TIMES_ROMAN_24, 0.1f, 0.05f, 0.15f, 0.8f);
    textC(500, 483, "KIDS' PLAY ZONE", GLUT_BITMAP_TIMES_ROMAN_24, 1.0f, 0.92f, 0.65f);
}
} // namespace

// =====================================================================
//  public API
// =====================================================================
void kidsPlayzoneMouseMove(int mx, int my, int winW, int winH)
{
    if (winW <= 0 || winH <= 0) return;
    mouseX = mx * 1000.0f / winW;
    mouseY = (winH - my) * 600.0f / winH;
    lastMove = T;
}

void kidsPlayzoneMouseClick(int mx, int my, int winW, int winH)
{
    if (winW <= 0 || winH <= 0) return;
    handleClick(mx * 1000.0f / winW, (winH - my) * 600.0f / winH);
}

void resetKidsPlayzone()
{
    lastT = -100.0f;      // next draw re-initialises all game state
}

void drawKidsPlayzone()
{
    float now = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    float dt = now - lastT;
    if (dt > 4.0f) { initState(); dt = 0.016f; }   // scene was re-entered -> fresh start
    if (dt > 0.05f) dt = 0.05f;
    if (dt < 0.0f)  dt = 0.0f;
    lastT = now; T = now;

    updateFish(dt);
    updateRings(dt);
    updateTargets(dt);

    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_LINE_BIT | GL_POINT_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);

    drawBackground();
    drawLanternString();

    // stall 1: goldfish scooping (blue roof)
    stallBack(SC_FISH);
    drawGoldfishContents();
    stallFront(SC_FISH, 0.17f, 0.52f, 0.78f, "GOLDFISH SCOOPING");

    // stall 2: ring toss (yellow roof)
    stallBack(SC_RING);
    drawRingContents();
    stallFront(SC_RING, 0.85f, 0.68f, 0.08f, "RING TOSS");

    // stall 3: target game (purple roof)
    stallBack(SC_TARGET);
    drawTargetContents();
    stallFront(SC_TARGET, 0.55f, 0.27f, 0.72f, "TARGET GAME");

    drawBlossoms();
    drawVisitors();
    drawPoi(poiX, poiY);
    drawRingsInFlight();
    drawDarts();
    drawPetals();
    drawFrontLayer();

    glLineWidth(1.0f);
    glPopAttrib();
}