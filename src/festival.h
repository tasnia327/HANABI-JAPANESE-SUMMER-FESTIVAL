// festival.h  -  HANABI : Festival Street module
// Coordinate system expected by this module: glOrtho(0, 1280, 0, 720, -1, 1)
#ifndef FESTIVAL_MODULE_H
#define FESTIVAL_MODULE_H

#define FESTIVAL_W 1280.0f
#define FESTIVAL_H 720.0f

// Call once after the OpenGL window is created.
void festivalInit();

// Call every frame. dt = seconds since last frame.
void festivalUpdate(float dt);

// Call from your display function (after glClear).
void festivalDraw();

// 0.0 = full day, 0.5 = sunset, 1.0 = full night.
// The scene manager animates this value for the day-to-night transition.
void festivalSetNight(float n);
float festivalGetNight();

// Round shirt-style button (bottom-right). It shows the mode a click switches TO:
// a MOON while it is day (click -> night) and a SUN while it is night (click -> day).
// Drawn by the festival module, clicked via the scene manager.
void festivalSetModeButton(bool show, bool showSun);   // show / hide, and which icon
void festivalPressButton();                            // click animation
bool festivalModeButtonHit(float x, float y);     // x,y in scene coordinates (0..1280, 0..720)
void festivalSetMouse(float x, float y);          // for the hover effect

#endif