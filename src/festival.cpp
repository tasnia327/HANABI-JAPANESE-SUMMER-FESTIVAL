// festival.cpp  -  HANABI : Festival Street (anime-style, one-point perspective)
//
// Camera looks down a festival street.  Stalls stand on the LEFT and RIGHT,
// visitors in yukata walk in the MIDDLE, lantern strings hang across the street,
// a pagoda closes the far end.  Day / sunset / night controlled by festivalSetNight().
//
// World space: X = left/right, Y = up, Z = distance from the camera.
// Perspective projection (done by hand):   sx = VPX + F*X/Z ,  sy = VPY + F*(Y-CAMH)/Z
#include "festival.h"
#include <GL/glut.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace {

const float W   = FESTIVAL_W;
const float H   = FESTIVAL_H;
const float PI  = 3.14159265f;
const float VPX = 640.0f;      // vanishing point (screen)
const float VPY = 385.0f;
const float FOC = 600.0f;      // "focal length" in pixels
const float CAMH = 1.6f;       // camera height in metres

inline float PX(float X, float Z) { return VPX + FOC * X / Z; }
inline float PY(float Y, float Z) { return VPY + FOC * (Y - CAMH) / Z; }
inline float KZ(float Z)          { return FOC / Z; }          // pixels per metre at depth Z

// ------------------------------------------------------------------ colours
struct Col { float r, g, b; };
inline Col   C(float r, float g, float b) { Col c = {r, g, b}; return c; }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline Col   mix(Col a, Col b, float t) { return C(lerpf(a.r, b.r, t), lerpf(a.g, b.g, t), lerpf(a.b, b.b, t)); }
inline Col   scaleCol(Col c, float k) { return C(c.r * k, c.g * k, c.b * k); }
inline void  setCol(Col c, float a = 1.0f) { glColor4f(c.r, c.g, c.b, a); }

float g_night = 0.0f;                 // 0 day .. 1 night
float g_lamp  = 0.0f;                 // lanterns / windows light up a little after the sky turns dark
bool  g_showBtn = false;              // sun/moon button visible?
int   g_btnSunTarget = 0;             // 1 = the button shows a SUN (night mode: click -> day), 0 = MOON (day mode: click -> night)
float g_btnMix = 0.0f;                // eased icon blend: 0 = moon, 1 = sun
float g_btnPress = 0.0f;              // click 'squish' animation
float g_mx = -1.0f, g_my = -1.0f;     // mouse position in scene coordinates (hover)
float g_time  = 0.0f;
Col   g_haze  = {0.8f, 0.9f, 1.0f};   // distance haze colour (= sky colour at horizon)
const Col INK = {0.13f, 0.07f, 0.09f};// anime outline colour
float g_lw = 1.5f;                    // current outline width

float rand01() { return rand() / (float)RAND_MAX; }
Col dn(Col day, Col night) { return mix(day, night, g_night); }
Col tint(Col c) { return mix(c, scaleCol(c, 0.66f), g_night); }
Col fogc(Col c, float Z) {            // atmospheric perspective
    float t = clampf((Z - 6.0f) / 60.0f, 0.0f, 1.0f) * 0.75f;
    return mix(c, g_haze, t);
}

