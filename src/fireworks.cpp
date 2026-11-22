#include "fireworks.h"
#include "audio.h"
#include <GL/freeglut.h>
#include <cmath>
#include <cstdlib>
#include <vector>

// =====================================================================
//  HANABI - FIREWORKS CELEBRATION
//  Particle system (position, velocity, colour, lifetime, gravity, drag)
//  Patterns: Burst, Peony, Ring, Willow, Heart
//  River: mirrored reflections + expanding ripples
// =====================================================================

namespace
{
const float PI    = 3.14159265f;
const float WATER = 228.0f;      // y of the river's top edge

float frand(float a, float b) { return a + (b - a) * (std::rand() / (float)RAND_MAX); }
float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
float mixf(float a, float b, float t) { return a + (b - a) * t; }

// ----------------------------------------------------------------
// drawing helpers
// ----------------------------------------------------------------
void rect(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f)
{
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
        glVertex2f(x, y); glVertex2f(x + w, y);
        glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}

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
        for (int i = 0; i <= 36; i++)
        {
            float t = 2.0f * PI * i / 36.0f;
            glVertex2f(x + cosf(t) * rx, y + sinf(t) * ry);
        }
    glEnd();
}

void glowDisc(float x, float y, float rad, float r, float g, float b, float a)
{
    glBegin(GL_TRIANGLE_FAN);
        glColor4f(r, g, b, a); glVertex2f(x, y);
        glColor4f(r, g, b, 0.0f);
        for (int i = 0; i <= 36; i++)
        {
            float t = 2.0f * PI * i / 36.0f;
            glVertex2f(x + cosf(t) * rad, y + sinf(t) * rad);
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

void textC(float cx, float y, const char* s, void* font, float r, float g, float b, float a)
{
    float w = (float)glutBitmapLength(font, (const unsigned char*)s);
    glColor4f(r, g, b, a);
    glRasterPos2f(cx - w * 0.5f, y);
    for (int i = 0; s[i]; i++) glutBitmapCharacter(font, s[i]);
}

// ----------------------------------------------------------------
// particle system
// ----------------------------------------------------------------
struct Particle
{
    float x, y, vx, vy;
    float life, maxLife;
    float r, g, b;
    float size, drag, grav, bright;
    bool  trail, twinkle;
};
struct Rocket  { float x, y, vy, targetY; int pattern; float r, g, b, r2, g2, b2; };
struct Flash   { float x, y, age, r, g, b; };
struct Ripple  { float x, age, r, g, b; };
struct Delayed { float t; int sfx; float vol; };

enum Pattern { P_BURST = 0, P_PEONY, P_RING, P_WILLOW, P_HEART, P_COUNT };

const float PAL[8][3] = {
    {1.00f, 0.25f, 0.32f}, {1.00f, 0.80f, 0.25f}, {0.30f, 0.80f, 1.00f}, {0.72f, 0.45f, 1.00f},
    {0.40f, 1.00f, 0.55f}, {1.00f, 0.50f, 0.80f}, {1.00f, 0.55f, 0.15f}, {1.00f, 1.00f, 0.90f}
};

std::vector<Particle> parts;
std::vector<Rocket>   rockets;
std::vector<Flash>    flashes;
std::vector<Ripple>   ripples;
std::vector<Delayed>  pending;

float T = 0.0f, lastT = -100.0f;
float launchTimer = 0.5f;
int   patternCounter = 0;
float lastLaunchX = 500.0f;
float finaleT = -1.0f;
bool  barrageDone = false;

const size_t MAX_PARTICLES = 7000;

void addP(float x, float y, float vx, float vy, float life,
          float r, float g, float b, float size, float drag, float grav,
          bool trail, bool twinkle, float bright = 1.0f)
{
    if (parts.size() >= MAX_PARTICLES) return;
    Particle p;
    p.x = x; p.y = y; p.vx = vx; p.vy = vy;
    p.life = life; p.maxLife = life;
    p.r = r; p.g = g; p.b = b;
    p.size = size; p.drag = drag; p.grav = grav; p.bright = bright;
    p.trail = trail; p.twinkle = twinkle;
    parts.push_back(p);
}

void explode(const Rocket& rk)
{
    Flash f = { rk.x, rk.y, 0.0f, rk.r, rk.g, rk.b };
    flashes.push_back(f);
    Ripple rp = { rk.x, 0.0f, rk.r, rk.g, rk.b };
    ripples.push_back(rp);

    float d = frand(0.12f, 0.30f);                    // sound arrives after the light
    Delayed boom = { d, SFX_BOOM, 0.80f };
    Delayed crk  = { d + 0.35f, SFX_CRACKLE, 0.35f };
    pending.push_back(boom);
    pending.push_back(crk);

    switch (rk.pattern)
    {
    case P_BURST:
        for (int i = 0; i < 100; i++)
        {
            float a = frand(0.0f, 2.0f * PI), sp = frand(60.0f, 230.0f);
            bool second = frand(0, 1) < 0.25f;
            addP(rk.x, rk.y, cosf(a) * sp, sinf(a) * sp, frand(1.4f, 2.2f),
                 second ? rk.r2 : rk.r, second ? rk.g2 : rk.g, second ? rk.b2 : rk.b,
                 3.5f, 0.9f, 70.0f, false, true);
        }
        break;

    case P_PEONY:
        for (int i = 0; i < 170; i++)
        {
            float a = 2.0f * PI * i / 170.0f + frand(-0.05f, 0.05f);
            float sp = 190.0f * sqrtf(frand(0.05f, 1.0f));
            bool inner = sp < 90.0f;
            addP(rk.x, rk.y, cosf(a) * sp, sinf(a) * sp, frand(1.8f, 2.4f),
                 inner ? rk.r2 : rk.r, inner ? rk.g2 : rk.g, inner ? rk.b2 : rk.b,
                 3.5f, 1.4f, 45.0f, false, true);
        }
        break;

    case P_RING:
        for (int i = 0; i < 72; i++)
        {
            float a = 2.0f * PI * i / 72.0f;
            addP(rk.x, rk.y, cosf(a) * 170.0f, sinf(a) * 170.0f, 2.0f,
                 rk.r, rk.g, rk.b, 4.0f, 1.2f, 30.0f, false, false);
        }
        for (int i = 0; i < 36; i++)
        {
            float a = 2.0f * PI * i / 36.0f;
            addP(rk.x, rk.y, cosf(a) * 90.0f, sinf(a) * 90.0f, 2.0f,
                 rk.r2, rk.g2, rk.b2, 3.5f, 1.2f, 30.0f, false, false);
        }
        break;

    case P_WILLOW:
        for (int i = 0; i < 120; i++)
        {
            float a = frand(0.0f, 2.0f * PI), sp = frand(40.0f, 150.0f);
            addP(rk.x, rk.y, cosf(a) * sp, sinf(a) * sp, frand(3.0f, 4.2f),
                 1.0f, 0.78f, 0.30f, 3.5f, 0.9f, 55.0f, true, false);
        }
        break;

    case P_HEART:
        for (int i = 0; i < 80; i++)
        {
            float t = 2.0f * PI * i / 80.0f;
            float hx = 16.0f * powf(sinf(t), 3.0f);
            float hy = 13.0f * cosf(t) - 5.0f * cosf(2 * t) - 2.0f * cosf(3 * t) - cosf(4 * t);
            addP(rk.x, rk.y, hx * 8.5f, hy * 8.5f + 10.0f, 2.2f,
                 1.0f, 0.35f, 0.55f, 4.0f, 1.5f, 30.0f, false, false);
        }
        break;
    }
}

void launchRocket(int pattern, float x)
{
    Rocket rk;
    rk.x = x; rk.y = WATER + 8.0f; rk.vy = 290.0f;
    rk.targetY = frand(340.0f, 490.0f);
    rk.pattern = pattern;
    int c1 = std::rand() % 8, c2 = (c1 + 1 + std::rand() % 7) % 8;
    rk.r = PAL[c1][0]; rk.g = PAL[c1][1]; rk.b = PAL[c1][2];
    rk.r2 = PAL[c2][0]; rk.g2 = PAL[c2][1]; rk.b2 = PAL[c2][2];
    rockets.push_back(rk);
    audioPlay(SFX_WHISTLE, 0.35f);
}

float nextLaunchX()
{
    lastLaunchX = 150.0f + fmodf(lastLaunchX - 150.0f + frand(160.0f, 420.0f), 700.0f);
    return lastLaunchX;
}

void update(float dt, bool finale)
{
    // ---- launching
    launchTimer -= dt;
    if (launchTimer <= 0.0f)
    {
        int count = 1;
        if (finale) count = 1 + std::rand() % 2;
        for (int i = 0; i < count; i++)
        {
            int pat = finale ? std::rand() % P_COUNT : (patternCounter++ % P_COUNT);
            launchRocket(pat, nextLaunchX());
        }
        launchTimer = finale ? frand(0.15f, 0.40f) : frand(0.8f, 1.6f);
    }
    if (finale && finaleT > 0.3f && !barrageDone)      // grand opening barrage
    {
        barrageDone = true;
        for (int i = 0; i < 7; i++)
            launchRocket(std::rand() % P_COUNT, 110.0f + i * 130.0f);
    }

    // ---- rockets
    for (size_t i = 0; i < rockets.size();)
    {
        Rocket& r = rockets[i];
        r.y += r.vy * dt;
        r.x += sinf(T * 30.0f + i) * 0.3f;
        addP(r.x + frand(-1.5f, 1.5f), r.y, frand(-8, 8), -30.0f, 0.45f,
             1.0f, 0.70f, 0.35f, 2.5f, 1.0f, 0.0f, false, false, 0.7f);
        if (r.y >= r.targetY)
        {
            explode(r);
            rockets[i] = rockets.back();
            rockets.pop_back();
        }
        else i++;
    }

    // ---- particles
    std::vector<Particle> spawn;
    for (size_t i = 0; i < parts.size();)
    {
        Particle& p = parts[i];
        p.life -= dt;
        if (p.life <= 0.0f) { parts[i] = parts.back(); parts.pop_back(); continue; }

        float k = expf(-p.drag * dt);
        p.vx *= k;
        p.vy = p.vy * k - p.grav * dt;
        p.x += p.vx * dt;
        p.y += p.vy * dt;

        if (p.trail && (std::rand() % 2 == 0))
        {
            Particle t = p;
            t.life = t.maxLife = 0.7f;
            t.vx *= 0.1f; t.vy *= 0.1f;
            t.size = 2.2f; t.trail = false; t.twinkle = false;
            t.bright = 0.55f; t.drag = 1.0f; t.grav = 10.0f;
            spawn.push_back(t);
        }
        i++;
    }
    for (size_t i = 0; i < spawn.size() && parts.size() < MAX_PARTICLES; i++) parts.push_back(spawn[i]);

    // ---- flashes, ripples, delayed sounds
    for (size_t i = 0; i < flashes.size();)
    {
        flashes[i].age += dt;
        if (flashes[i].age > 0.7f) { flashes[i] = flashes.back(); flashes.pop_back(); }
        else i++;
    }
    for (size_t i = 0; i < ripples.size();)
    {
        ripples[i].age += dt;
        if (ripples[i].age > 2.4f) { ripples[i] = ripples.back(); ripples.pop_back(); }
        else i++;
    }
    for (size_t i = 0; i < pending.size();)
    {
        pending[i].t -= dt;
        if (pending[i].t <= 0.0f)
        {
            audioPlay(pending[i].sfx, pending[i].vol);
            pending[i] = pending.back();
            pending.pop_back();
        }
        else i++;
    }
}

// ----------------------------------------------------------------
// scenery
// ----------------------------------------------------------------
void skyColor(float y, float& r, float& g, float& b)
{
    float t = clamp01((y - WATER) / (600.0f - WATER));
    r = mixf(0.10f, 0.012f, t);
    g = mixf(0.10f, 0.016f, t);
    b = mixf(0.27f, 0.070f, t);
}

void mountain(float base, float amp, float seed, float r, float g, float b)
{
    glColor3f(r, g, b);
    glBegin(GL_TRIANGLE_STRIP);
    for (int i = 0; i <= 125; i++)
    {
        float x = i * 8.0f;
        float h = base + amp * (0.55f * sinf(x * 0.006f + seed)
                              + 0.30f * sinf(x * 0.015f + seed * 2.3f)
                              + 0.15f * sinf(x * 0.040f + seed * 5.0f));
        glVertex2f(x, WATER);
        glVertex2f(x, h);
    }
    glEnd();
}

void pine(float x, float baseY, float h)
{
    glColor3f(0.015f, 0.045f, 0.06f);
    glBegin(GL_QUADS);
        glVertex2f(x - h * 0.03f, baseY); glVertex2f(x + h * 0.03f, baseY);
        glVertex2f(x + h * 0.03f, baseY + h * 0.2f); glVertex2f(x - h * 0.03f, baseY + h * 0.2f);
    glEnd();
    for (int k = 0; k < 3; k++)
    {
        float yb = baseY + h * (0.15f + 0.25f * k);
        float w  = h * (0.26f - 0.05f * k);
        tri(x - w, yb, x + w, yb, x, yb + h * 0.42f, 0.015f, 0.045f, 0.06f);
    }
}

void spectator(float x, float y, float s, float ph)
{
    float lean = sinf(T * 0.8f + ph) * 1.2f * s;
    glColor3f(0.008f, 0.012f, 0.03f);
    glBegin(GL_QUADS);
        glVertex2f(x - 6 * s, y); glVertex2f(x + 6 * s, y);
        glVertex2f(x + 5 * s, y + 17 * s); glVertex2f(x - 5 * s, y + 17 * s);
    glEnd();
    disc(x, y + 17 * s, 6.5f * s, 3.5f * s, 0.008f, 0.012f, 0.03f);
    disc(x + lean, y + 24 * s, 5.5f * s, 5.5f * s, 0.008f, 0.012f, 0.03f);
}

void drawScenery()
{
    // sky
    gradRect(0, WATER, 1000, 600 - WATER, 0.10f, 0.10f, 0.27f, 0.012f, 0.016f, 0.070f);

    // stars
    glEnable(GL_POINT_SMOOTH);
    glPointSize(2.2f);
    glBegin(GL_POINTS);
    for (int i = 0; i < 90; i++)
    {
        float sx = fmodf(i * 137.5f + 23.0f, 1000.0f);
        float sy = 270.0f + fmodf(i * 83.7f + i * i * 1.3f, 325.0f);
        float a = 0.35f + 0.5f * (0.5f + 0.5f * sinf(T * (1.5f + (i % 5) * 0.4f) + i));
        glColor4f(0.95f, 0.95f, 1.0f, a);
        glVertex2f(sx, sy);
    }
    glEnd();

    // crescent moon
    disc(120, 525, 34, 34, 1.0f, 0.93f, 0.68f);
    float sr, sg, sb;
    skyColor(535, sr, sg, sb);
    disc(137, 536, 30, 30, sr, sg, sb);
    glowDisc(120, 525, 110, 1.0f, 0.95f, 0.70f, 0.10f);   // halo drawn last so the cut-out stays invisible

    // mountains, trees, bank
    mountain(315, 40, 1.0f, 0.045f, 0.055f, 0.16f);
    mountain(285, 45, 4.2f, 0.025f, 0.035f, 0.10f);

    pine(40, 240, 110);  pine(92, 240, 70);  pine(235, 240, 85);
    pine(775, 240, 85);  pine(935, 240, 115); pine(980, 240, 65);

    rect(0, WATER, 1000, 24, 0.012f, 0.018f, 0.045f);

    spectator(62, 252, 1.2f, 0.0f);  spectator(95, 252, 1.3f, 1.3f);
    spectator(922, 252, 1.3f, 2.1f); spectator(956, 252, 1.2f, 3.4f);

    // river
    gradRect(0, 0, 1000, WATER, 0.008f, 0.015f, 0.055f, 0.035f, 0.070f, 0.185f);

    // moon reflection
    for (int i = 0; i < 14; i++)
    {
        float y = WATER - 8.0f - i * 14.0f;
        float w = 34.0f - i * 1.8f + 4.0f * sinf(T * 2.0f + i);
        float x = 120.0f + sinf(T * 1.5f + i * 0.8f) * 6.0f;
        rect(x - w * 0.5f, y, w, 3.0f, 1.0f, 0.93f, 0.68f, 0.30f - i * 0.018f);
    }

    // gentle water shimmer
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (int i = 0; i < 60; i++)
    {
        float ly = 6.0f + fmodf(i * 37.7f, WATER - 14.0f);
        float lx = fmodf(i * 211.3f + T * (6.0f + (i % 5) * 3.0f), 1100.0f) - 50.0f;
        float len = 20.0f + (i % 7) * 10.0f;
        glColor4f(0.5f, 0.6f, 0.95f, 0.05f + 0.04f * sinf(T * 2.0f + i));
        glVertex2f(lx, ly); glVertex2f(lx + len, ly);
    }
    glEnd();
}

// ----------------------------------------------------------------
// fireworks drawing (additive)
// ----------------------------------------------------------------
float partAlpha(const Particle& p)
{
    float f = clamp01(p.life / p.maxLife);
    float a = f * f * p.bright;
    if (p.twinkle && (std::rand() % 5 == 0)) a *= 0.35f;
    return a;
}

void drawReflections()
{
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    for (size_t i = 0; i < parts.size(); i++)
    {
        const Particle& p = parts[i];
        if (p.y <= WATER) continue;
        float ry = WATER - (p.y - WATER) * 0.55f;
        if (ry < 4.0f) continue;
        float wob = sinf(T * 7.0f + p.y * 0.08f + p.x * 0.02f) * 4.0f;
        float len = 4.0f + p.size * 1.8f + (p.y - WATER) * 0.02f;
        float a = partAlpha(p) * 0.30f * clamp01((ry) / 60.0f + 0.4f);
        glColor4f(p.r, p.g, p.b, a);
        glVertex2f(p.x + wob - len, ry);
        glVertex2f(p.x + wob + len, ry);
    }
    for (size_t i = 0; i < rockets.size(); i++)
    {
        const Rocket& r = rockets[i];
        float ry = WATER - (r.y - WATER) * 0.55f;
        glColor4f(1.0f, 0.8f, 0.5f, 0.3f);
        glVertex2f(r.x - 5, ry); glVertex2f(r.x + 5, ry);
    }
    glEnd();

    // expanding ripples under every explosion
    for (size_t i = 0; i < ripples.size(); i++)
    {
        const Ripple& rp = ripples[i];
        for (int k = 0; k < 2; k++)
        {
            float age = rp.age - k * 0.28f;
            if (age < 0.0f) continue;
            float u = age / 2.4f;
            float rx = 15.0f + age * 110.0f;
            float ry = rx * 0.16f;
            glLineWidth(1.5f);
            glColor4f(mixf(rp.r, 1.0f, 0.3f), mixf(rp.g, 1.0f, 0.3f), mixf(rp.b, 1.0f, 0.3f),
                      0.55f * (1.0f - u));
            glBegin(GL_LINE_LOOP);
            for (int s = 0; s < 48; s++)
            {
                float a = 2.0f * PI * s / 48.0f;
                glVertex2f(rp.x + cosf(a) * rx, 90.0f + sinf(a) * ry);
            }
            glEnd();
        }
    }
}

void drawSparks()
{
    // flashes: soft glow in the sky
    for (size_t i = 0; i < flashes.size(); i++)
    {
        const Flash& f = flashes[i];
        float u = f.age / 0.7f;
        glowDisc(f.x, f.y, 90.0f + f.age * 130.0f, f.r, f.g, f.b, 0.35f * (1 - u) * (1 - u));
    }

    glEnable(GL_POINT_SMOOTH);

    // soft halos
    glPointSize(12.0f);
    glBegin(GL_POINTS);
    for (size_t i = 0; i < parts.size(); i++)
    {
        const Particle& p = parts[i];
        if (p.size < 3.0f) continue;
        glColor4f(p.r, p.g, p.b, partAlpha(p) * 0.10f);
        glVertex2f(p.x, p.y);
    }
    glEnd();

    // bright cores: large then small
    for (int pass = 0; pass < 2; pass++)
    {
        glPointSize(pass == 0 ? 4.6f : 2.6f);
        glBegin(GL_POINTS);
        for (size_t i = 0; i < parts.size(); i++)
        {
            const Particle& p = parts[i];
            if ((p.size >= 3.0f) != (pass == 0)) continue;
            float f = clamp01(p.life / p.maxLife);
            float w = clamp01((f - 0.8f) * 4.0f);      // young particles are whiter
            glColor4f(mixf(p.r, 1.0f, w), mixf(p.g, 1.0f, w), mixf(p.b, 1.0f, w), partAlpha(p));
            glVertex2f(p.x, p.y);
        }
        glEnd();
    }

    // rocket heads
    glPointSize(5.5f);
    glBegin(GL_POINTS);
    for (size_t i = 0; i < rockets.size(); i++)
    {
        glColor4f(1.0f, 0.95f, 0.8f, 1.0f);
        glVertex2f(rockets[i].x, rockets[i].y);
    }
    glEnd();
}

void blossomCluster(float cx, float cy, float sc, float seed)
{
    for (int i = 0; i < 28; i++)
    {
        float a = i * 2.399f + seed;
        float d = sqrtf((i + 1) / 28.0f) * 60.0f * sc;
        float x = cx + cosf(a) * d * 1.25f;
        float y = cy + sinf(a) * d * 0.9f;
        float rad = (14.0f + (i * 7) % 12) * sc;
        float sh = ((i * 37) % 5) / 5.0f;
        disc(x, y, rad, rad, 0.80f, 0.40f + 0.12f * sh, 0.62f + 0.1f * sh, 0.95f);   // night-tinted pink
        disc(x - rad * 0.25f, y + rad * 0.25f, rad * 0.45f, rad * 0.45f, 1.0f, 0.70f, 0.85f, 0.30f);
    }
}

// One floating paper lantern with ripples and a shimmering reflection
void floatLantern(float x, float y, float s, int type, float ph)
{
    float cr, cg, cb;
    if (type == 0)      { cr = 1.00f; cg = 0.62f; cb = 0.18f; }
    else if (type == 1) { cr = 1.00f; cg = 0.45f; cb = 0.58f; }
    else                { cr = 1.00f; cg = 0.92f; cb = 0.62f; }

    y += sinf(T * 1.6f + ph) * 1.5f * s;
    float w = 24.0f * s, h = 28.0f * s, base = 5.0f * s;

    // ripples spreading around the lantern
    float u = fmodf(T * 0.5f + ph, 1.0f);
    float rr = w * 0.8f + u * w * 1.4f;
    glLineWidth(1.2f);
    glColor4f(cr, cg, cb, 0.35f * (1.0f - u));
    glBegin(GL_LINE_LOOP);
    for (int k = 0; k < 32; k++)
    {
        float a = 2.0f * PI * k / 32.0f;
        glVertex2f(x + cosf(a) * rr, y - 2.0f * s + sinf(a) * rr * 0.22f);
    }
    glEnd();

    // reflection: rippling streaks of light under the lantern
    for (int k = 0; k < 9; k++)
    {
        float ry = y - 5.0f * s - k * 6.0f * s;
        float rw = w * (0.95f - k * 0.08f);
        float wob = sinf(T * 3.0f + k * 1.1f + ph) * 3.0f * s;
        rect(x - rw * 0.5f + wob, ry, rw, 3.0f * s, cr, cg, cb, 0.36f - k * 0.038f);
    }

    // warm glow on the water around it
    glowDisc(x, y + h * 0.4f, 60.0f * s, 1.0f, 0.65f, 0.25f, 0.30f);

    // wooden base
    rect(x - w * 0.62f, y, w * 1.24f, base, 0.25f, 0.15f, 0.10f);

    // glowing paper body (bright in the middle, coloured at the edges)
    glBegin(GL_QUADS);
        glColor3f(cr, cg * 0.85f, cb * 0.7f);
        glVertex2f(x - w * 0.5f, y + base); glVertex2f(x + w * 0.5f, y + base);
        glColor3f(1.0f, 0.97f, 0.78f);
        glVertex2f(x + w * 0.42f, y + base + h); glVertex2f(x - w * 0.42f, y + base + h);
    glEnd();
    glowDisc(x, y + base + h * 0.45f, 17.0f * s, 1.0f, 0.97f, 0.65f, 0.65f);   // flame inside

    // wooden frame
    glLineWidth(1.6f);
    glColor4f(0.28f, 0.14f, 0.08f, 0.9f);
    glBegin(GL_LINES);
        glVertex2f(x - w * 0.5f, y + base);  glVertex2f(x - w * 0.42f, y + base + h);
        glVertex2f(x + w * 0.5f, y + base);  glVertex2f(x + w * 0.42f, y + base + h);
        glVertex2f(x, y + base);             glVertex2f(x, y + base + h);
        glVertex2f(x - w * 0.46f, y + base + h * 0.5f); glVertex2f(x + w * 0.46f, y + base + h * 0.5f);
    glEnd();

    // little roof
    tri(x - w * 0.62f, y + base + h, x + w * 0.62f, y + base + h, x, y + base + h + 9.0f * s, 0.22f, 0.13f, 0.10f);
    rect(x - w * 0.56f, y + base + h, w * 1.12f, 2.5f * s, 0.32f, 0.18f, 0.12f);
}

void drawFloatingLanterns()
{
    const float ly[12] = { 205, 195, 182, 168, 154, 140, 126, 110, 94, 78, 60, 42 };
    for (int i = 0; i < 12; i++)
    {
        float y = ly[i];
        float s = mixf(0.6f, 1.35f, 1.0f - y / WATER);           // nearer = bigger
        float x = fmodf(fmodf(i * 173.0f + 37.0f, 1000.0f) + T * (4.0f + (i % 4) * 2.5f) * s, 1100.0f) - 50.0f;
        floatLantern(x, y, s, i % 3, i * 1.7f);
    }
}

void drawFrame()
{
    drawFloatingLanterns();      // toro nagashi: glowing lanterns floating on the river

    // cherry-blossom branches in the top corners (same look as the Festival Street)
    glLineWidth(6.0f);
    glColor3f(0.12f, 0.07f, 0.09f);
    glBegin(GL_LINES);
        glVertex2f(0, 470);    glVertex2f(95, 535);
        glVertex2f(1000, 470); glVertex2f(905, 535);
    glEnd();
    for (int side = 0; side < 2; side++)
    {
        float m = side == 0 ? 1.0f : -1.0f;
        float o = side == 0 ? 0.0f : 1000.0f;
        blossomCluster(o + m * 40,  555, 1.15f, side * 2.0f);
        blossomCluster(o + m * 115, 520, 0.90f, 1.0f + side);
        blossomCluster(o + m * 25,  455, 0.95f, 2.0f + side);
    }

    // drifting petals
    for (int i = 0; i < 24; i++)
    {
        float px = fmodf(i * 131.0f + T * (10.0f + (i % 5) * 4.0f) + sinf(T + i) * 30.0f, 1000.0f);
        float py = 600.0f - fmodf(i * 53.0f + T * (22.0f + (i % 4) * 6.0f), 600.0f);
        disc(px, py, 3.5f, 2.0f, 1.0f, 0.70f, 0.84f, 0.7f);
    }
}
} // namespace

// =====================================================================
//  public API
// =====================================================================
void resetFireworks()
{
    parts.clear(); rockets.clear(); flashes.clear(); ripples.clear(); pending.clear();
    launchTimer = 0.4f; patternCounter = 0; lastLaunchX = 500.0f;
    finaleT = -1.0f; barrageDone = false;
}

void drawFireworks(bool finale)
{
    float now = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
    float dt = now - lastT;
    if (dt > 4.0f) { resetFireworks(); dt = 0.016f; }     // scene re-entered
    if (dt > 0.05f) dt = 0.05f;
    if (dt < 0.0f)  dt = 0.0f;
    lastT = now; T = now;

    if (finale) finaleT = (finaleT < 0.0f) ? 0.0f : finaleT + dt;
    else        { finaleT = -1.0f; barrageDone = false; }

    update(dt, finale);

    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_LINE_BIT | GL_POINT_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);

    drawScenery();
    drawFrame();

    glBlendFunc(GL_SRC_ALPHA, GL_ONE);         // additive light for sparks and reflections
    drawReflections();
    drawSparks();

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (finaleT >= 0.0f)
    {
        float a = clamp01(finaleT / 2.5f);
        rect(230, 102, 540, 62, 0.0f, 0.01f, 0.06f, 0.55f * a);
        textC(502, 148, "THANK YOU FOR VISITING HANABI", GLUT_BITMAP_TIMES_ROMAN_24, 0.0f, 0.0f, 0.05f, a * 0.8f);
        textC(500, 150, "THANK YOU FOR VISITING HANABI", GLUT_BITMAP_TIMES_ROMAN_24, 1.0f, 0.92f, 0.65f, a);
        textC(500, 118, "See you next summer!", GLUT_BITMAP_HELVETICA_18, 0.85f, 0.88f, 1.0f, clamp01((finaleT - 1.2f) / 2.0f));
    }

    glLineWidth(1.0f);
    glPopAttrib();
}