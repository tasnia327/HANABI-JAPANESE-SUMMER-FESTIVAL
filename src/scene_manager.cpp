// scene_manager.cpp  -  HANABI : central scene controller (JOINT file)
#include "scene_manager.h"
#include "festival.h"
#include "kids_playzone.h"
#include "fireworks.h"
#include "audio.h"
#include <GL/glut.h>

#include <cstdlib>
#include <cstdio>

namespace {

// true  = the street starts in DAY mode; the round sun/moon button (or N) switches DAY <-> NIGHT.
//         From NIGHT, press N to continue:  Kids' Play Zone -> Fireworks -> Finale -> back to day.
// false = fully automatic timeline (all scenes play one after another and loop)
const bool MANUAL_NIGHT = true;

// set to true to print every mouse click (position) in the terminal - handy for testing
const bool DEBUG_CLICKS = false;

// Seconds a sunset / sunrise takes (manual mode)
const float TRANSITION = 7.0f;

// ----- automatic mode only -----
const Scene SEQUENCE[] = { SCENE_FESTIVAL_DAY, SCENE_DAY_TO_NIGHT, SCENE_FESTIVAL_NIGHT,
                           SCENE_KIDS_ZONE,    SCENE_FIREWORKS,    SCENE_FINALE };
const float DURATION[] = { 8.0f,               9.0f,               8.0f,
                           14.0f,              16.0f,              14.0f };
const int   SEQ_LENGTH = sizeof(SEQUENCE) / sizeof(SEQUENCE[0]);

int   g_index  = 0;
float g_timer  = 0.0f;
bool  g_paused = false;

// ----- manual mode -----
float g_p      = 0.0f;   // progress: 0 = day ... 1 = night
int   g_target = 0;      // 0 = going to / staying in day, 1 = going to / staying in night
int   g_stage  = 0;      // 0 = festival street, 1 = kids' zone, 2 = fireworks, 3 = finale

float smoothstep01(float t) {
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    return t * t * (3.0f - 2.0f * t);
}

void toScene(int wx, int wy, float& lx, float& ly) {     // window pixels -> festival coordinates
    float w = (float)glutGet(GLUT_WINDOW_WIDTH), h = (float)glutGet(GLUT_WINDOW_HEIGHT);
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    lx = wx * FESTIVAL_W / w;
    ly = FESTIVAL_H - wy * FESTIVAL_H / h;
}

bool isTasniaScene(Scene s) {
    return s == SCENE_KIDS_ZONE || s == SCENE_FIREWORKS || s == SCENE_FINALE;
}

// Tasnia's scenes are drawn in a 1000 x 600 world; scale it to the 1280 x 720 festival window.
void drawTasniaScene(Scene s) {
    glPushMatrix();
    glScalef(FESTIVAL_W / 1000.0f, FESTIVAL_H / 600.0f, 1.0f);
    if (s == SCENE_KIDS_ZONE)      drawKidsPlayzone();
    else if (s == SCENE_FIREWORKS) drawFireworks(false);
    else                           drawFireworks(true);
    glPopMatrix();
}

// start-of-scene setup for Tasnia's scenes
void startTasniaScene(Scene s) {
    if (s == SCENE_KIDS_ZONE) resetKidsPlayzone();
    if (s == SCENE_FIREWORKS) resetFireworks();   // the finale continues the same sky, so no reset there
}

Scene stageScene(int stage) {
    switch (stage) {
        case 1:  return SCENE_KIDS_ZONE;
        case 2:  return SCENE_FIREWORKS;
        case 3:  return SCENE_FINALE;
        default: return SCENE_FESTIVAL_DAY;
    }
}

void enterScene(int index) {
    g_index = index;
    g_timer = 0.0f;
    startTasniaScene(SEQUENCE[index]);
}

void applyNightValue() {
    switch (SEQUENCE[g_index]) {
        case SCENE_FESTIVAL_DAY:   festivalSetNight(0.0f); break;
        case SCENE_DAY_TO_NIGHT:   festivalSetNight(smoothstep01(g_timer / DURATION[g_index])); break;
        case SCENE_FESTIVAL_NIGHT: festivalSetNight(1.0f); break;
        default: break;
    }
}

void nextScene() {
    int next = g_index + 1;
    if (next >= SEQ_LENGTH) next = 0;
    enterScene(next);
    applyNightValue();
}

// manual mode: jump to a stage
void enterStage(int stage) {
    g_stage = stage;
    startTasniaScene(stageScene(stage));
}

void restartAll() {
    g_stage = 0;
    g_p = 0.0f; g_target = 0;
    enterScene(0);
    applyNightValue();
    if (MANUAL_NIGHT) festivalSetNight(0.0f);
    resetFireworks();
    resetKidsPlayzone();
    g_paused = false;
}

} // namespace

