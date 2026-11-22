#ifndef FIREWORKS_H
#define FIREWORKS_H

// Fireworks scene  (world coordinates 0-1000 x 0-600)
//
// drawFireworks(false) : Fireworks Celebration scene
// drawFireworks(true)  : Grand Finale (barrage + closing message).
//                        Switching from false to true keeps the sky alive,
//                        so the finale continues smoothly from the celebration.
//
// The scene resets automatically when it has not been drawn for a few seconds
// (scene re-entry / restart). resetFireworks() forces a reset.
void drawFireworks(bool finale = false);
void resetFireworks();

#endif