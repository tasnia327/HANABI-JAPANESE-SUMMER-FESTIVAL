// scene_manager.h  -  HANABI : central scene controller (JOINT file)
// Nuzhat: festival street (day / transition / night)
// Tasnia: kids' play zone, fireworks, grand finale, audio
#ifndef SCENE_MANAGER_H
#define SCENE_MANAGER_H

enum Scene {
    SCENE_FESTIVAL_DAY = 0,     // Nuzhat
    SCENE_DAY_TO_NIGHT,         // Nuzhat
    SCENE_FESTIVAL_NIGHT,       // Nuzhat
    SCENE_KIDS_ZONE,            // Tasnia
    SCENE_FIREWORKS,            // Tasnia
    SCENE_FINALE,               // Tasnia
    SCENE_COUNT
};

void  sceneInit();                       // call once after window creation
void  sceneUpdate(float dt);             // call every frame
void  sceneDraw();                       // call from display()
void  sceneKeyboard(unsigned char key);  // Space, R, N, M
void  sceneMouse(int button, int state, int x, int y);   // NIGHT MODE button + kids' games
void  sceneMouseMove(int x, int y);                      // hover effect + goldfish scooper
void  sceneToggleMode();                 // day <-> night (sunset / sunrise)
Scene sceneCurrent();
bool  scenePaused();

#endif