void sceneInit() {
    festivalInit();
    audioInit();                      // music + festival ambience
    enterScene(0);
    g_p = 0.0f; g_target = 0; g_stage = 0;
    applyNightValue();
}

void sceneToggleMode() {
    if (MANUAL_NIGHT && g_stage == 0) { g_target = 1 - g_target; festivalPressButton(); }
}

void sceneUpdate(float dt) {
    if (g_paused) return;

    if (MANUAL_NIGHT) {
        if (g_stage != 0) return;     // Tasnia's scenes animate themselves while drawing
        float step = dt / TRANSITION;
        if (g_target == 1) { g_p += step; if (g_p > 1.0f) g_p = 1.0f; }
        else               { g_p -= step; if (g_p < 0.0f) g_p = 0.0f; }
        festivalSetNight(0.5f * (g_p + smoothstep01(g_p)));
        festivalSetModeButton(true, g_target == 1);
        festivalUpdate(dt);
        return;
    }

    g_timer += dt;
    if (g_timer >= DURATION[g_index]) nextScene();
    applyNightValue();
    festivalSetModeButton(false, false);

    if (!isTasniaScene(SEQUENCE[g_index]))
        festivalUpdate(dt);
}

void sceneDraw() {
    if (MANUAL_NIGHT) {
        if (g_stage == 0) festivalDraw();
        else              drawTasniaScene(stageScene(g_stage));
        return;
    }
    Scene s = SEQUENCE[g_index];
    if (isTasniaScene(s)) drawTasniaScene(s);
    else                  festivalDraw();
}

void sceneMouse(int button, int state, int x, int y) {
    if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN) return;
    float lx, ly;
    toScene(x, y, lx, ly);
    if (DEBUG_CLICKS) printf("click at scene (%.0f, %.0f)\n", lx, ly);

    if (sceneCurrent() == SCENE_KIDS_ZONE) {         // click the three games
        kidsPlayzoneMouseClick(x, y, glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT));
        return;
    }
    if (MANUAL_NIGHT && g_stage == 0 && festivalModeButtonHit(lx, ly)) sceneToggleMode();
}

void sceneMouseMove(int x, int y) {
    float lx, ly;
    toScene(x, y, lx, ly);

    if (sceneCurrent() == SCENE_KIDS_ZONE) {
        kidsPlayzoneMouseMove(x, y, glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT));
        glutSetCursor(GLUT_CURSOR_LEFT_ARROW);
        return;
    }
    festivalSetMouse(lx, ly);
    bool overButton = MANUAL_NIGHT && g_stage == 0 && festivalModeButtonHit(lx, ly);
    glutSetCursor(overButton ? GLUT_CURSOR_INFO : GLUT_CURSOR_LEFT_ARROW);
}

void sceneKeyboard(unsigned char key) {
    switch (key) {
        case ' ':  g_paused = !g_paused; break;               // pause / resume (festival street)
        case 'r':
        case 'R':  restartAll(); break;                        // back to the day street
        case 'n':
        case 'N':
            if (!MANUAL_NIGHT) { nextScene(); break; }
            if (g_stage == 0) {
                if (g_target == 0) sceneToggleMode();          // day -> night
                else               enterStage(1);              // night -> kids' zone
            } else if (g_stage == 3) {
                restartAll();                                  // after the finale: start again
            } else {
                enterStage(g_stage + 1);                       // kids -> fireworks -> finale
            }
            break;
        case 'm':
        case 'M':  audioToggle(); break;                      // sound on / off
        case 27:   exit(0); break;                            // ESC
        default: break;
    }
}

Scene sceneCurrent() {
    if (MANUAL_NIGHT) {
        if (g_stage != 0) return stageScene(g_stage);
        if (g_p <= 0.0f && g_target == 0) return SCENE_FESTIVAL_DAY;
        if (g_p >= 1.0f && g_target == 1) return SCENE_FESTIVAL_NIGHT;
        return SCENE_DAY_TO_NIGHT;
    }
    return SEQUENCE[g_index];
}
bool scenePaused()  { return g_paused; }