// ------------------------------------------------------------------ 2D primitives
void rect(float x, float y, float w, float h) {
    glBegin(GL_QUADS); glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h); glEnd();
}
void tri(float x1, float y1, float x2, float y2, float x3, float y3) {
    glBegin(GL_TRIANGLES); glVertex2f(x1, y1); glVertex2f(x2, y2); glVertex2f(x3, y3); glEnd();
}
void ellipse(float cx, float cy, float rx, float ry, int seg = 28) {
    glBegin(GL_TRIANGLE_FAN); glVertex2f(cx, cy);
    for (int i = 0; i <= seg; i++) { float a = 2.0f * PI * i / seg; glVertex2f(cx + rx * cosf(a), cy + ry * sinf(a)); }
    glEnd();
}
void circle(float cx, float cy, float r, int seg = 28) { ellipse(cx, cy, r, r, seg); }
void arcFill(float cx, float cy, float r, float a0, float a1, int seg = 16) {
    glBegin(GL_TRIANGLE_FAN); glVertex2f(cx, cy);
    for (int i = 0; i <= seg; i++) { float a = (a0 + (a1 - a0) * i / seg) * PI / 180.0f; glVertex2f(cx + r * cosf(a), cy + r * sinf(a)); }
    glEnd();
}
void line(float x1, float y1, float x2, float y2, float w = 1.0f) {
    glLineWidth(w); glBegin(GL_LINES); glVertex2f(x1, y1); glVertex2f(x2, y2); glEnd();
}
// filled convex polygon + dark anime outline
void fillPoly(const float* p, int n, Col fill, float a = 1.0f) {
    setCol(fill, a);
    glBegin(GL_TRIANGLE_FAN); for (int i = 0; i < n; i++) glVertex2f(p[2 * i], p[2 * i + 1]); glEnd();
    setCol(INK, 0.9f * a); glLineWidth(g_lw);
    glBegin(GL_LINE_LOOP); for (int i = 0; i < n; i++) glVertex2f(p[2 * i], p[2 * i + 1]); glEnd();
}
void ellipseOut(float cx, float cy, float rx, float ry, Col fill, float a = 1.0f) {
    setCol(fill, a); ellipse(cx, cy, rx, ry, 24);
    setCol(INK, 0.9f * a); glLineWidth(g_lw);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 24; i++) { float t = 2.0f * PI * i / 24; glVertex2f(cx + rx * cosf(t), cy + ry * sinf(t)); }
    glEnd();
}
// additive soft glow
void glowEllipse(float cx, float cy, float rx, float ry, Col c, float strength) {
    if (strength <= 0.01f) return;
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    for (int i = 0; i < 5; i++) { float k = 1.0f - i / 5.0f; setCol(c, strength * 0.08f); ellipse(cx, cy, rx * k, ry * k, 24); }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
void glowCircle(float cx, float cy, float r, Col c, float s) { glowEllipse(cx, cy, r, r, c, s); }

// ------------------------------------------------------------------ 3D helpers (perspective)
struct V3 { float x, y, z; };
V3 v3(float x, float y, float z) { V3 v = {x, y, z}; return v; }
void vtx(V3 p) { glVertex2f(PX(p.x, p.z), PY(p.y, p.z)); }
void poly4(V3 a, V3 b, V3 c, V3 d) { glBegin(GL_QUADS); vtx(a); vtx(b); vtx(c); vtx(d); glEnd(); }
void poly4g(V3 a, V3 b, V3 c, V3 d, Col c0, Col c1) {    // a,b bottom edge (c0), c,d top edge (c1)
    glBegin(GL_QUADS); setCol(c0); vtx(a); vtx(b); setCol(c1); vtx(c); vtx(d); glEnd();
}
void edge3(V3 a, V3 b, float w) { glLineWidth(w); glBegin(GL_LINES); vtx(a); vtx(b); glEnd(); }

// ------------------------------------------------------------------ data
struct Person { float x, z, speed, phase; int dir, style, id; Col kimono, obi, hair;
                bool woman, child, fan, lantern, idle, vendor; int food, face; };
struct Stall  { int side; float z0, z1; int type; Col roof; const char* name; bool smoke; int var; };
struct House  { int side; float z0, z1, h; Col wall; };
struct Tree   { float x, z, r; int seed; };
struct Smoke  { float x, y, z, vx, vy, age, life, size; int stall; bool alive; };
struct Petal  { float x, y, vy, vx, rot, spin, size, phase; };
struct Star   { float x, y, size, phase; };
struct Cloud  { float x, y, s, speed; };

const int MAXP = 56, MAXS = 64, MAXH = 80, MAXT = 16, MAXSM = 140, NPET = 45, NSTAR = 90, NCLOUD = 4;
Person g_people[MAXP]; int g_nPeople = 0;
Stall  g_stalls[MAXS]; int g_nStalls = 0;
House  g_houses[MAXH]; int g_nHouses = 0;
Tree   g_trees[MAXT];  int g_nTrees = 0;
Smoke  g_smoke[MAXSM];
Petal  g_petals[NPET];
Star   g_stars[NSTAR];
Cloud  g_clouds[NCLOUD];
float  g_emit[MAXS];

const int NSTR = 8;
const float STR_Z[NSTR] = {3.6f, 5.4f, 7.6f, 10.4f, 14.0f, 19.0f, 26.0f, 36.0f};

// ================================================================ SKY
void drawSky() {
    Col dTop = C(0.28f, 0.60f, 0.96f), dBot = C(0.82f, 0.93f, 1.00f);
    Col sTop = C(0.30f, 0.25f, 0.58f), sBot = C(1.00f, 0.60f, 0.35f);
    Col nTop = C(0.02f, 0.03f, 0.14f), nBot = C(0.17f, 0.12f, 0.36f);
    Col top, bot;
    if (g_night < 0.5f) { float t = g_night * 2; top = mix(dTop, sTop, t); bot = mix(dBot, sBot, t); }
    else { float t = (g_night - 0.5f) * 2; top = mix(sTop, nTop, t); bot = mix(sBot, nBot, t); }
    g_haze = bot;
    glBegin(GL_QUADS);
    setCol(bot); glVertex2f(0, 300); glVertex2f(W, 300);
    setCol(top); glVertex2f(W, H);   glVertex2f(0, H);
    glEnd();
}
void drawSunMoon() {
    float sunY = 640.0f - g_night * 270.0f;
    Col sc = mix(C(1.0f, 0.92f, 0.45f), C(1.0f, 0.45f, 0.15f), g_night);
    glowCircle(710, sunY, 120, sc, 1.0f - g_night * 0.4f);
    setCol(sc); circle(710, sunY, 38);
    if (g_night > 0.02f) {
        float my = 430.0f + g_night * 230.0f;
        glowCircle(560, my, 110, C(0.75f, 0.82f, 1.0f), g_night);
        setCol(C(0.98f, 0.96f, 0.82f), g_night); circle(560, my, 30);
        setCol(C(0.85f, 0.82f, 0.66f), g_night * 0.7f); circle(550, my + 8, 6); circle(571, my - 9, 5);
    }
}
void drawStars() {
    if (g_night < 0.3f) return;
    float a0 = (g_night - 0.3f) / 0.7f;
    for (int i = 0; i < NSTAR; i++) {
        float tw = 0.5f + 0.5f * sinf(g_time * 2.0f + g_stars[i].phase);
        setCol(C(1, 1, 0.92f), a0 * tw); float s = g_stars[i].size; rect(g_stars[i].x, g_stars[i].y, s, s);
    }
}
void drawClouds() {
    Col c = dn(C(1, 1, 1), C(0.4f, 0.33f, 0.55f));
    float a = 0.9f * (1.0f - 0.7f * g_night);
    for (int i = 0; i < NCLOUD; i++) {
        float x = g_clouds[i].x, y = g_clouds[i].y, s = g_clouds[i].s;
        setCol(c, a);
        circle(x, y, 24 * s); circle(x + 26 * s, y + 8 * s, 30 * s); circle(x + 56 * s, y, 24 * s); circle(x + 28 * s, y - 8 * s, 26 * s);
    }
}
void drawHills() {
    static const float xs[] = {0, 150, 330, 470, 560, 640, 730, 830, 980, 1130, 1280};
    static const float ys[] = {430, 470, 450, 500, 468, 520, 470, 495, 455, 468, 440};
    Col c = mix(dn(C(0.45f, 0.62f, 0.60f), C(0.07f, 0.09f, 0.22f)), g_haze, 0.3f);
    setCol(c);
    for (int i = 0; i < 10; i++) {
        glBegin(GL_QUADS); glVertex2f(xs[i], ys[i]); glVertex2f(xs[i + 1], ys[i + 1]); glVertex2f(xs[i + 1], 300); glVertex2f(xs[i], 300); glEnd();
    }
}

// ================================================================ GROUND
void drawGround() {
    Col sideC = dn(C(0.46f, 0.56f, 0.36f), C(0.07f, 0.11f, 0.11f));
    glBegin(GL_QUADS);
    setCol(sideC); glVertex2f(0, 0); glVertex2f(W, 0);
    setCol(mix(sideC, g_haze, 0.8f)); glVertex2f(W, VPY + 2); glVertex2f(0, VPY + 2);
    glEnd();
    Col st = dn(C(0.80f, 0.68f, 0.52f), C(0.30f, 0.23f, 0.25f));
    poly4g(v3(-3.2f, 0, 2.0f), v3(3.2f, 0, 2.0f), v3(3.2f, 0, 90.0f), v3(-3.2f, 0, 90.0f), st, mix(st, g_haze, 0.85f));
    // cobble pattern
    Col ln = dn(C(0.52f, 0.42f, 0.30f), C(0.10f, 0.07f, 0.10f));
    float z = 2.0f; int row = 0;
    while (z < 60.0f) {
        float dz = z < 8 ? 0.45f : (z < 20 ? 0.8f : 1.6f);
        float a = 0.7f * (1.0f - z / 60.0f);
        setCol(ln, a);
        edge3(v3(-3.2f, 0, z), v3(3.2f, 0, z), 1.2f);
        float off = (row % 2) * 0.4f;
        for (float x = -3.2f + off; x < 3.2f; x += 0.8f) edge3(v3(x, 0, z), v3(x, 0, z + dz), 1.0f);
        z += dz; row++;
    }
    // light pools from the lantern strings
    if (g_lamp > 0.02f) {
        for (int i = 0; i < NSTR; i++) {
            float Z = STR_Z[i];
            glowEllipse(PX(0, Z), PY(0, Z), 3.4f * KZ(Z), FOC * CAMH * 1.6f / (Z * Z), C(1.0f, 0.62f, 0.25f), g_lamp * 0.55f);
        }
    }
}

// ================================================================ HOUSES (street walls)
void drawHouse(const House& hs) {
    float xs = hs.side * 5.4f, xe = hs.side * 4.5f, z0 = hs.z0, z1 = hs.z1, h = hs.h, zm = 0.5f * (z0 + z1);
    Col day = hs.wall, night = C(hs.wall.r * 0.22f, hs.wall.g * 0.22f, hs.wall.b * 0.38f);
    Col wall = fogc(dn(day, night), zm);
    poly4g(v3(xs, 0, z0), v3(xs, 0, z1), v3(xs, h, z1), v3(xs, h, z0), scaleCol(wall, 0.75f), wall);
    // timber posts
    setCol(fogc(dn(C(0.28f, 0.18f, 0.12f), C(0.08f, 0.05f, 0.06f)), zm));
    for (float z = z0; z <= z1 + 0.01f; z += 1.2f) edge3(v3(xs, 0, z), v3(xs, h, z), 1.5f);
    edge3(v3(xs, 3.0f, z0), v3(xs, 3.0f, z1), 2.0f);
    // windows (shoji) - glow at night
    Col win = fogc(mix(C(0.86f, 0.82f, 0.66f), C(1.0f, 0.74f, 0.30f), g_lamp), zm);
    for (int row = 0; row < 2; row++) {
        float y0 = row == 0 ? 3.4f : 5.5f, y1 = y0 + 1.3f;
        if (y1 + 0.6f > h) break;
        for (float wz = z0 + 0.4f; wz + 0.9f < z1; wz += 1.4f) {
            setCol(win); poly4(v3(xs, y0, wz), v3(xs, y0, wz + 0.9f), v3(xs, y1, wz + 0.9f), v3(xs, y1, wz));
            setCol(fogc(dn(C(0.30f, 0.20f, 0.12f), C(0.10f, 0.06f, 0.05f)), zm));
            edge3(v3(xs, y0, wz), v3(xs, y1, wz), 1.2f); edge3(v3(xs, y0, wz + 0.9f), v3(xs, y1, wz + 0.9f), 1.2f);
            edge3(v3(xs, 0.5f * (y0 + y1), wz), v3(xs, 0.5f * (y0 + y1), wz + 0.9f), 1.0f);
            edge3(v3(xs, y0, wz + 0.45f), v3(xs, y1, wz + 0.45f), 1.0f);
            float zc = wz + 0.45f;
            glowEllipse(PX(xs, zc), PY(0.5f * (y0 + y1), zc), 0.9f * KZ(zc), 0.9f * KZ(zc), C(1.0f, 0.68f, 0.25f), g_lamp * 0.55f);
        }
    }
    // tiled eaves
    Col eave = fogc(dn(C(0.18f, 0.20f, 0.28f), C(0.05f, 0.05f, 0.10f)), zm);
    setCol(scaleCol(eave, 0.7f)); poly4(v3(xs, h, z0), v3(xs, h, z1), v3(xe, h, z1), v3(xe, h, z0));
    setCol(eave);                 poly4(v3(xe, h, z0), v3(xe, h, z1), v3(xe, h + 0.5f, z1), v3(xe, h + 0.5f, z0));
    setCol(fogc(dn(C(0.45f, 0.48f, 0.58f), C(0.14f, 0.15f, 0.25f)), zm), 0.8f);
    edge3(v3(xe, h + 0.5f, z0), v3(xe, h + 0.5f, z1), 2.0f);
}

// ================================================================ PAGODA & TREES
void drawPagoda(float Z) {
    float k = KZ(Z);
    glPushMatrix(); glTranslatef(PX(0, Z), PY(0, Z), 0); glScalef(k, k, 1);
    g_lw = 1.0f;
    Col body = fogc(dn(C(0.80f, 0.22f, 0.15f), C(0.32f, 0.08f, 0.09f)), Z);
    Col roof = fogc(dn(C(0.22f, 0.26f, 0.38f), C(0.06f, 0.07f, 0.14f)), Z);
    Col trim = fogc(dn(C(0.95f, 0.80f, 0.35f), C(0.45f, 0.35f, 0.15f)), Z);
    setCol(fogc(dn(C(0.55f, 0.50f, 0.45f), C(0.15f, 0.14f, 0.18f)), Z)); rect(-3.4f, 0, 6.8f, 0.5f);
    float y = 0.5f;
    for (int i = 0; i < 5; i++) {
        float w = 5.6f - i * 0.8f, bh = 1.1f;
        setCol(body); rect(-w / 2 + 0.4f, y, w - 0.8f, bh);
        setCol(mix(C(0.95f, 0.9f, 0.7f), C(1.0f, 0.75f, 0.3f), g_lamp)); rect(-0.2f, y + 0.3f, 0.4f, 0.5f);
        glowCircle(0, y + 0.55f, 0.9f, C(1.0f, 0.7f, 0.25f), g_lamp * 0.8f);
        setCol(roof);
        glBegin(GL_QUADS); glVertex2f(-w / 2 - 0.9f, y + bh); glVertex2f(w / 2 + 0.9f, y + bh); glVertex2f(w / 2 - 0.1f, y + bh + 0.7f); glVertex2f(-w / 2 + 0.1f, y + bh + 0.7f); glEnd();
        tri(-w / 2 - 0.9f, y + bh, -w / 2 - 1.3f, y + bh + 0.4f, -w / 2 - 0.4f, y + bh + 0.1f);
        tri(w / 2 + 0.9f, y + bh, w / 2 + 1.3f, y + bh + 0.4f, w / 2 + 0.4f, y + bh + 0.1f);
        setCol(trim); rect(-w / 2 - 0.9f, y + bh, w + 1.8f, 0.1f);
        y += bh + 0.7f;
    }
    setCol(trim); rect(-0.08f, y, 0.16f, 2.2f);
    for (int i = 0; i < 4; i++) rect(-0.25f + i * 0.04f, y + 0.4f + i * 0.4f, 0.5f - i * 0.08f, 0.08f);
    glPopMatrix();
}

void blossoms(float cx, float cy, float R, int seed, Col a, Col b, Col dotc, float alpha) {
    for (int i = 0; i < 16; i++) {
        float ang = i * 2.399f + seed * 1.7f, rr = R * 0.8f * sqrtf((float)(i + 1) / 16);
        float px = cx + cosf(ang) * rr, py = cy + sinf(ang) * rr * 0.75f;
        float rad = R * (0.26f + 0.08f * sinf(i * 1.9f + seed));
        setCol(mix(a, b, 0.5f + 0.5f * sinf(i * 1.3f + seed)), alpha); circle(px, py, rad, 22);
    }
    for (int i = 0; i < 36; i++) {
        float ang = i * 2.1f + seed * 0.9f, rr = R * (0.1f + 0.9f * fabsf(sinf(i * 1.37f + seed)));
        setCol(dotc, alpha * 0.85f); circle(cx + cosf(ang) * rr, cy + sinf(ang) * rr * 0.75f, R * 0.045f, 8);
    }
}
void pinks(Col& a, Col& b, Col& d, float Z) {
    a = fogc(dn(C(1.00f, 0.70f, 0.80f), C(0.78f, 0.45f, 0.58f)), Z);
    b = fogc(dn(C(0.95f, 0.58f, 0.72f), C(0.62f, 0.34f, 0.50f)), Z);
    d = fogc(dn(C(1.00f, 0.90f, 0.94f), C(0.90f, 0.66f, 0.72f)), Z);
}
void drawTree(const Tree& t) {
    float k = KZ(t.z);
    glPushMatrix(); glTranslatef(PX(t.x, t.z), PY(0, t.z), 0); glScalef(k, k, 1);
    setCol(fogc(dn(C(0.36f, 0.24f, 0.17f), C(0.12f, 0.08f, 0.08f)), t.z)); rect(-0.22f, 0, 0.44f, 3.6f);
    glLineWidth(fmaxf(1.0f, 0.12f * k));
    line(0, 3.0f, -1.0f, 4.2f, fmaxf(1.0f, 0.12f * k)); line(0, 3.2f, 1.1f, 4.3f, fmaxf(1.0f, 0.12f * k));
    Col a, b, d; pinks(a, b, d, t.z);
    blossoms(0, 4.5f, t.r, t.seed, a, b, d, 0.96f);
    glPopMatrix();
}

// ================================================================ LANTERNS
float lampFor(int id) {   // lanterns switch on one after another while the sky gets dark
    float t = clampf((g_night - 0.30f) / 0.45f - (id % 7) * 0.07f, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
void drawLantern(float cx, float cy, float w, float h, Col base, int id, float lw) {
    float lamp = lampFor(id);
    float flick = 0.85f + 0.15f * sinf(g_time * 7.0f + id * 2.3f);
    Col body = mix(scaleCol(base, 0.88f), mix(base, C(1.0f, 0.85f, 0.5f), 0.45f), lamp);
    glowCircle(cx, cy, w * 1.8f, mix(base, C(1.0f, 0.70f, 0.30f), 0.5f), lamp * flick * 0.95f);
    setCol(body); ellipse(cx, cy, w / 2, h / 2, 24);
    setCol(C(1.0f, 0.92f, 0.6f), lamp * 0.6f * flick); ellipse(cx, cy, w * 0.34f, h * 0.38f, 20);
    setCol(scaleCol(base, 0.55f), 0.8f);
    for (int k = -2; k <= 2; k++) {
        float dy = k * h * 0.17f, hw = (w / 2) * sqrtf(1.0f - (dy / (h / 2)) * (dy / (h / 2)));
        line(cx - hw, cy + dy, cx + hw, cy + dy, lw);
    }
    Col dark = C(0.10f, 0.08f, 0.06f);
    setCol(dark); rect(cx - w * 0.30f, cy + h / 2 - h * 0.02f, w * 0.6f, h * 0.07f); rect(cx - w * 0.30f, cy - h / 2 - h * 0.05f, w * 0.6f, h * 0.07f);
    bool red = (base.r > 0.8f && base.g < 0.3f);
    setCol(red ? C(1.0f, 0.93f, 0.75f) : dark, 0.85f);
    line(cx, cy + h * 0.22f, cx, cy - h * 0.22f, lw * 1.6f);
    line(cx - w * 0.15f, cy + h * 0.08f, cx + w * 0.15f, cy + h * 0.08f, lw * 1.3f);
    line(cx - w * 0.12f, cy - h * 0.10f, cx + w * 0.12f, cy - h * 0.10f, lw * 1.3f);
    setCol(INK, 0.8f); glLineWidth(lw);
    glBegin(GL_LINE_LOOP); for (int i = 0; i < 24; i++) { float t = 2 * PI * i / 24; glVertex2f(cx + w / 2 * cosf(t), cy + h / 2 * sinf(t)); } glEnd();
}
void lanternW(float X, float Y, float Z, float wM, float hM, Col base, int id) {
    float k = KZ(Z);
    drawLantern(PX(X, Z), PY(Y, Z), wM * k, hM * k, fogc(base, Z), id, clampf(wM * k * 0.03f, 0.8f, 2.2f));
}
void drawLanternString(int i) {
    float Z = STR_Z[i];
    setCol(dn(C(0.15f, 0.12f, 0.10f), C(0.04f, 0.03f, 0.03f)));
    glLineWidth(clampf(0.012f * KZ(Z), 1.0f, 3.0f));
    glBegin(GL_LINE_STRIP);
    for (int s = 0; s <= 40; s++) { float X = -3.5f + 7.0f * s / 40.0f; float t = X / 3.5f; glVertex2f(PX(X, Z), PY(3.55f - 0.55f * (1 - t * t), Z)); }
    glEnd();
    static const Col pal[3] = { {0.88f, 0.14f, 0.10f}, {0.99f, 0.92f, 0.62f}, {0.98f, 0.55f, 0.12f} };
    for (int j = 0; j < 9; j++) {
        float X = -3.2f + 0.8f * j, t = X / 3.5f;
        float ys = 3.55f - 0.55f * (1 - t * t);
        float sway = sinf(g_time * 1.3f + j * 1.7f + i) * 0.03f;
        lanternW(X + sway, ys - 0.28f, Z, 0.34f, 0.46f, pal[(i + j) % 3], i * 9 + j);
    }
}

// ================================================================ BANNER TEXT (stroke font on a slanted plane)
void bannerText(const char* s, float xo, float zL, float zR, float yBot, float yTop, float wFrac, float vLo, float vHi, Col c, float lw) {
    float ox = PX(xo, zL), oy = PY(yBot, zL);
    float ux = PX(xo, zR) - ox, uy = PY(yBot, zR) - oy;
    float vy = PY(yTop, zL) - oy;
    float len = (float)glutStrokeLength(GLUT_STROKE_ROMAN, (const unsigned char*)s);
    if (len < 1.0f) return;
    float m[16] = { ux, uy, 0, 0,   0, vy, 0, 0,   0, 0, 1, 0,   ox, oy, 0, 1 };
    for (int pass = 0; pass < 2; pass++) {
        glPushMatrix(); glMultMatrixf(m);
        glTranslatef((1.0f - wFrac) * 0.5f, vLo, 0);
        glScalef(wFrac / len, (vHi - vLo) / 119.05f, 1);
        if (pass == 0) { setCol(INK, 0.9f); glLineWidth(lw + 2.5f); } else { setCol(c); glLineWidth(lw); }
        for (const char* p = s; *p; ++p) glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);
        glPopMatrix();
    }
}

// ================================================================ FOOD (drawn in metres, origin = counter top)
void drawFood(int type, float sx, float sy, float k, float Z) {
    glPushMatrix(); glTranslatef(sx, sy, 0); glScalef(k, k, 1);
    g_lw = clampf(k * 0.012f, 0.7f, 1.6f);
    Col dark = fogc(C(0.22f, 0.22f, 0.25f), Z);
    if (type == 0) {                                          // TAKOYAKI
        setCol(dark); rect(-0.28f, 0, 0.56f, 0.04f);
        for (int i = 0; i < 5; i++) ellipseOut(-0.18f + 0.09f * i, 0.13f, 0.045f, 0.045f, fogc(tint(C(0.72f, 0.45f, 0.16f)), Z));
        for (int i = 0; i < 6; i++) {
            float bx = -0.225f + 0.09f * i;
            ellipseOut(bx, 0.07f, 0.05f, 0.05f, fogc(tint(C(0.84f, 0.54f, 0.18f)), Z));
            setCol(fogc(tint(C(0.35f, 0.15f, 0.08f)), Z)); circle(bx - 0.005f, 0.085f, 0.028f, 10);
            setCol(fogc(C(0.3f, 0.7f, 0.2f), Z)); circle(bx + 0.015f, 0.09f, 0.008f, 6);
        }
    } else if (type == 1) {                                   // YAKISOBA
        setCol(dark); rect(-0.32f, 0, 0.64f, 0.035f);
        ellipseOut(0, 0.08f, 0.27f, 0.075f, fogc(tint(C(0.90f, 0.64f, 0.22f)), Z));
        setCol(fogc(tint(C(0.60f, 0.34f, 0.10f)), Z));
        for (int kk = 0; kk < 5; kk++) {
            glLineWidth(1.5f); glBegin(GL_LINE_STRIP);
            for (int i = 0; i <= 20; i++) glVertex2f(-0.22f + 0.022f * i, 0.075f + (kk - 2) * 0.018f + sinf(i * 0.9f + kk + g_time * 2) * 0.012f);
            glEnd();
        }
        for (int i = 0; i < 8; i++) {
            setCol(fogc(tint(C(0.35f, 0.7f, 0.25f)), Z)); circle(-0.2f + i * 0.057f, 0.1f + (i % 3) * 0.012f, 0.014f, 6);
            setCol(fogc(tint(C(0.85f, 0.15f, 0.15f)), Z)); circle(-0.18f + i * 0.057f, 0.07f + (i % 2) * 0.016f, 0.01f, 6);
        }
    } else if (type == 2) {                                   // OKONOMIYAKI
        setCol(dark); rect(-0.32f, 0, 0.64f, 0.035f);
        for (int d = 0; d < 2; d++) {
            float ox = -0.15f + 0.30f * d;
            ellipseOut(ox, 0.06f, 0.15f, 0.05f, fogc(tint(C(0.72f, 0.46f, 0.20f)), Z));
            setCol(fogc(tint(C(0.42f, 0.18f, 0.10f)), Z)); ellipse(ox, 0.075f, 0.135f, 0.038f, 20);
            setCol(fogc(tint(C(1.0f, 0.95f, 0.8f)), Z)); glLineWidth(1.3f);
            glBegin(GL_LINE_STRIP); for (int i = 0; i <= 10; i++) glVertex2f(ox - 0.12f + i * 0.024f, 0.075f + ((i % 2) ? 0.02f : -0.012f)); glEnd();
            setCol(fogc(tint(C(0.3f, 0.7f, 0.2f)), Z)); for (int i = 0; i < 6; i++) circle(ox - 0.09f + i * 0.036f, 0.085f + (i % 2) * 0.01f, 0.008f, 6);
        }
    } else {                                                  // KAKIGORI
        setCol(fogc(dn(C(0.70f, 0.74f, 0.78f), C(0.45f, 0.48f, 0.55f)), Z)); rect(-0.30f, 0, 0.17f, 0.30f);
        setCol(dark); circle(-0.215f, 0.23f, 0.035f, 10); line(-0.215f, 0.23f, -0.15f, 0.28f, 2.0f);
        static const Col syrup[3] = { {0.95f, 0.15f, 0.25f}, {0.20f, 0.55f, 0.95f}, {0.45f, 0.85f, 0.35f} };
        for (int i = 0; i < 3; i++) {
            float cx = -0.02f + 0.14f * i;
            setCol(fogc(C(1, 1, 1), Z), 0.85f); glBegin(GL_QUADS); glVertex2f(cx - 0.045f, 0); glVertex2f(cx + 0.045f, 0); glVertex2f(cx + 0.06f, 0.13f); glVertex2f(cx - 0.06f, 0.13f); glEnd();
            ellipseOut(cx, 0.16f, 0.07f, 0.07f, fogc(tint(syrup[i]), Z));
            setCol(fogc(C(1, 1, 1), Z), 0.7f); circle(cx - 0.02f, 0.19f, 0.02f, 8);
        }
    }
    glPopMatrix();
}

// ================================================================ PEOPLE (drawn in metres, feet at origin)
float smooth01(float t) { t = clampf(t, 0.0f, 1.0f); return t * t * (3.0f - 2.0f * t); }

// 0 = hand rests, 1 = hand at the mouth (a bite), repeats about every 2 seconds
float eatLift(int id) {
    float t = fmodf(g_time * 0.45f + id * 0.37f, 1.0f);
    if (t < 0.5f)  return 0.0f;
    if (t < 0.65f) return smooth01((t - 0.5f) / 0.15f);
    if (t < 0.82f) return 1.0f;
    if (t < 0.97f) return 1.0f - smooth01((t - 0.82f) / 0.15f);
    return 0.0f;
}
// polygon that is star-shaped around (cx,cy): fan from the centre, outline on the border
void fillStar(float cx, float cy, const float* p, int n, Col fill) {
    setCol(fill);
    glBegin(GL_TRIANGLE_FAN); glVertex2f(cx, cy);
    for (int i = 0; i < n; i++) glVertex2f(p[2 * i], p[2 * i + 1]);
    glVertex2f(p[0], p[1]); glEnd();
    setCol(INK, 0.9f); glLineWidth(g_lw);
    glBegin(GL_LINE_LOOP); for (int i = 0; i < n; i++) glVertex2f(p[2 * i], p[2 * i + 1]); glEnd();
}
// thick segment (arm piece) with outline
void limb(float ax, float ay, float bx, float by, float wa, float wb, Col fill) {
    float dx = bx - ax, dy = by - ay, d = sqrtf(dx * dx + dy * dy);
    if (d < 1e-5f) return;
    float nx = -dy / d, ny = dx / d;
    float p[8] = { ax + nx * wa, ay + ny * wa, bx + nx * wb, by + ny * wb, bx - nx * wb, by - ny * wb, ax - nx * wa, ay - ny * wa };
    fillPoly(p, 4, fill);
}
void flower(float cx, float cy, float r, Col petal, Col center) {
    setCol(petal);
    for (int i = 0; i < 5; i++) { float a = i * 1.2566f + 0.3f; circle(cx + cosf(a) * r * 0.62f, cy + sinf(a) * r * 0.62f, r * 0.5f, 8); }
    setCol(center); circle(cx, cy, r * 0.32f, 8);
}
// two-bone arm: finds the elbow so the hand reaches (tx,ty)
void ik2(float sx, float sy, float tx, float ty, float l1, float l2, float& ex, float& ey, float& hx, float& hy) {
    float dx = tx - sx, dy = ty - sy, d0 = sqrtf(dx * dx + dy * dy);
    if (d0 < 1e-4f) { dx = 0; dy = -1; d0 = 1e-4f; }
    float ux = dx / d0, uy = dy / d0;
    float d = clampf(d0, fabsf(l1 - l2) + 0.01f, l1 + l2 - 0.005f);
    float a = (l1 * l1 - l2 * l2 + d * d) / (2.0f * d);
    float h2 = l1 * l1 - a * a, h = h2 > 0 ? sqrtf(h2) : 0.0f;
    float nx = uy, ny = -ux;
    if (ny > 0) { nx = -nx; ny = -ny; }                    // elbow points downwards
    ex = sx + ux * a + nx * h; ey = sy + uy * a + ny * h;
    hx = sx + ux * d;          hy = sy + uy * d;
}
// the piece of food that goes to the mouth (stick / chopsticks / spoon), drawn pointing up from the hand
void drawBite(int type, float hx, float hy, float ang) {
    glPushMatrix(); glTranslatef(hx, hy, 0); glRotatef(ang, 0, 0, 1);
    if (type == 0) {                                      // takoyaki on a pick
        setCol(C(0.75f, 0.6f, 0.4f)); line(0, -0.03f, 0, 0.13f, 1.6f);
        setCol(C(0.82f, 0.52f, 0.18f)); circle(0, 0.15f, 0.032f, 10);
        setCol(C(0.35f, 0.15f, 0.08f)); circle(-0.004f, 0.16f, 0.017f, 8);
    } else if (type == 1) {                               // yakisoba with chopsticks
        setCol(C(0.8f, 0.65f, 0.4f)); line(-0.008f, -0.03f, -0.004f, 0.15f, 1.4f); line(0.008f, -0.03f, 0.01f, 0.15f, 1.4f);
        setCol(C(0.9f, 0.64f, 0.22f)); circle(0.003f, 0.16f, 0.022f, 8);
        setCol(C(0.6f, 0.34f, 0.10f)); line(-0.01f, 0.15f, 0.0f, 0.20f, 1.2f); line(0.012f, 0.15f, 0.01f, 0.20f, 1.2f);
    } else if (type == 2) {                               // piece of okonomiyaki
        setCol(C(0.75f, 0.6f, 0.4f)); line(0, -0.03f, 0, 0.12f, 1.6f);
        float w[6] = { -0.03f, 0.12f, 0.03f, 0.12f, 0.0f, 0.185f }; fillPoly(w, 3, C(0.72f, 0.46f, 0.20f));
        setCol(C(0.42f, 0.18f, 0.10f)); tri(-0.025f, 0.13f, 0.025f, 0.13f, 0.0f, 0.175f);
    } else {                                              // kakigori spoon
        setCol(C(0.85f, 0.85f, 0.88f)); line(0, -0.03f, 0, 0.10f, 1.6f);
        ellipseOut(0, 0.12f, 0.022f, 0.028f, C(1, 1, 1));
        setCol(C(0.95f, 0.2f, 0.3f)); circle(0, 0.135f, 0.026f, 8);
    }
    glPopMatrix();
}
// the dish held in the other hand (origin = top of the palm)
void drawTray(int type, float x, float y, Col ice) {
    glPushMatrix(); glTranslatef(x, y, 0);
    if (type == 0) {                                      // takoyaki boat
        float b[8] = { -0.10f, 0.0f, 0.10f, 0.0f, 0.08f, -0.04f, -0.08f, -0.04f }; fillPoly(b, 4, C(0.85f, 0.70f, 0.45f));
        for (int i = 0; i < 4; i++) {
            setCol(C(0.80f, 0.50f, 0.17f)); circle(-0.06f + i * 0.04f, 0.025f, 0.026f, 10);
            setCol(C(0.35f, 0.15f, 0.08f)); circle(-0.06f + i * 0.04f, 0.032f, 0.014f, 8);
        }
    } else if (type == 1) {                               // yakisoba paper tray
        float b[8] = { -0.10f, 0.0f, 0.10f, 0.0f, 0.085f, -0.04f, -0.085f, -0.04f }; fillPoly(b, 4, C(0.97f, 0.95f, 0.90f));
        setCol(C(0.90f, 0.62f, 0.22f)); ellipse(0, 0.02f, 0.085f, 0.035f, 16);
        line(-0.06f, 0.02f, 0.05f, 0.03f, 1.2f); line(-0.05f, 0.035f, 0.06f, 0.015f, 1.2f);
        setCol(C(0.35f, 0.70f, 0.25f)); circle(-0.03f, 0.04f, 0.01f, 6);
        setCol(C(0.85f, 0.15f, 0.15f)); circle(0.04f, 0.04f, 0.009f, 6);
    } else if (type == 2) {                               // okonomiyaki on a paper plate
        setCol(C(0.97f, 0.97f, 0.95f)); ellipse(0, 0, 0.11f, 0.025f, 18);
        ellipseOut(0, 0.02f, 0.085f, 0.03f, C(0.72f, 0.46f, 0.20f));
        setCol(C(0.42f, 0.18f, 0.10f)); ellipse(0, 0.028f, 0.07f, 0.02f, 16);
        setCol(C(1.0f, 0.95f, 0.8f)); glLineWidth(1.2f);
        glBegin(GL_LINE_STRIP); for (int i = 0; i <= 8; i++) glVertex2f(-0.06f + i * 0.015f, 0.03f + ((i % 2) ? 0.01f : -0.005f)); glEnd();
    } else {                                              // kakigori cup
        float c[8] = { -0.05f, 0.0f, 0.05f, 0.0f, 0.035f, -0.10f, -0.035f, -0.10f }; fillPoly(c, 4, C(1, 1, 1), 0.9f);
        ellipseOut(0, 0.03f, 0.06f, 0.055f, ice);
        setCol(C(1, 1, 1), 0.7f); circle(-0.02f, 0.05f, 0.015f, 8);
    }
    glPopMatrix();
}
void drawFan(float x, float y, float rot, Col a, Col b) {
    glPushMatrix(); glTranslatef(x, y, 0); glRotatef(rot, 0, 0, 1);
    setCol(C(0.55f, 0.35f, 0.20f)); line(0, -0.12f, 0, 0, 2.0f);
    ellipseOut(0, 0.1f, 0.11f, 0.11f, a);
    setCol(b, 0.85f); circle(0, 0.1f, 0.06f, 12);
    setCol(C(1, 1, 1), 0.9f); circle(-0.02f, 0.12f, 0.015f, 6);
    glPopMatrix();
}

// ---------------------------------------------------------------- walkers / vendors (front or back view)
void drawPersonFB(const Person& p, float X, float Z) {
    float k = KZ(Z), s = p.child ? 0.70f : 1.0f;
    float sx = PX(X, Z), sy = PY(0, Z), hpx = 1.8f * k * s;
    if (sx < -hpx || sx > W + hpx || sy > H + 30 || sy + hpx < 0) return;
    bool front = (p.dir < 0);
    float ph = p.phase;
    float liftL = p.idle ? 0.0f : fmaxf(0.0f, sinf(ph)) * 0.07f;
    float liftR = p.idle ? 0.0f : fmaxf(0.0f, -sinf(ph)) * 0.07f;
    float bob   = p.idle ? 0.004f * sinf(g_time * 2 + p.id) : fabsf(sinf(ph)) * 0.025f;
    float sway  = p.idle ? 0.0f : sinf(ph) * 2.5f;
    float armSw = p.vendor ? sinf(g_time * 6 + p.id) * 18.0f : (p.idle ? sinf(g_time * 1.5f + p.id) * 3.0f : sinf(ph) * 10.0f);
    g_lw = clampf(k * s * 0.014f, 0.8f, 2.4f);

    Col kim  = fogc(tint(p.kimono), Z), obi = fogc(tint(p.obi), Z), hair = fogc(tint(p.hair), Z);
    Col skin = fogc(tint(C(1.0f, 0.85f, 0.74f)), Z), geta = fogc(tint(C(0.30f, 0.18f, 0.10f)), Z);
    Col light = mix(kim, C(1, 1, 1), 0.55f), dark = scaleCol(kim, 0.65f);
    Col white = fogc(tint(C(1, 1, 1)), Z), red = fogc(tint(C(0.88f, 0.15f, 0.22f)), Z);
    Col gold = C(0.97f, 0.80f, 0.30f), hlight = mix(hair, C(1, 1, 1), 0.3f);
    Col eyeCol = fogc(tint(C(0.34f, 0.18f, 0.10f)), Z);
    bool eating = (p.food > 0 && front && !p.vendor);
    float lift = eating ? eatLift(p.id) : 0.0f;

    glPushMatrix();
    glTranslatef(sx, sy, 0); glScalef(k * s, k * s, 1);
    setCol(C(0, 0, 0), 0.25f); ellipse(0, 0, 0.30f, 0.06f, 16);                       // contact shadow
    glRotatef(sway, 0, 0, 1); glTranslatef(0, bob, 0);

    // legs, geta sandals with red straps
    for (int sg = -1; sg <= 1; sg += 2) {
        float l = (sg < 0) ? liftL : liftR, fx = sg * 0.085f;
        setCol(skin); rect(fx - 0.03f, 0.05f + l, 0.06f, 0.25f);
        ellipseOut(fx, 0.035f + l, 0.07f, 0.03f, geta);
        setCol(red); tri(fx - 0.025f, 0.048f + l, fx + 0.025f, 0.048f + l, fx, 0.085f + l);
    }
    // yukata body
    float bp[16] = { -0.25f, 0.27f, 0.25f, 0.27f, 0.24f, 0.62f, 0.17f, 0.95f, 0.21f, 1.21f, -0.21f, 1.21f, -0.17f, 0.95f, -0.24f, 0.62f };
    fillStar(0, 0.7f, bp, 8, kim);
    setCol(C(0, 0, 0), 0.10f);                                                         // soft shading on one side
    glBegin(GL_TRIANGLE_FAN); glVertex2f(0.10f, 0.28f); glVertex2f(0.245f, 0.28f); glVertex2f(0.235f, 0.62f); glVertex2f(0.165f, 0.95f); glVertex2f(0.09f, 0.95f); glEnd();
    setCol(dark, 0.5f); line(-0.07f, 0.30f, -0.05f, 0.85f, 1.0f); line(0.07f, 0.30f, 0.05f, 0.85f, 1.0f);   // cloth folds
    setCol(dark); rect(-0.25f, 0.27f, 0.50f, 0.06f);                                    // hem band
    if (p.style == 0) {
        static const float fx[9] = { -0.12f, 0.10f, -0.04f, 0.14f, -0.13f, 0.02f, -0.11f, 0.12f, 0.0f };
        static const float fy[9] = {  0.40f, 0.46f,  0.58f, 0.68f,  0.76f, 0.34f,  1.08f, 1.10f, 0.74f };
        for (int i = 0; i < 9; i++) flower(fx[i], fy[i], 0.042f, light, gold);
    } else if (p.style == 1) {
        setCol(light, 0.8f);
        for (int i = -3; i <= 3; i++) { line(i * 0.055f, 0.34f, i * 0.05f, 0.84f, 1.2f); line(i * 0.05f, 1.05f, i * 0.045f, 1.2f, 1.2f); }
        for (int i = 0; i < 6; i++) circle(-0.15f + i * 0.06f, 0.30f + (i % 2) * 0.04f, 0.012f, 6);
    } else if (p.style == 2) {
        flower(-0.12f, 0.45f, 0.085f, light, gold); flower(0.12f, 0.40f, 0.07f, light, gold);
        flower(0.0f, 0.64f, 0.06f, light, gold); flower(0.1f, 1.1f, 0.04f, light, gold); flower(-0.1f, 1.1f, 0.04f, light, gold);
    } else {
        setCol(light, 0.8f);
        for (int r = 0; r < 4; r++) {
            glLineWidth(1.3f); glBegin(GL_LINE_STRIP);
            for (int i = 0; i <= 16; i++) { float x = -0.2f + i * 0.025f; glVertex2f(x, 0.38f + r * 0.14f + 0.012f * sinf(x * 40.0f + r)); }
            glEnd();
        }
    }
    glowEllipse(0, 0.75f, 0.40f, 0.70f, C(1.0f, 0.60f, 0.25f), g_lamp * 0.22f);       // warm lantern light on the cloth

    // obi (wide belt) with stripes, cord, jewel and scarf
    float oh = p.woman ? 0.17f : 0.10f, oy = p.woman ? 0.86f : 0.90f;
    float ob[8] = { -0.19f, oy, 0.19f, oy, 0.195f, oy + oh, -0.195f, oy + oh };
    fillPoly(ob, 4, obi);
    setCol(gold, 0.9f); rect(-0.19f, oy + oh * 0.15f, 0.38f, 0.01f); rect(-0.195f, oy + oh * 0.8f, 0.39f, 0.01f);
    if (front) {
        if (p.woman) { setCol(mix(obi, white, 0.5f)); glBegin(GL_QUADS); glVertex2f(-0.17f, oy + oh); glVertex2f(0.17f, oy + oh); glVertex2f(0.15f, oy + oh + 0.045f); glVertex2f(-0.15f, oy + oh + 0.045f); glEnd(); }
        setCol(red); line(-0.19f, oy + oh * 0.5f, 0.19f, oy + oh * 0.5f, 1.6f);
        setCol(gold); circle(0, oy + oh * 0.5f, 0.022f, 10);
    } else if (p.woman) {                                                              // big bow at the back
        for (int sg = -1; sg <= 1; sg += 2) {
            glPushMatrix(); glTranslatef(sg * 0.12f, 0.97f, 0); glRotatef(-sg * 22.0f, 0, 0, 1);
            ellipseOut(0, 0, 0.12f, 0.075f, obi);
            setCol(gold, 0.8f); line(-0.08f, 0, 0.08f, 0, 1.2f);
            glPopMatrix();
        }
        float t1[8] = { -0.07f, 0.88f, -0.01f, 0.88f, -0.02f, 0.60f, -0.10f, 0.62f };
        float t2[8] = { 0.01f, 0.88f, 0.07f, 0.88f, 0.10f, 0.62f, 0.02f, 0.60f };
        fillPoly(t1, 4, scaleCol(obi, 0.92f)); fillPoly(t2, 4, scaleCol(obi, 0.92f));
        ellipseOut(0, 0.95f, 0.045f, 0.045f, scaleCol(obi, 1.1f));
    }
    // collar (right-over-left) / back collar and nape
    if (front) {
        setCol(white); tri(-0.075f, 1.215f, 0.075f, 1.215f, 0.0f, 0.975f);
        setCol(skin);  tri(-0.05f, 1.215f, 0.05f, 1.215f, 0.0f, 1.11f);
        setCol(INK, 0.8f); line(-0.075f, 1.215f, 0.0f, 0.975f, 1.5f); line(0.075f, 1.215f, 0.0f, 0.975f, 1.5f);
        setCol(dark, 0.9f); line(-0.05f, 1.2f, 0.12f, 0.99f, 1.5f);
    } else { setCol(white); rect(-0.075f, 1.19f, 0.15f, 0.035f); }

    // neck and head
    setCol(skin); rect(-0.04f, 1.19f, 0.08f, 0.12f);
    float hy = 1.35f;
    if (front) {
        ellipseOut(0, hy + 0.04f, 0.135f, 0.14f, hair);                                  // back hair
        if (p.woman) {
            float lk[8] = { -0.13f, 1.42f, -0.095f, 1.42f, -0.10f, 1.20f, -0.14f, 1.20f };
            float rk[8] = {  0.13f, 1.42f,  0.095f, 1.42f,  0.10f, 1.20f,  0.14f, 1.20f };
            fillPoly(lk, 4, hair); fillPoly(rk, 4, hair);                                // loose side locks
        }
        ellipseOut(0, hy, 0.118f, 0.13f, skin);                                          // face
        setCol(hair); arcFill(0, hy + 0.07f, 0.122f, 5, 175, 14);                        // fringe
        for (int i = 0; i < 5; i++) {
            float x0 = -0.115f + i * 0.046f, x1 = x0 + 0.046f, tip = (i % 2) ? 1.385f : 1.372f;
            tri(x0, 1.42f, x1, 1.42f, (x0 + x1) * 0.5f + 0.01f, tip);
        }
        setCol(hlight, 0.7f); glLineWidth(2.0f);                                         // hair shine
        glBegin(GL_LINE_STRIP); for (int a = 40; a <= 140; a += 10) glVertex2f(cosf(a * PI / 180) * 0.105f, 1.42f + sinf(a * PI / 180) * 0.10f); glEnd();
        for (int sg = -1; sg <= 1; sg += 2) {                                            // big anime eyes
            float ex = sg * 0.048f, ey = 1.32f;
            setCol(INK); ellipse(ex, ey, 0.024f, 0.034f, 14);
            setCol(C(1, 1, 1)); ellipse(ex, ey, 0.019f, 0.029f, 14);
            setCol(eyeCol); ellipse(ex, ey - 0.003f, 0.0165f, 0.026f, 14);
            setCol(INK); ellipse(ex, ey - 0.004f, 0.009f, 0.016f, 10);
            setCol(C(1, 1, 1)); circle(ex - 0.006f, ey + 0.011f, 0.007f, 8); circle(ex + 0.006f, ey - 0.011f, 0.003f, 6);
            setCol(INK); glLineWidth(2.0f);
            glBegin(GL_LINE_STRIP); glVertex2f(ex - 0.027f, ey + 0.015f); glVertex2f(ex, ey + 0.034f); glVertex2f(ex + 0.027f, ey + 0.02f); glEnd();
            setCol(hair); line(ex - 0.025f, 1.375f, ex + 0.025f, 1.38f, 1.5f);          // brows
            setCol(C(1.0f, 0.55f, 0.55f), 0.55f); ellipse(sg * 0.075f, 1.28f, 0.026f, 0.014f, 10);   // blush
        }
        setCol(skin); setCol(mix(skin, C(0.6f, 0.3f, 0.2f), 0.4f)); line(0.0f, 1.297f, 0.006f, 1.286f, 1.2f);
        if (eating && lift > 0.9f) { setCol(C(0.55f, 0.12f, 0.15f)); ellipse(0.0f, 1.262f, 0.014f, 0.016f, 10); }   // biting
        else { setCol(C(0.55f, 0.12f, 0.15f)); arcFill(0, 1.272f, 0.024f, 185, 355, 10); setCol(C(1.0f, 0.5f, 0.55f)); arcFill(0, 1.262f, 0.012f, 200, 340, 8); }
    } else {
        setCol(skin); rect(-0.05f, 1.2f, 0.10f, 0.12f);                                  // nape
        ellipseOut(0, hy + 0.04f, 0.135f, 0.14f, hair);
        setCol(hlight, 0.6f); glLineWidth(2.0f);
        glBegin(GL_LINE_STRIP); for (int a = 50; a <= 130; a += 10) glVertex2f(cosf(a * PI / 180) * 0.11f, 1.40f + sinf(a * PI / 180) * 0.10f); glEnd();
    }
    if (p.vendor) { setCol(white); rect(-0.125f, 1.435f, 0.25f, 0.045f); }              // headband
    else if (p.woman) {                                                                  // hair bun + flower pin + tassel
        ellipseOut(0, 1.545f, 0.07f, 0.062f, hair);
        Col petal = fogc(tint(C(1.0f, 0.95f, 0.97f)), Z), pink = fogc(tint(C(1.0f, 0.62f, 0.78f)), Z);
        flower(0.07f, 1.55f, 0.04f, petal, gold); flower(0.045f, 1.59f, 0.028f, pink, gold); flower(0.10f, 1.52f, 0.025f, petal, gold);
        if (front) { setCol(gold); line(0.07f, 1.51f, 0.085f, 1.41f, 1.0f); circle(0.085f, 1.40f, 0.012f, 6); circle(0.088f, 1.375f, 0.012f, 6); }
    } else {                                                                             // short spiky hair for men
        setCol(hair);
        tri(-0.11f, 1.49f, -0.06f, 1.57f, -0.01f, 1.50f); tri(-0.04f, 1.50f, 0.01f, 1.58f, 0.06f, 1.50f); tri(0.03f, 1.49f, 0.08f, 1.55f, 0.115f, 1.47f);
    }

    // sleeves, arms, things in hand
    for (int sg = -1; sg <= 1; sg += 2) {
        if (eating && sg > 0) {                                                          // right arm brings the food to the mouth
            float tx = lerpf(0.24f, 0.07f, lift), ty = lerpf(0.92f, 1.24f, lift);
            float ex, ey, hx, hy2;
            ik2(0.17f, 1.14f, tx, ty, 0.22f, 0.32f, ex, ey, hx, hy2);
            limb(0.17f, 1.15f, ex, ey, 0.05f, 0.045f, scaleCol(kim, 0.92f));
            limb(ex, ey, hx, hy2, 0.035f, 0.028f, skin);
            float sl2[8] = { ex - 0.05f, ey + 0.03f, ex + 0.09f, ey + 0.02f, ex + 0.12f, ey - 0.20f, ex - 0.02f, ey - 0.20f };
            fillPoly(sl2, 4, scaleCol(kim, 0.92f));
            setCol(skin); circle(hx, hy2, 0.03f, 10);
            drawBite(p.food - 1, hx, hy2 + 0.01f, 10.0f + lift * 25.0f);
            continue;
        }
        glPushMatrix(); glTranslatef(sg * 0.19f, 1.17f, 0); glRotatef(sg * (2.0f + armSw * 0.35f * (sg < 0 ? 1 : -1)), 0, 0, 1);
        float sl[10] = { -sg * 0.02f, 0.03f, sg * 0.13f, 0.01f, sg * 0.17f, -0.25f, sg * 0.12f, -0.42f, 0.0f, -0.38f };
        fillPoly(sl, 5, scaleCol(kim, 0.92f));
        flower(sg * 0.10f, -0.20f, 0.035f, light, gold);
        setCol(skin); circle(sg * 0.07f, -0.44f, 0.03f, 10);
        if (p.fan && front && sg > 0) drawFan(sg * 0.09f, -0.42f, 12.0f, fogc(tint(C(1.0f, 0.97f, 0.9f)), Z), fogc(tint(C(0.35f, 0.55f, 0.9f)), Z));
        glPopMatrix();
    }
    if (p.lantern) {                                                                     // hand-held paper lantern
        setCol(C(0.45f, 0.30f, 0.20f)); line(-0.30f, 0.62f, -0.40f, 0.95f, 2.0f);
        drawLantern(-0.40f, 0.82f, 0.17f, 0.21f, C(0.98f, 0.55f, 0.15f), p.id, g_lw);
    }
    if (p.id % 3 == 0 && p.woman && !p.vendor) {                                         // small kinchaku bag
        setCol(C(0.3f, 0.2f, 0.15f)); line(-0.20f, 0.95f, -0.23f, 0.76f, 1.2f);
        ellipseOut(-0.23f, 0.72f, 0.045f, 0.055f, obi);
    }
    glPopMatrix();
}

// ---------------------------------------------------------------- customers at the stalls (side view, facing the counter)
void drawPersonSide(const Person& p, float X, float Z) {
    float k = KZ(Z), s = p.child ? 0.70f : 1.0f;
    float sx = PX(X, Z), sy = PY(0, Z), hpx = 1.8f * k * s;
    if (sx < -hpx || sx > W + hpx || sy > H + 30 || sy + hpx < 0) return;
    float face = (float)p.face;
    g_lw = clampf(k * s * 0.014f, 0.8f, 2.4f);
    Col kim  = fogc(tint(p.kimono), Z), obi = fogc(tint(p.obi), Z), hair = fogc(tint(p.hair), Z);
    Col skin = fogc(tint(C(1.0f, 0.85f, 0.74f)), Z), geta = fogc(tint(C(0.30f, 0.18f, 0.10f)), Z);
    Col light = mix(kim, C(1, 1, 1), 0.55f), dark = scaleCol(kim, 0.65f);
    Col white = fogc(tint(C(1, 1, 1)), Z), red = fogc(tint(C(0.88f, 0.15f, 0.22f)), Z);
    Col gold = C(0.97f, 0.80f, 0.30f), eyeCol = fogc(tint(C(0.34f, 0.18f, 0.10f)), Z);
    static const Col ices[3] = { {0.95f, 0.20f, 0.30f}, {0.25f, 0.55f, 0.95f}, {0.45f, 0.85f, 0.35f} };
    float lift = eatLift(p.id);
    int ft = p.food - 1;

    glPushMatrix();
    glTranslatef(sx, sy, 0); glScalef(face * k * s, k * s, 1);                           // local +x = the direction he/she faces
    setCol(C(0, 0, 0), 0.25f); ellipse(0, 0, 0.30f, 0.06f, 16);

    // far arm (behind the body) holds the dish
    float fex, fey, fhx, fhy;
    ik2(-0.01f, 1.13f, 0.22f, 0.95f, 0.28f, 0.26f, fex, fey, fhx, fhy);
    limb(-0.01f, 1.13f, fex, fey, 0.045f, 0.04f, scaleCol(kim, 0.8f));
    limb(fex, fey, fhx, fhy, 0.032f, 0.027f, scaleCol(skin, 0.9f));

    // legs
    for (int i = 0; i < 2; i++) {
        float lx = (i == 0) ? -0.04f : 0.04f;
        setCol(i == 0 ? scaleCol(skin, 0.9f) : skin); rect(lx - 0.03f, 0.05f, 0.06f, 0.25f);
        ellipseOut(lx + 0.025f, 0.035f, 0.075f, 0.03f, geta);
        setCol(red); tri(lx - 0.01f, 0.048f, lx + 0.05f, 0.048f, lx + 0.02f, 0.085f);
    }
    // body (side silhouette) with obi and a big bow at the back
    for (int sg = 0; sg < 2; sg++) {
        glPushMatrix(); glTranslatef(-0.20f, 0.97f + sg * 0.03f, 0); glRotatef(25.0f - sg * 50.0f, 0, 0, 1);
        ellipseOut(0, 0, 0.11f, 0.075f, obi); setCol(gold, 0.8f); line(-0.07f, 0, 0.07f, 0, 1.2f);
        glPopMatrix();
    }
    float tl[8] = { -0.13f, 0.90f, -0.19f, 0.90f, -0.23f, 0.62f, -0.15f, 0.60f };
    fillPoly(tl, 4, scaleCol(obi, 0.92f));
    float bp[18] = { -0.15f, 0.27f, 0.16f, 0.27f, 0.15f, 0.62f, 0.13f, 0.95f, 0.12f, 1.15f, 0.06f, 1.22f, -0.07f, 1.22f, -0.11f, 1.0f, -0.14f, 0.62f };
    fillStar(0, 0.7f, bp, 9, kim);
    setCol(dark); rect(-0.15f, 0.27f, 0.31f, 0.06f);
    flower(-0.02f, 0.48f, 0.05f, light, gold); flower(0.06f, 0.66f, 0.04f, light, gold); flower(-0.04f, 0.78f, 0.035f, light, gold);
    glowEllipse(0, 0.75f, 0.35f, 0.65f, C(1.0f, 0.60f, 0.25f), g_lamp * 0.22f);
    float ob[8] = { -0.14f, 0.86f, 0.14f, 0.86f, 0.145f, 1.03f, -0.135f, 1.03f };
    fillPoly(ob, 4, obi);
    setCol(gold, 0.9f); rect(-0.14f, 0.885f, 0.28f, 0.01f); rect(-0.135f, 1.0f, 0.28f, 0.01f);
    setCol(white); line(0.08f, 1.21f, 0.115f, 1.03f, 3.0f);                              // collar

    // head in profile
    setCol(skin); rect(-0.03f, 1.19f, 0.07f, 0.12f);
    ellipseOut(-0.04f, 1.39f, 0.14f, 0.145f, hair);                                      // back hair
    ellipseOut(0.03f, 1.35f, 0.105f, 0.125f, skin);                                      // face
    float nz[6] = { 0.10f, 1.345f, 0.145f, 1.315f, 0.10f, 1.305f }; fillPoly(nz, 3, skin);   // nose
    setCol(hair); arcFill(0.02f, 1.415f, 0.115f, 5, 170, 14);                            // fringe
    tri(0.07f, 1.415f, 0.125f, 1.40f, 0.115f, 1.365f); tri(0.02f, 1.415f, 0.08f, 1.415f, 0.06f, 1.375f);
    setCol(INK); ellipse(0.075f, 1.325f, 0.018f, 0.032f, 14);
    setCol(C(1, 1, 1)); ellipse(0.075f, 1.325f, 0.014f, 0.027f, 14);
    setCol(eyeCol); ellipse(0.078f, 1.322f, 0.012f, 0.024f, 14);
    setCol(INK); ellipse(0.079f, 1.32f, 0.007f, 0.015f, 10);
    setCol(C(1, 1, 1)); circle(0.074f, 1.337f, 0.006f, 8);
    setCol(INK); glLineWidth(2.0f); glBegin(GL_LINE_STRIP); glVertex2f(0.055f, 1.34f); glVertex2f(0.08f, 1.357f); glVertex2f(0.10f, 1.343f); glEnd();
    setCol(hair); line(0.06f, 1.377f, 0.10f, 1.375f, 1.5f);
    setCol(C(1.0f, 0.55f, 0.55f), 0.55f); ellipse(0.055f, 1.285f, 0.026f, 0.014f, 10);
    setCol(C(0.55f, 0.12f, 0.15f));
    if (lift > 0.9f) ellipse(0.115f, 1.262f, 0.012f, 0.014f, 10);                        // mouth open for the bite
    else { glLineWidth(1.4f); glBegin(GL_LINE_STRIP); glVertex2f(0.095f, 1.268f); glVertex2f(0.11f, 1.262f); glVertex2f(0.122f, 1.27f); glEnd(); }
    if (p.woman) {
        ellipseOut(-0.07f, 1.545f, 0.07f, 0.062f, hair);
        Col petal = fogc(tint(C(1.0f, 0.95f, 0.97f)), Z), pink = fogc(tint(C(1.0f, 0.62f, 0.78f)), Z);
        flower(-0.02f, 1.55f, 0.04f, petal, gold); flower(-0.045f, 1.59f, 0.028f, pink, gold);
        setCol(gold); line(-0.02f, 1.51f, -0.05f, 1.41f, 1.0f); circle(-0.05f, 1.40f, 0.012f, 6); circle(-0.052f, 1.375f, 0.012f, 6);
    } else {
        setCol(hair); tri(-0.09f, 1.49f, -0.03f, 1.57f, 0.02f, 1.50f); tri(0.0f, 1.50f, 0.06f, 1.56f, 0.11f, 1.47f);
    }

    // the dish in the far hand
    setCol(skin); circle(fhx, fhy, 0.03f, 10);
    drawTray(ft, fhx, fhy + 0.02f, fogc(ices[p.id % 3], Z));
    // near arm: lifts the food to the mouth
    float tx = lerpf(0.23f, 0.12f, lift), ty = lerpf(0.99f, 1.27f, lift);
    float ex, ey, hx, hy;
    ik2(0.01f, 1.16f, tx, ty, 0.28f, 0.26f, ex, ey, hx, hy);
    limb(0.01f, 1.16f, ex, ey, 0.052f, 0.047f, scaleCol(kim, 0.95f));
    limb(ex, ey, hx, hy, 0.036f, 0.028f, skin);
    float sl2[8] = { ex - 0.05f, ey + 0.03f, ex + 0.09f, ey + 0.02f, ex + 0.12f, ey - 0.22f, ex - 0.03f, ey - 0.20f };
    fillPoly(sl2, 4, scaleCol(kim, 0.95f));
    flower(ex + 0.04f, ey - 0.1f, 0.03f, light, gold);
    setCol(skin); circle(hx, hy, 0.03f, 10);
    drawBite(ft, hx, hy + 0.01f, -(30.0f + 25.0f * lift));
    glPopMatrix();
}

void drawPerson(const Person& p, float X, float Z) {
    if (p.face != 0) drawPersonSide(p, X, Z); else drawPersonFB(p, X, Z);
}

// ================================================================ STALL
void drawStall(const Stall& s, int idx) {
    const float side = (float)s.side, z0 = s.z0, z1 = s.z1, zm = 0.5f * (z0 + z1);
    const float xf = side * 3.2f, xb = side * 5.0f, xo = side * 2.55f;
    const float k = KZ(zm);
    bool detail = z0 < 16.0f;
    Col roofC = fogc(tint(s.roof), zm), white = fogc(tint(C(1, 1, 1)), zm);
    Col warm = C(1.0f, 0.70f, 0.30f);
    Col woodD = fogc(dn(C(0.50f, 0.33f, 0.18f), C(0.26f, 0.15f, 0.08f)), zm);
    Col woodL = fogc(dn(C(0.72f, 0.50f, 0.30f), C(0.40f, 0.24f, 0.12f)), zm);

    setCol(C(0, 0, 0), 0.2f); poly4(v3(xf, 0, z0), v3(xf, 0, z1), v3(side * 2.35f, 0, z1), v3(side * 2.35f, 0, z0));

    // interior back wall (warm light at night)
    Col inB = fogc(mix(C(0.62f, 0.48f, 0.34f), C(1.0f, 0.66f, 0.26f), g_lamp), zm);
    poly4g(v3(xb, 0, z0), v3(xb, 0, z1), v3(xb, 2.6f, z1), v3(xb, 2.6f, z0), inB, scaleCol(inB, 0.5f));
    glowEllipse(PX(xf + side * 0.8f, zm), PY(1.55f, zm), fabsf(PX(xf, z0) - PX(xf, z1)) * 0.6f + 30.0f, 0.9f * k + 10.0f, warm, g_lamp * 0.8f);

    // vendor (stands behind the counter)
    if (detail) {
        static const Col happi[4] = { {0.15f, 0.30f, 0.55f}, {0.20f, 0.45f, 0.35f}, {0.55f, 0.20f, 0.20f}, {0.30f, 0.30f, 0.60f} };
        Person v = { 0, 0, 0, 0, -1, 3, 900 + idx, happi[idx % 4], C(0.95f, 0.95f, 0.95f), C(0.1f, 0.07f, 0.06f), false, false, false, false, true, true, 0, 0 };
        float vz = z1 - 0.55f;
        drawPerson(v, xf + side * 0.55f, vz);
    }

    // counter front with striped cloth skirt
    poly4g(v3(xf, 0, z0), v3(xf, 0, z1), v3(xf, 1.0f, z1), v3(xf, 1.0f, z0), woodD, woodL);
    for (int i = 0; i < 10; i++) {
        float za = z0 + (z1 - z0) * i / 10.0f, zb = z0 + (z1 - z0) * (i + 1) / 10.0f;
        setCol((i % 2) ? white : roofC);
        poly4(v3(xf, 0.1f, za), v3(xf, 0.1f, zb), v3(xf, 0.9f, zb), v3(xf, 0.9f, za));
    }
    setCol(INK, 0.7f);
    edge3(v3(xf, 0.1f, z0), v3(xf, 0.1f, z1), 1.3f);
    edge3(v3(xf, 0.9f, z0), v3(xf, 0.9f, z1), 1.3f);
    setCol(woodL); poly4(v3(xf, 1.0f, z0), v3(xf, 1.0f, z1), v3(xf + side * 0.9f, 1.0f, z1), v3(xf + side * 0.9f, 1.0f, z0));
    setCol(INK, 0.8f); edge3(v3(xf, 1.0f, z0), v3(xf, 1.0f, z1), 1.8f);

    // food
    if (detail) {
        int ft = (s.type >= 0) ? s.type : (s.var % 4);
        float fz[2] = { z0 + 0.7f, z0 + 1.4f };
        for (int u = 0; u < 2; u++) drawFood(ft, PX(xf + side * 0.4f, fz[u]), PY(1.0f, fz[u]), KZ(fz[u]) * 1.0f, fz[u]);
    }
    // smoke
    for (int i = 0; i < MAXSM; i++) {
        const Smoke& sm = g_smoke[i];
        if (!sm.alive || sm.stall != idx) continue;
        float t = sm.age / sm.life, fadeIn = (t < 0.1f) ? t / 0.1f : 1.0f;
        setCol(dn(C(0.96f, 0.96f, 0.96f), C(0.78f, 0.78f, 0.9f)), 0.5f * (1 - t) * fadeIn);
        circle(PX(sm.x, sm.z), PY(sm.y, sm.z), (sm.size + t * 0.14f) * KZ(sm.z), 14);
    }

    // awning (striped, seen from below)
    for (int i = 0; i < 10; i++) {
        float za = z0 + (z1 - z0) * i / 10.0f, zb = z0 + (z1 - z0) * (i + 1) / 10.0f;
        setCol((i % 2) ? white : roofC);
        poly4(v3(xf, 2.6f, za), v3(xf, 2.6f, zb), v3(xo, 2.1f, zb), v3(xo, 2.1f, za));
    }
    // front banner (noren) with scalloped edge
    setCol(roofC); poly4(v3(xo, 1.70f, z0), v3(xo, 1.70f, z1), v3(xo, 2.10f, z1), v3(xo, 2.10f, z0));
    for (int i = 0; i < 8; i++) {
        float za = z0 + (z1 - z0) * i / 8.0f, zb = z0 + (z1 - z0) * (i + 1) / 8.0f;
        setCol((i % 2) ? white : roofC);
        glBegin(GL_TRIANGLES); vtx(v3(xo, 1.70f, za)); vtx(v3(xo, 1.70f, zb)); vtx(v3(xo, 1.58f, 0.5f * (za + zb))); glEnd();
    }
    setCol(INK, 0.8f); edge3(v3(xo, 2.10f, z0), v3(xo, 2.10f, z1), 1.5f);
    if (z0 < 20.0f) {
        static const char* GN[] = { "MATSURI", "YAKITORI", "RAMUNE", "DANGO", "KUJI", "SODA" };
        const char* name = s.name ? s.name : GN[s.var % 6];
        float zL = (PX(xo, z0) < PX(xo, z1)) ? z0 : z1, zR = (zL == z0) ? z1 : z0;
        bannerText(name, xo, zL, zR, 1.70f, 2.10f, 0.80f, 0.30f, 0.85f, C(1, 1, 1), clampf(0.018f * k, 1.0f, 3.5f));
    }
    // posts
    setCol(woodD);
    float pw = clampf(0.07f * k, 1.5f, 9.0f);
    edge3(v3(xo, 0, z0), v3(xo, 2.1f, z0), pw); edge3(v3(xo, 0, z1), v3(xo, 2.1f, z1), pw);
    // hanging bulbs
    for (int b = 0; b < 2; b++) {
        float bz = (b == 0) ? z0 + 0.25f : z1 - 0.25f;
        float bx = PX(xo, bz), by = PY(1.52f, bz), br = 0.045f * KZ(bz);
        setCol(C(0.1f, 0.1f, 0.1f)); line(bx, by, bx, PY(1.62f, bz), 1.2f);
        setCol(C(1.0f, 0.95f, 0.7f)); circle(bx, by, br, 12);
        glowCircle(bx, by, 0.4f * KZ(bz), warm, g_lamp * 0.9f);
    }
    // nobori flag on a pole (flutters)
    if (idx < 4) {
        float xp = xo - side * 0.15f, za = z0 - 0.15f, zb = z0 + 0.35f, sw = side * 0.12f * sinf(g_time * 2.0f + idx);
        setCol(woodD); edge3(v3(xp, 0, za), v3(xp, 3.5f, za), clampf(0.05f * KZ(za), 1.5f, 8.0f));
        setCol(roofC);  poly4(v3(xp, 3.4f, za), v3(xp + sw, 3.4f, zb), v3(xp + sw * 1.5f, 1.9f, zb), v3(xp, 1.9f, za));
        setCol(white);  poly4(v3(xp, 3.0f, za), v3(xp + sw * 0.9f, 3.0f, zb), v3(xp + sw * 1.2f, 2.3f, zb), v3(xp, 2.3f, za));
    }
}

// ================================================================ OVERLAYS
void drawCornerBlossoms(int side) {
    Col a, b, d; pinks(a, b, d, 3.0f);
    float cx = side < 0 ? 30.0f : W - 30.0f, sg = side < 0 ? 1.0f : -1.0f;
    setCol(dn(C(0.30f, 0.20f, 0.15f), C(0.10f, 0.06f, 0.07f)));
    line(cx - sg * 40, H - 120, cx + sg * 260, H - 70, 12); line(cx - sg * 40, H - 200, cx + sg * 180, H - 240, 8);
    blossoms(cx + sg * 70, H - 30, 170, 3 + side, a, b, d, 0.97f);
    blossoms(cx + sg * 210, H - 20, 130, 7 + side, a, b, d, 0.97f);
    blossoms(cx + sg * 10, H - 130, 110, 11 + side, a, b, d, 0.97f);
}
void drawVignette() {
    float a = 0.22f + 0.28f * g_night;
    glBegin(GL_QUADS);
    glColor4f(0, 0, 0.05f, a); glVertex2f(0, 0); glVertex2f(0, H); glColor4f(0, 0, 0.05f, 0); glVertex2f(260, H); glVertex2f(260, 0);
    glColor4f(0, 0, 0.05f, a); glVertex2f(W, 0); glVertex2f(W, H); glColor4f(0, 0, 0.05f, 0); glVertex2f(W - 260, H); glVertex2f(W - 260, 0);
    glColor4f(0, 0, 0.05f, a); glVertex2f(0, 0); glVertex2f(W, 0); glColor4f(0, 0, 0.05f, 0); glVertex2f(W, 130); glVertex2f(0, 130);
    glEnd();
}
void drawPetals() {
    Col c = dn(C(1.0f, 0.75f, 0.85f), C(0.95f, 0.65f, 0.78f));
    for (int i = 0; i < NPET; i++) {
        const Petal& p = g_petals[i];
        glPushMatrix(); glTranslatef(p.x, p.y, 0); glRotatef(p.rot, 0, 0, 1);
        setCol(c, 0.85f); ellipse(0, 0, p.size, p.size * 0.55f, 10); glPopMatrix();
    }
}

// ================================================================ DEPTH SORTING (painter's algorithm)
struct Item { float z; int kind, idx; };
bool farther(const Item& a, const Item& b) { return a.z > b.z; }

void spawnSmoke(int stall, float x, float y, float z) {
    for (int i = 0; i < MAXSM; i++) if (!g_smoke[i].alive) {
        Smoke& s = g_smoke[i];
        s.x = x + (rand01() - 0.5f) * 0.1f; s.y = y; s.z = z + (rand01() - 0.5f) * 0.2f;
        s.vx = (rand01() - 0.5f) * 0.05f; s.vy = 0.28f + rand01() * 0.2f;
        s.age = 0; s.life = 2.2f + rand01() * 1.2f; s.size = 0.05f + rand01() * 0.03f; s.stall = stall; s.alive = true;
        return;
    }
}


// ================================================================ SUN / MOON BUTTON (small, round, bottom-right)
// Day mode shows a MOON (click -> night), night mode shows a SUN (click -> day).
const float MB_X = 1230.0f, MB_Y = 54.0f, MB_R = 28.0f;
void drawModeButton() {
    if (!g_showBtn) return;
    float dxm = g_mx - MB_X, dym = g_my - MB_Y;
    bool hov = (dxm * dxm + dym * dym) <= (MB_R + 10) * (MB_R + 10);
    float n = 1.0f - g_btnMix;                  // 1 = moon, 0 = sun (cross-fades when you click)
    float sunA = 1.0f - n, moonA = n;
    float r = MB_R * (hov ? 1.12f : 1.0f) * (1.0f - 0.2f * g_btnPress);
    float cx = MB_X, cy = MB_Y;
    float pulse = 0.5f + 0.5f * sinf(g_time * 2.5f);

    // soft glows
    glowCircle(cx, cy, r * 2.7f, C(1.0f, 0.60f, 0.15f), sunA * (0.55f + 0.25f * pulse + (hov ? 0.2f : 0.0f)));
    glowCircle(cx, cy, r * 2.4f, C(0.65f, 0.75f, 1.00f), moonA * (0.50f + 0.20f * pulse + (hov ? 0.2f : 0.0f)));

    if (sunA > 0.01f) {                         // ---- SUN: glowing ball with turning rays
        for (int i = 0; i < 12; i++) {          // long rays
            float a = g_time * 0.35f + i * PI / 6.0f, w = 0.13f;
            setCol(C(1.0f, 0.72f, 0.15f), sunA);
            tri(cx + cosf(a - w) * r * 1.08f, cy + sinf(a - w) * r * 1.08f, cx + cosf(a + w) * r * 1.08f, cy + sinf(a + w) * r * 1.08f,
                cx + cosf(a) * r * 1.78f, cy + sinf(a) * r * 1.78f);
        }
        for (int i = 0; i < 12; i++) {          // short rays in between, turning the other way
            float a = -g_time * 0.25f + (i + 0.5f) * PI / 6.0f, w = 0.10f;
            setCol(C(1.0f, 0.86f, 0.30f), sunA * 0.9f);
            tri(cx + cosf(a - w) * r * 1.08f, cy + sinf(a - w) * r * 1.08f, cx + cosf(a + w) * r * 1.08f, cy + sinf(a + w) * r * 1.08f,
                cx + cosf(a) * r * 1.42f, cy + sinf(a) * r * 1.42f);
        }
        for (int i = 0; i < 8; i++) {           // ball: orange edge -> pale yellow centre
            float t = i / 7.0f;
            setCol(mix(C(1.0f, 0.52f, 0.08f), C(1.0f, 0.97f, 0.66f), t), sunA);
            circle(cx, cy, r * (1.0f - t * 0.78f), 36);
        }
    }
    if (moonA > 0.01f) {                        // ---- MOON: shaded sphere with craters and twinkling stars
        for (int i = 0; i < 8; i++) {
            float t = i / 7.0f, rr = r * (1.0f - t * 0.55f);
            setCol(mix(C(0.60f, 0.67f, 0.86f), C(0.99f, 0.97f, 0.86f), t), moonA);
            circle(cx - r * 0.18f * t, cy + r * 0.18f * t, rr, 36);
        }
        setCol(C(0.68f, 0.71f, 0.80f), moonA * 0.75f);
        circle(cx - r * 0.35f, cy + r * 0.30f, r * 0.20f, 16);
        circle(cx + r * 0.30f, cy - r * 0.15f, r * 0.16f, 16);
        circle(cx - r * 0.05f, cy - r * 0.45f, r * 0.12f, 14);
        circle(cx + r * 0.38f, cy + r * 0.36f, r * 0.09f, 12);
        setCol(C(1.0f, 1.0f, 1.0f), moonA * 0.45f);
        circle(cx - r * 0.40f, cy + r * 0.36f, r * 0.07f, 10);
        for (int i = 0; i < 3; i++) {           // little stars around the moon
            float a = 0.9f + i * 2.2f, tw = 0.5f + 0.5f * sinf(g_time * 3.0f + i * 2.0f);
            setCol(C(1.0f, 1.0f, 0.9f), moonA * tw);
            rect(cx + cosf(a) * r * 1.75f - 1.5f, cy + sinf(a) * r * 1.75f - 1.5f, 3.0f, 3.0f);
        }
    }
}

} // namespace

// ================================================================ PUBLIC API
void festivalSetNight(float n) {
    g_night = clampf(n, 0.0f, 1.0f);
    float t = clampf((g_night - 0.35f) / 0.5f, 0.0f, 1.0f);
    g_lamp = t * t * (3.0f - 2.0f * t);
}
float festivalGetNight() { return g_night; }
void festivalSetMouse(float x, float y) { g_mx = x; g_my = y; }
void festivalSetModeButton(bool show, bool showSun) { g_showBtn = show; g_btnSunTarget = showSun ? 1 : 0; }
void festivalPressButton() { g_btnPress = 1.0f; }
bool festivalModeButtonHit(float x, float y) { float dx = x - MB_X, dy = y - MB_Y; return dx * dx + dy * dy <= (MB_R + 10) * (MB_R + 10); }

void festivalInit() {
    srand(2026);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glEnable(GL_LINE_SMOOTH);

    for (int i = 0; i < NSTAR; i++) {
        g_stars[i].x = 250 + rand01() * 780; g_stars[i].y = 420 + rand01() * 300;
        g_stars[i].size = 1.5f + rand01() * 1.5f; g_stars[i].phase = rand01() * 6.28f;
    }
    static const float cx[NCLOUD] = {380, 560, 760, 900}, cy[NCLOUD] = {600, 660, 620, 680};
    for (int i = 0; i < NCLOUD; i++) { g_clouds[i].x = cx[i]; g_clouds[i].y = cy[i]; g_clouds[i].s = 0.8f + rand01() * 0.5f; g_clouds[i].speed = 5 + rand01() * 6; }
    for (int i = 0; i < NPET; i++) {
        g_petals[i].x = rand01() * W; g_petals[i].y = rand01() * H; g_petals[i].vy = 20 + rand01() * 25; g_petals[i].vx = -5 + rand01() * 10;
        g_petals[i].rot = rand01() * 360; g_petals[i].spin = -80 + rand01() * 160; g_petals[i].size = 3 + rand01() * 2.5f; g_petals[i].phase = rand01() * 6.28f;
    }
    for (int i = 0; i < MAXSM; i++) g_smoke[i].alive = false;

    // ---- stalls: four named (Takoyaki, Yakisoba, Okonomiyaki, Kakigori) + generic ones further away
    g_nStalls = 0;
    Stall named[4] = {
        { -1, 2.8f, 5.0f, 0, {0.85f, 0.18f, 0.15f}, "TAKOYAKI",    true,  0 },
        {  1, 2.8f, 5.0f, 1, {0.15f, 0.45f, 0.80f}, "YAKISOBA",    true,  1 },
        { -1, 5.2f, 8.2f, 2, {0.93f, 0.72f, 0.12f}, "OKONOMIYAKI", true,  2 },
        {  1, 5.2f, 8.2f, 3, {0.58f, 0.30f, 0.72f}, "KAKIGORI",    false, 3 } };
    for (int i = 0; i < 4; i++) g_stalls[g_nStalls++] = named[i];
    static const Col roofs[6] = { {0.85f,0.18f,0.15f}, {0.15f,0.45f,0.80f}, {0.93f,0.72f,0.12f}, {0.58f,0.30f,0.72f}, {0.20f,0.65f,0.45f}, {0.95f,0.45f,0.20f} };
    for (int side = -1; side <= 1; side += 2) {
        float z = 8.4f; int i = 0;
        while (z < 42.0f && g_nStalls < MAXS) {
            Stall s = { side, z, z + 3.0f, -1, roofs[(i + (side > 0 ? 3 : 0)) % 6], 0, false, i + (side > 0 ? 2 : 0) };
            g_stalls[g_nStalls++] = s; z += 3.2f; i++;
        }
    }
    for (int i = 0; i < MAXS; i++) g_emit[i] = 0;

    // ---- houses along both sides
    g_nHouses = 0;
    for (int side = -1; side <= 1; side += 2) {
        float z = 2.4f; int i = 0;
        while (z < 80.0f && g_nHouses < MAXH) {
            float len = 3.8f + rand01() * 3.2f;
            static const Col walls[4] = { {0.62f,0.45f,0.30f}, {0.72f,0.58f,0.40f}, {0.55f,0.36f,0.28f}, {0.68f,0.52f,0.36f} };
            House h = { side, z, z + len, 6.2f + rand01() * 2.3f, walls[(i + (side > 0 ? 1 : 0)) % 4] };
            g_houses[g_nHouses++] = h; z += len; i++;
        }
    }
    // ---- sakura trees behind the stalls
    g_nTrees = 0;
    static const float tz[6] = { 6.4f, 11.0f, 16.0f, 22.0f, 30.0f, 40.0f };
    for (int side = -1; side <= 1; side += 2)
        for (int i = 0; i < 6; i++) { Tree t = { side * 5.3f, tz[i] + (side > 0 ? 1.5f : 0.0f), 2.0f + rand01() * 0.5f, i * 3 + (side > 0 ? 5 : 0) }; g_trees[g_nTrees++] = t; }

    // ---- visitors walking in the middle of the street + customers at the stalls
    static const Col fem[8] = { {0.95f,0.50f,0.70f}, {0.30f,0.50f,0.90f}, {0.20f,0.70f,0.65f}, {0.97f,0.60f,0.25f}, {0.62f,0.35f,0.78f}, {0.55f,0.75f,0.35f}, {0.85f,0.20f,0.25f}, {0.97f,0.80f,0.35f} };
    static const Col mal[4] = { {0.20f,0.25f,0.50f}, {0.30f,0.40f,0.35f}, {0.35f,0.28f,0.30f}, {0.22f,0.30f,0.55f} };
    static const Col obis[6] = { {0.95f,0.80f,0.20f}, {0.85f,0.15f,0.20f}, {0.97f,0.97f,0.97f}, {0.95f,0.45f,0.65f}, {0.20f,0.20f,0.30f}, {0.90f,0.55f,0.15f} };
    static const Col hairs[4] = { {0.10f,0.07f,0.07f}, {0.17f,0.10f,0.08f}, {0.08f,0.06f,0.06f}, {0.25f,0.15f,0.10f} };
    g_nPeople = 0;
    for (int i = 0; i < 36 && g_nPeople < MAXP; i++) {
        Person p;
        p.dir = (i % 2) ? 1 : -1;
        p.x = (p.dir < 0 ? -2.3f : 0.1f) + rand01() * 2.2f;
        p.z = 5.2f + rand01() * 40.0f;
        p.speed = 0.6f + rand01() * 0.5f; p.phase = rand01() * 6.28f;
        p.woman = (i % 4 != 3); p.child = (i % 9 == 5);
        p.kimono = p.woman ? fem[i % 8] : mal[i % 4]; p.obi = obis[(i * 5) % 6]; p.hair = hairs[i % 4];
        p.style = i % 4; p.id = i; p.fan = (i % 5 == 1); p.lantern = (i % 7 == 2); p.idle = false; p.vendor = false;
        p.food = (i % 4 == 0) ? 1 + (i / 4) % 4 : 0; if (p.food) { p.fan = false; p.lantern = false; } p.face = 0;
        g_people[g_nPeople++] = p;
    }
    // customers eating at the stalls (side view, facing the counter)
    struct CD { float x, z; int stallType; };
    static const CD cd[8] = {
        {-2.30f, 4.0f, 0}, {-2.10f, 4.7f, 0}, { 2.30f, 4.2f, 1}, { 2.10f, 4.8f, 1},
        {-2.25f, 6.3f, 2}, {-2.20f, 7.5f, 2}, { 2.25f, 6.4f, 3}, { 2.20f, 7.7f, 3} };
    for (int i = 0; i < 8 && g_nPeople < MAXP; i++) {
        Person p;
        p.dir = -1; p.x = cd[i].x; p.z = cd[i].z; p.speed = 0; p.phase = 0;
        p.woman = (i % 2 == 0); p.child = (i == 3);
        p.kimono = p.woman ? fem[(i * 3 + 1) % 8] : mal[i % 4]; p.obi = obis[(i * 2 + 1) % 6]; p.hair = hairs[i % 4];
        p.style = (i + 1) % 4; p.id = 200 + i; p.fan = false; p.lantern = false; p.idle = true; p.vendor = false;
        p.food = cd[i].stallType + 1; p.face = (cd[i].x < 0) ? -1 : 1;
        g_people[g_nPeople++] = p;
    }
}

void festivalUpdate(float dt) {
    if (dt > 0.05f) dt = 0.05f;
    g_time += dt;
    {   // icon swaps quickly between moon and sun, button squishes when clicked
        float d = (g_btnSunTarget ? 1.0f : 0.0f) - g_btnMix, st = dt * 5.0f;
        if (d > st) d = st;
        if (d < -st) d = -st;
        g_btnMix += d;
        g_btnPress -= dt * 4.0f; if (g_btnPress < 0.0f) g_btnPress = 0.0f;
    }
    for (int i = 0; i < NCLOUD; i++) { g_clouds[i].x += g_clouds[i].speed * dt; if (g_clouds[i].x > 1000) g_clouds[i].x = 300; }
    for (int i = 0; i < g_nPeople; i++) {
        Person& p = g_people[i];
        if (p.idle) continue;
        p.z += p.dir * p.speed * dt;
        p.phase += dt * p.speed * 6.0f;
        p.x += sinf(g_time * 0.5f + p.id) * 0.05f * dt;
        if (p.dir < 0 && p.z < 4.8f)  { p.dir = 1;  p.x = 0.1f + rand01() * 2.1f; }     // turn around
        if (p.dir > 0 && p.z > 46.0f) { p.dir = -1; p.x = -2.2f + rand01() * 2.1f; }
    }
    for (int i = 0; i < NPET; i++) {
        Petal& p = g_petals[i];
        p.y -= p.vy * dt; p.x += (p.vx + sinf(g_time * 1.5f + p.phase) * 18.0f) * dt; p.rot += p.spin * dt;
        if (p.y < -10) { p.y = H + 10; p.x = rand01() * W; }
        if (p.x > W + 10) p.x = -10;
        if (p.x < -10) p.x = W + 10;
    }
    for (int i = 0; i < g_nStalls; i++) {
        if (!g_stalls[i].smoke) continue;
        g_emit[i] += dt;
        while (g_emit[i] > 0.14f) {
            g_emit[i] -= 0.14f;
            const Stall& s = g_stalls[i];
            spawnSmoke(i, s.side * 3.2f + s.side * 0.4f, 1.15f, s.z0 + 0.7f);
        }
    }
    for (int i = 0; i < MAXSM; i++) {
        Smoke& s = g_smoke[i];
        if (!s.alive) continue;
        s.age += dt; if (s.age >= s.life) { s.alive = false; continue; }
        s.y += s.vy * dt; s.x += (s.vx + sinf(s.age * 3 + i) * 0.04f) * dt;
    }
}

void festivalDraw() {
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    drawSky(); drawSunMoon(); drawStars(); drawClouds(); drawHills();
    drawGround();
    // warm glow where the street vanishes (crowd + many lanterns)
    glowEllipse(VPX, VPY + 25, 160, 70, C(1.0f, 0.65f, 0.3f), g_lamp * 0.9f);

    // collect everything that has a depth, sort far -> near, draw
    static Item items[256]; int n = 0;
    for (int i = 0; i < g_nHouses; i++) items[n++] = { g_houses[i].z1, 1, i };
    for (int i = 0; i < g_nStalls; i++) items[n++] = { g_stalls[i].z1, 0, i };
    for (int i = 0; i < g_nTrees;  i++) items[n++] = { g_trees[i].z,   2, i };
    for (int i = 0; i < g_nPeople; i++) items[n++] = { g_people[i].z,  3, i };
    for (int i = 0; i < NSTR;      i++) items[n++] = { STR_Z[i],       4, i };
    items[n++] = { 48.0f, 5, 0 };
    items[n++] = { 2.2f,  6, 0 };     // big blossoms in the foreground corners
    std::sort(items, items + n, farther);
    for (int i = 0; i < n; i++) {
        switch (items[i].kind) {
            case 0: drawStall(g_stalls[items[i].idx], items[i].idx); break;
            case 1: drawHouse(g_houses[items[i].idx]); break;
            case 2: drawTree(g_trees[items[i].idx]); break;
            case 3: { const Person& p = g_people[items[i].idx]; drawPerson(p, p.x, p.z); break; }
            case 4: drawLanternString(items[i].idx); break;
            case 5: drawPagoda(48.0f); break;
            case 6: drawCornerBlossoms(-1); drawCornerBlossoms(1); break;
        }
    }

    // warm tint while the sun is setting
    float sunset = sinf(g_night * PI);
    if (sunset > 0.01f) { setCol(C(1.0f, 0.5f, 0.2f), 0.16f * sunset); rect(0, 0, W, H); }
    drawPetals();
    drawVignette();
    drawModeButton();
    glLineWidth(1.0f);
}