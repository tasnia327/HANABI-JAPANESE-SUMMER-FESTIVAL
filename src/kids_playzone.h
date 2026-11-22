#ifndef KIDS_PLAYZONE_H
#define KIDS_PLAYZONE_H

// Kids' Play Zone  (world coordinates 0-1000 x 0-600)
//
// drawKidsPlayzone()  : call every frame while this scene is active.
//                       The three games play themselves automatically and
//                       the state resets by itself when the scene is re-entered.
//
// Optional mouse interaction (pass raw window pixel coordinates):
void drawKidsPlayzone();
void resetKidsPlayzone();                  // start the three games fresh
void kidsPlayzoneMouseMove(int mx, int my, int winW, int winH);
void kidsPlayzoneMouseClick(int mx, int my, int winW, int winH);

#endif