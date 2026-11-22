// main.cpp  -  HANABI : Japanese Summer Festival (JOINT file)
// Creates the window, sets the 2D coordinate system and forwards events
// to the scene manager.
#include <GL/glut.h>
#include "scene_manager.h"
#include "festival.h"      // for FESTIVAL_W / FESTIVAL_H (1280 x 720)

static int g_lastTime = 0;

static void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    sceneDraw();
    glutSwapBuffers();
}

static void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, FESTIVAL_W, 0, FESTIVAL_H, -1, 1);
    glMatrixMode(GL_MODELVIEW);
}

static void keyboard(unsigned char key, int, int) {
    sceneKeyboard(key);
}

static void mouse(int button, int state, int x, int y) {
    sceneMouse(button, state, x, y);          // click on the NIGHT MODE button
}

static void motion(int x, int y) {
    sceneMouseMove(x, y);                     // hover effect
}

static void timer(int) {
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = (now - g_lastTime) / 1000.0f;
    g_lastTime = now;
    sceneUpdate(dt);
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);            // ~60 FPS
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(1280, 720);
    glutCreateWindow("HANABI - Japanese Summer Festival");

    glClearColor(0, 0, 0, 1);
    sceneInit();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouse);
    glutPassiveMotionFunc(motion);
    glutMotionFunc(motion);

    g_lastTime = glutGet(GLUT_ELAPSED_TIME);
    glutTimerFunc(16, timer, 0);
    glutMainLoop();
    return 0;
}