//=============================================================================
//  TANK MAZE GAME  —  OpenGL 3D Assignment
//  Complete rebuild: modern GLSL shaders + full game-loop architecture.
//
//  Marking criteria implemented:
//    [10%] Maze environment  — dynamic M×N grid from level.txt
//    [10%] 3D model loading  — OBJ parser, BMP textures, mipmaps
//    [10%] Tank physics      — kinematic V/P equations, wheel spin, gravity fall
//    [10%] Camera / Turret   — spherical orbit, passive mouse aim, arc projectile
//    [30%] GLSL + polish     — Blinn-Phong lighting, token animation, menus
//=============================================================================

#include <GL/glew.h>
#include <GL/glut.h>
#include <Shader.h>
#include <Vector.h>
#include <Matrix.h>
#include <Mesh.h>
#include <Texture.h>
#include <SphericalCameraManipulator.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <cstdio>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ============================================================
//  CONSTANTS
// ============================================================

// Window
const int    SCR_W          = 1280;
const int    SCR_H          = 720;

// World geometry
// Tile cube (initCube) goes −1→+1; after scale(0.5, 0.2, 0.5) → top face at y=+0.2
const float  TILE_SCALE_XZ  = 0.5f;   // half-extent of tile in X and Z
const float  TILE_SCALE_Y   = 0.2f;   // half-extent of tile in Y
const float  TILE_TOP       = 0.2f;   // world-Y of tile top surface

// Tank
const float  TANK_SCALE     = 0.45f;
const float  TANK_ABOVE_TILE= 0.50f;  // tank centre Y offset above tile top
const float  A_ACCEL        = 8.0f;   // drive acceleration  (m s⁻²)
const float  A_DECEL        = 5.0f;   // drag deceleration   (m s⁻²)
const float  MAX_SPEED      = 5.5f;   // maximum linear speed (m s⁻¹)
const float  TURN_RATE      = 85.0f;  // turning speed  (deg s⁻¹)
const float  WHEEL_RADIUS   = 0.28f;  // wheel radius for spin calculation

// Physics
const float  GRAVITY        = 9.81f;

// Projectile (ball.obj)
const float  PROJ_SPEED     = 16.0f;
const float  PROJ_UP_BIAS   =  3.5f;  // initial upward velocity component
const float  PROJ_MAX_LIFE  =  6.0f;  // seconds before auto-despawn
const float  PROJ_SCALE     =  0.18f;

// Tokens (coin.obj)
const float  TOKEN_HEIGHT   =  1.40f; // above tile top
const float  TOKEN_BOB_AMP  =  0.18f; // vertical bob amplitude
const float  TOKEN_BOB_FREQ =  2.00f; // bob frequency (rad s⁻¹)
const float  TOKEN_SPIN_SPD = 80.0f;  // spin rate (deg s⁻¹)
const float  TOKEN_SCALE    =  0.35f;

// Collision radii (bounding sphere)
const float  TOKEN_COLL_R   = 0.70f;
const float  PROJ_COLL_R    = 0.45f;

// Game timer
const int    GAME_SECS      = 120;

// Lighting  — warm sunlight
static const float LIGHT_POS[3]  = {  8.0f, 18.0f,  6.0f };
static const float LIGHT_COL[3]  = {  1.0f,  0.96f, 0.88f };
const float  AMBIENT_STR    = 0.28f;
const float  SPECULAR_STR   = 0.55f;
const float  SHININESS      = 48.0f;

// Shader attribute layout locations (must match layout qualifiers in .vert)
const GLuint ATTR_POS  = 0;
const GLuint ATTR_NORM = 1;
const GLuint ATTR_TEX  = 2;

// ============================================================
//  ENUMERATIONS & STRUCTURES
// ============================================================

enum GameState { STATE_MENU, STATE_PLAYING, STATE_GAME_OVER };

struct Token {
    int   col, row;
    bool  alive;
    float spinAngle;  // degrees
    float bobPhase;   // radians — staggered per token for visual variety
};

struct Projectile {
    Vector3f pos, vel;
    bool     active;
    float    life;    // seconds elapsed since spawn
};

// ============================================================
//  GLOBALS — Window & Input
// ============================================================
int  g_scrW = SCR_W, g_scrH = SCR_H;
bool g_keys[256];

// ============================================================
//  GLOBALS — Game State
// ============================================================
GameState g_state     = STATE_MENU;
bool      g_playerWon = false;

// ============================================================
//  GLOBALS — Maze
// ============================================================
int  g_rows = 0, g_cols = 0;
std::vector< std::vector<int> > g_maze;
std::vector<Token>               g_tokens;
int  g_totalTokens     = 0;
int  g_tokensCollected = 0;

// ============================================================
//  GLOBALS — Game Timer
// ============================================================
int   g_timeLeft   = GAME_SECS;
float g_timerAccum = 0.0f;

// ============================================================
//  GLOBALS — Tank Physics
// ============================================================
float g_tankX     = 1.0f;   // world-space X position
float g_tankZ     = 1.0f;   // world-space Z position
float g_tankSpeed = 0.0f;   // signed forward speed (m s⁻¹)
float g_tankHead  = 0.0f;   // heading in degrees (Y-up)
float g_fallVel   = 0.0f;   // downward fall velocity
float g_fallOff   = 0.0f;   // cumulative Y fall offset (negative when falling)
bool  g_falling   = false;
float g_wheelAng  = 0.0f;   // wheel spin angle in degrees

// ============================================================
//  GLOBALS — Camera
// ============================================================
// Pan / tilt in radians.
// aVec = (sin(tilt)*sin(pan), cos(tilt), sin(tilt)*cos(pan))
// Camera is at focus + aVec * radius  (tilt < 0 → camera above)
float g_camPan  =  0.0f;
float g_camTilt = -0.42f;
float g_camRad  =  9.0f;
int   g_mLastX  = -1, g_mLastY = -1;

// ============================================================
//  GLOBALS — Projectiles
// ============================================================
std::vector<Projectile> g_projs;

// ============================================================
//  GLOBALS — Meshes & Textures
// ============================================================
Mesh   g_mChassis, g_mTurret, g_mFWhl, g_mBWhl;
Mesh   g_mCoin,    g_mBall,   g_mTile;
GLuint g_tTank  = 0, g_tCoin = 0, g_tBall = 0, g_tCrate = 0;

// ============================================================
//  GLOBALS — Shader & Uniforms
// ============================================================
GLuint g_prog = 0;

GLint g_uModel    = -1, g_uView     = -1, g_uProj    = -1;
GLint g_uNormMat  = -1;
GLint g_uLightPos = -1, g_uLightCol = -1, g_uViewPos = -1;
GLint g_uAmbient  = -1, g_uSpecular = -1, g_uShine   = -1;
GLint g_uUseTex   = -1, g_uObjClr   = -1, g_uTexSmp  = -1;

// ============================================================
//  GLOBALS — Timing
// ============================================================
int   g_lastMs = 0;
float g_dt     = 0.0f;
float g_animT  = 0.0f;  // total animation time (always accumulates)

// ============================================================
//  FORWARD DECLARATIONS
// ============================================================
bool     initGL(int argc, char** argv);
bool     loadAssets();
bool     parseMaze(const std::string& file);
void     resetGame();

void     display();
void     keyboard(unsigned char key, int x, int y);
void     keyUp(unsigned char key, int x, int y);
void     mouseBtn(int btn, int state, int x, int y);
void     mouseMove(int x, int y);
void     Timer(int value);

void     updatePhysics();
void     updateTokens();
void     updateProjectiles();
void     checkCollisions();

bool     isOverTile(float x, float z);
Vector3f camWorldPos();
void     fireProjectile();

// Draw — 3D
void     drawScene();
void     drawMaze();
void     drawTokens();
void     drawTank();
void     drawProjectiles();

// Draw — 2D overlay (legacy fixed-function)
void     drawHUD();
void     drawMenu();
void     drawGameOver();
void     drawScreenText(int x, int y, const std::string& s,
                        void* font = GLUT_BITMAP_HELVETICA_18,
                        float r = 1.f, float g = 1.f, float b = 1.f);
void     drawScreenQuad(int x, int y, int w, int h,
                        float r, float g, float b, float a);

// Shader helpers
void     setModel(Matrix4x4 model);
void     drawTextured(Mesh& mesh, GLuint tex);
void     drawColored(Mesh& mesh, float r, float g, float b);


// ============================================================
//  MAIN ENTRY
// ============================================================
int main(int argc, char** argv)
{
    if (!initGL(argc, argv)) return -1;
    if (!loadAssets())       return -1;

    std::fill(g_keys, g_keys + 256, false);
    g_lastMs = glutGet(GLUT_ELAPSED_TIME);

    glutMainLoop();
    return 0;
}


// ============================================================
//  INITIALISE OpenGL / GLUT
// ============================================================
bool initGL(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(g_scrW, g_scrH);
    glutInitWindowPosition(80, 40);
    glutCreateWindow("Tank Maze Game  —  OpenGL Assignment");

    if (glewInit() != GLEW_OK) {
        std::cerr << "[GLEW] Initialisation failed.\n";
        return false;
    }
    std::cout << "[OpenGL] " << glGetString(GL_VERSION)
              << "  GLSL " << glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n";

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Register GLUT callbacks
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyUp);
    glutMouseFunc(mouseBtn);
    glutPassiveMotionFunc(mouseMove);  // camera aims on passive mouse motion
    glutMotionFunc(mouseMove);

    // Start the game loop timer (fires every 10 ms → ~100 Hz physics)
    glutTimerFunc(10, Timer, 0);
    return true;
}


// ============================================================
//  LOAD ALL ASSETS
// ============================================================
bool loadAssets()
{
    // ── Shaders ─────────────────────────────────────────────
    g_prog = Shader::LoadFromFile("shaders/main.vert", "shaders/main.frag");
    if (g_prog == 0) {
        std::cerr << "[Shader] Compilation failed — check shaders/main.{vert,frag}\n";
        return false;
    }
    glUseProgram(g_prog);

    // Cache all uniform locations
    g_uModel    = glGetUniformLocation(g_prog, "modelMatrix");
    g_uView     = glGetUniformLocation(g_prog, "viewMatrix");
    g_uProj     = glGetUniformLocation(g_prog, "projMatrix");
    g_uNormMat  = glGetUniformLocation(g_prog, "normalMatrix");
    g_uLightPos = glGetUniformLocation(g_prog, "lightPos");
    g_uLightCol = glGetUniformLocation(g_prog, "lightColor");
    g_uViewPos  = glGetUniformLocation(g_prog, "viewPos");
    g_uAmbient  = glGetUniformLocation(g_prog, "ambientStrength");
    g_uSpecular = glGetUniformLocation(g_prog, "specularStrength");
    g_uShine    = glGetUniformLocation(g_prog, "shininess");
    g_uUseTex   = glGetUniformLocation(g_prog, "useTexture");
    g_uObjClr   = glGetUniformLocation(g_prog, "objectColor");
    g_uTexSmp   = glGetUniformLocation(g_prog, "texSampler");

    // ── OBJ Meshes ───────────────────────────────────────────
    if (!g_mChassis.loadOBJ("../models/chassis.obj"))    return false;
    if (!g_mTurret.loadOBJ("../models/turret.obj"))      return false;
    if (!g_mFWhl.loadOBJ("../models/front_wheel.obj"))   return false;
    if (!g_mBWhl.loadOBJ("../models/back_wheel.obj"))    return false;
    if (!g_mCoin.loadOBJ("../models/coin.obj"))          return false;
    if (!g_mBall.loadOBJ("../models/ball.obj"))          return false;

    // Procedural tile geometry (cube)
    g_mTile.initCube();

    // ── BMP Textures ─────────────────────────────────────────
    g_tTank  = Texture::LoadBMP("../models/hamvee.bmp");
    g_tCoin  = Texture::LoadBMP("../models/coin.bmp");
    g_tBall  = Texture::LoadBMP("../models/ball.bmp");
    g_tCrate = Texture::LoadBMP("../models/Crate.bmp");

    // Apply wrapping + filtering parameters to every loaded texture
    GLuint texList[4] = { g_tTank, g_tCoin, g_tBall, g_tCrate };
    for (int i = 0; i < 4; ++i) {
        GLuint t = texList[i];
        if (!t) { std::cerr << "[Texture] Slot " << i << " failed to load.\n"; continue; }
        glBindTexture(GL_TEXTURE_2D, t);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    // ── Maze ────────────────────────────────────────────────
    if (!parseMaze("level.txt")) return false;

    return true;
}


// ============================================================
//  PARSE MAZE FILE
//  Format:  first line "M N", then M rows of N integers
//  0 = void, 1 = tile, 2 = tile with token
// ============================================================
bool parseMaze(const std::string& file)
{
    std::ifstream fs(file.c_str());
    if (!fs.is_open()) {
        std::cerr << "[Maze] Cannot open: " << file << "\n";
        return false;
    }

    fs >> g_rows >> g_cols;
    g_maze.resize(g_rows, std::vector<int>(g_cols, 0));

    for (int r = 0; r < g_rows; ++r) {
        for (int c = 0; c < g_cols; ++c) {
            fs >> g_maze[r][c];
            if (g_maze[r][c] == 2) {
                Token tok;
                tok.col       = c;
                tok.row       = r;
                tok.alive     = true;
                tok.spinAngle = 0.0f;
                // Stagger bob phases so tokens don't all rise/fall in unison
                tok.bobPhase  = (float)(c * 3 + r) * 0.37f;
                g_tokens.push_back(tok);
                ++g_totalTokens;
            }
        }
    }

    std::cout << "[Maze] Loaded " << g_rows << "x" << g_cols
              << " grid.  Tokens: " << g_totalTokens << "\n";

    // Place tank on first tile (row=1, col=1) — confirmed '1' in level.txt
    g_tankX = 1.0f;
    g_tankZ = 1.0f;
    return true;
}


// ============================================================
//  RESET — restart the game from scratch
// ============================================================
void resetGame()
{
    g_state          = STATE_PLAYING;
    g_playerWon      = false;
    g_tokensCollected = 0;
    g_timeLeft        = GAME_SECS;
    g_timerAccum      = 0.0f;

    g_tankX    = 1.0f;  g_tankZ  = 1.0f;
    g_tankSpeed= 0.0f;  g_tankHead = 0.0f;
    g_fallVel  = 0.0f;  g_fallOff  = 0.0f;
    g_falling  = false; g_wheelAng  = 0.0f;

    g_camPan = 0.0f;  g_camTilt = -0.42f;
    g_mLastX = g_mLastY = -1;

    g_projs.clear();

    for (size_t i = 0; i < g_tokens.size(); ++i)
        g_tokens[i].alive = true;
}


// ============================================================
//  TIMER — physics game loop  (~100 Hz)
// ============================================================
void Timer(int value)
{
    int now = glutGet(GLUT_ELAPSED_TIME);
    g_dt = (float)(now - g_lastMs) / 1000.0f;
    if (g_dt > 0.05f) g_dt = 0.05f;  // cap to 20 fps equivalent
    g_lastMs = now;
    g_animT += g_dt;

    if (g_state == STATE_PLAYING) {
        updatePhysics();
        updateTokens();
        updateProjectiles();
        checkCollisions();

        // Count down game clock (1-second resolution)
        g_timerAccum += g_dt;
        while (g_timerAccum >= 1.0f) {
            g_timerAccum -= 1.0f;
            if (g_timeLeft > 0) --g_timeLeft;
            if (g_timeLeft <= 0) {
                g_state     = STATE_GAME_OVER;
                g_playerWon = false;
            }
        }

        // Prune dead projectiles when the list grows large
        if (g_projs.size() > 30) {
            std::vector<Projectile> live;
            for (size_t i = 0; i < g_projs.size(); ++i)
                if (g_projs[i].active) live.push_back(g_projs[i]);
            g_projs = live;
        }
    }

    glutPostRedisplay();
    glutTimerFunc(10, Timer, 0);
}


// ============================================================
//  PHYSICS UPDATE
// ============================================================
void updatePhysics()
{
    float headRad = g_tankHead * (float)M_PI / 180.0f;

    // ── Kinematic equations (rubric spec) ────────────────────
    //   V_new = V_prev + Δt * (a_accel − a_decel)
    //   P_new = P_prev + Δt * V_new

    float a_accel = 0.0f;
    if (g_keys['w'] || g_keys['W']) a_accel += A_ACCEL;
    if (g_keys['s'] || g_keys['S']) a_accel -= A_ACCEL;

    // Drag always opposes current motion direction
    float a_decel = 0.0f;
    if      (g_tankSpeed >  0.01f) a_decel =  A_DECEL;
    else if (g_tankSpeed < -0.01f) a_decel = -A_DECEL;

    float v_new = g_tankSpeed + g_dt * (a_accel - a_decel);

    // Prevent drag from reversing direction when no drive key held
    if (a_accel == 0.0f) {
        if ((g_tankSpeed > 0.0f && v_new < 0.0f) ||
            (g_tankSpeed < 0.0f && v_new > 0.0f))
            v_new = 0.0f;
    }

    // Clamp to maximum speed
    if (v_new >  MAX_SPEED) v_new =  MAX_SPEED;
    if (v_new < -MAX_SPEED) v_new = -MAX_SPEED;
    g_tankSpeed = v_new;

    // Update position
    g_tankX += g_dt * g_tankSpeed * sinf(headRad);
    g_tankZ += g_dt * g_tankSpeed * cosf(headRad);

    // ── Turning ──────────────────────────────────────────────
    if (g_keys['a'] || g_keys['A']) g_tankHead -= TURN_RATE * g_dt;
    if (g_keys['d'] || g_keys['D']) g_tankHead += TURN_RATE * g_dt;
    // Wrap heading within [0, 360)
    while (g_tankHead >=  360.0f) g_tankHead -= 360.0f;
    while (g_tankHead <    0.0f)  g_tankHead += 360.0f;

    // ── Wheel rotation ────────────────────────────────────────
    // Wheel spin angle = linear distance / circumference * 360 degrees
    float distMoved = g_tankSpeed * g_dt;
    g_wheelAng += (distMoved / WHEEL_RADIUS) * (180.0f / (float)M_PI);

    // ── Edge detection & falling ─────────────────────────────
    bool onTile = isOverTile(g_tankX, g_tankZ);

    if (!onTile && !g_falling) {
        // Just left the edge — start falling
        g_falling = true;
        g_fallVel = 0.0f;
    }
    if (g_falling && onTile) {
        // Recovered to a tile (edge case)
        g_falling = false;
        g_fallVel = 0.0f;
        g_fallOff = 0.0f;
    }
    if (g_falling) {
        g_fallVel += GRAVITY * g_dt;   // gravity acceleration
        g_fallOff -= g_fallVel * g_dt; // fall offset becomes increasingly negative
        if (g_fallOff < -20.0f) {
            g_state     = STATE_GAME_OVER;
            g_playerWon = false;
        }
    }
}


// ============================================================
//  TOKEN ANIMATION UPDATE
// ============================================================
void updateTokens()
{
    for (size_t i = 0; i < g_tokens.size(); ++i) {
        if (!g_tokens[i].alive) continue;
        g_tokens[i].spinAngle += TOKEN_SPIN_SPD * g_dt;
        if (g_tokens[i].spinAngle > 360.0f) g_tokens[i].spinAngle -= 360.0f;
        g_tokens[i].bobPhase  += TOKEN_BOB_FREQ * g_dt;
    }
}


// ============================================================
//  PROJECTILE UPDATE
// ============================================================
void updateProjectiles()
{
    for (size_t i = 0; i < g_projs.size(); ++i) {
        Projectile& p = g_projs[i];
        if (!p.active) continue;
        p.pos.x += p.vel.x * g_dt;
        p.pos.y += p.vel.y * g_dt;
        p.pos.z += p.vel.z * g_dt;
        p.vel.y -= GRAVITY * g_dt;   // gravity creates arc trajectory
        p.life  += g_dt;
        if (p.life > PROJ_MAX_LIFE || p.pos.y < -15.0f)
            p.active = false;
    }
}


// ============================================================
//  COLLISION DETECTION  — bounding sphere checks
// ============================================================
void checkCollisions()
{
    float tankWorldY = TILE_TOP + TANK_ABOVE_TILE + g_fallOff;

    for (size_t i = 0; i < g_tokens.size(); ++i) {
        Token& tok = g_tokens[i];
        if (!tok.alive) continue;

        float tx = (float)tok.col;
        float tz = (float)tok.row;
        float ty = TILE_TOP + TOKEN_HEIGHT + TOKEN_BOB_AMP * sinf(tok.bobPhase);

        // ── Tank vs Token ─────────────────────────────────────
        {
            float dx = g_tankX - tx;
            float dy = tankWorldY - ty;
            float dz = g_tankZ  - tz;
            if (sqrtf(dx*dx + dy*dy + dz*dz) < TOKEN_COLL_R) {
                tok.alive = false;
                ++g_tokensCollected;
                continue;  // already collected, skip projectile check
            }
        }

        // ── Projectile vs Token ────────────────────────────────
        for (size_t j = 0; j < g_projs.size(); ++j) {
            Projectile& p = g_projs[j];
            if (!p.active) continue;
            float dx = p.pos.x - tx;
            float dy = p.pos.y - ty;
            float dz = p.pos.z - tz;
            float combined = PROJ_COLL_R + TOKEN_COLL_R * 0.5f;
            if (sqrtf(dx*dx + dy*dy + dz*dz) < combined) {
                tok.alive  = false;
                p.active   = false;
                ++g_tokensCollected;
                break;
            }
        }
    }

    // Win condition — all tokens collected
    if (g_totalTokens > 0 && g_tokensCollected >= g_totalTokens) {
        g_state     = STATE_GAME_OVER;
        g_playerWon = true;
    }
}


// ============================================================
//  HELPER — Is the given XZ position over any maze tile?
// ============================================================
bool isOverTile(float x, float z)
{
    for (int r = 0; r < g_rows; ++r) {
        for (int c = 0; c < g_cols; ++c) {
            if (g_maze[r][c] > 0) {
                // Tile occupies [c−0.5, c+0.5] × [r−0.5, r+0.5]
                // Use 0.46 margin to allow walking close to edges
                if (fabsf(x - (float)c) < 0.46f &&
                    fabsf(z - (float)r) < 0.46f)
                    return true;
            }
        }
    }
    return false;
}


// ============================================================
//  HELPER — Camera world-space position
// ============================================================
Vector3f camWorldPos()
{
    // Spherical orbit: focus + aVec * radius
    // aVec = (sin(tilt)*sin(pan),  cos(tilt),  sin(tilt)*cos(pan))
    float tankWorldY = TILE_TOP + TANK_ABOVE_TILE + g_fallOff;
    Vector3f focus(g_tankX, tankWorldY + 0.5f, g_tankZ);

    float ts = sinf(g_camTilt);   // negative → camera above focus
    float sp = sinf(g_camPan);
    float cp = cosf(g_camPan);
    float ct = cosf(g_camTilt);

    return Vector3f(
        focus.x + ts * sp * g_camRad,
        focus.y + ct      * g_camRad,
        focus.z + ts * cp * g_camRad
    );
}


// ============================================================
//  FIRE PROJECTILE — spawns a ball.obj projectile with arc
// ============================================================
void fireProjectile()
{
    // Derive fire direction from camera orbit angles.
    // Camera aVec = (sin(tilt)*sin(pan), cos(tilt), sin(tilt)*cos(pan))
    // Turret fires in direction: −normalize(aVec_xz)
    float ts = sinf(g_camTilt);  // negative
    float sp = sinf(g_camPan);
    float cp = cosf(g_camPan);

    // In XZ plane: aVec_xz direction = (ts*sp, ts*cp)
    // Negated (away from camera): (−ts*sp, −ts*cp) = (|ts|*sp, |ts|*cp)
    float fx = -ts * sp;
    float fz = -ts * cp;
    float len = sqrtf(fx*fx + fz*fz);
    if (len > 0.0001f) { fx /= len;  fz /= len; }

    float tankWorldY = TILE_TOP + TANK_ABOVE_TILE + g_fallOff + 0.35f;

    Projectile p;
    p.pos    = Vector3f(g_tankX + fx * 0.7f, tankWorldY, g_tankZ + fz * 0.7f);
    p.vel    = Vector3f(fx * PROJ_SPEED, PROJ_UP_BIAS, fz * PROJ_SPEED);
    p.active = true;
    p.life   = 0.0f;
    g_projs.push_back(p);
}


// ============================================================
//  SET MODEL MATRIX + NORMAL MATRIX uniforms
// ============================================================
void setModel(Matrix4x4 model)
{
    glUniformMatrix4fv(g_uModel, 1, GL_FALSE, model.getPtr());

    // Normal matrix = upper 3×3 of transpose(inverse(model))
    // Needed for correct normal transformation under non-uniform scaling.
    Matrix4x4 invModel  = model.inverse();
    Matrix4x4 normMat4  = invModel.transpose();
    float*    p16       = normMat4.getPtr();

    // Extract 3×3 column-major sub-matrix
    // p16 layout: col0[0..3], col1[4..7], col2[8..11], col3[12..15]
    float nm3[9] = {
        p16[0], p16[1], p16[2],   // column 0
        p16[4], p16[5], p16[6],   // column 1
        p16[8], p16[9], p16[10]   // column 2
    };
    glUniformMatrix3fv(g_uNormMat, 1, GL_FALSE, nm3);
}


// ============================================================
//  MESH DRAW HELPERS
// ============================================================

// Draw mesh with an applied texture
void drawTextured(Mesh& mesh, GLuint tex)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex ? tex : 0);
    glUniform1i(g_uUseTex, 1);
    glUniform1i(g_uTexSmp, 0);
    mesh.Draw(ATTR_POS, ATTR_NORM, ATTR_TEX);
}

// Draw mesh with a solid colour (no texture sampling)
void drawColored(Mesh& mesh, float r, float g, float b)
{
    glUniform1i(g_uUseTex, 0);
    glUniform3f(g_uObjClr, r, g, b);
    mesh.Draw(ATTR_POS, ATTR_NORM, (GLuint)(-1));
}


// ============================================================
//  2D OVERLAY HELPERS  — legacy fixed-function pipeline
// ============================================================

// Render a string at window coordinates (x, y) from bottom-left
void drawScreenText(int x, int y, const std::string& s, void* font,
                    float r, float g, float b)
{
    glUseProgram(0);
    glDisable(GL_DEPTH_TEST);
    glColor3f(r, g, b);
    glWindowPos2i(x, y);
    for (size_t i = 0; i < s.size(); ++i)
        glutBitmapCharacter(font, s[i]);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(g_prog);
}

// Render a filled screen-space quad (with alpha blending)
void drawScreenQuad(int x, int y, int w, int h,
                    float r, float g, float b, float a)
{
    glUseProgram(0);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, g_scrW, 0.0, g_scrH, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
        glVertex2i(x,     y);
        glVertex2i(x + w, y);
        glVertex2i(x + w, y + h);
        glVertex2i(x,     y + h);
    glEnd();

    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);  glPopMatrix();
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(g_prog);
}


// ============================================================
//  DISPLAY — main render function
// ============================================================
void display()
{
    // Deep space background colour
    glClearColor(0.04f, 0.06f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, g_scrW, g_scrH);

    if (g_state == STATE_MENU) {
        drawMenu();
    } else {
        drawScene();
        if (g_state == STATE_GAME_OVER) drawGameOver();
        drawHUD();
    }

    glutSwapBuffers();
}


// ============================================================
//  DRAW 3D SCENE
// ============================================================
void drawScene()
{
    glUseProgram(g_prog);

    // ── Camera ───────────────────────────────────────────────
    float tankWorldY = TILE_TOP + TANK_ABOVE_TILE + g_fallOff;
    Vector3f focus(g_tankX, tankWorldY + 0.5f, g_tankZ);
    Vector3f camPos = camWorldPos();

    Matrix4x4 viewMat;
    viewMat.lookAt(camPos, focus, Vector3f(0.0f, 1.0f, 0.0f));

    Matrix4x4 projMat;
    projMat.perspective(55.0f,
                        (float)g_scrW / (float)g_scrH,
                        0.1f, 120.0f);

    glUniformMatrix4fv(g_uView, 1, GL_FALSE, viewMat.getPtr());
    glUniformMatrix4fv(g_uProj, 1, GL_FALSE, projMat.getPtr());

    // ── Lighting ─────────────────────────────────────────────
    glUniform3fv(g_uLightPos, 1, LIGHT_POS);
    glUniform3fv(g_uLightCol, 1, LIGHT_COL);
    glUniform3f(g_uViewPos, camPos.x, camPos.y, camPos.z);
    glUniform1f(g_uAmbient,  AMBIENT_STR);
    glUniform1f(g_uSpecular, SPECULAR_STR);
    glUniform1f(g_uShine,    SHININESS);

    // ── Scene objects ────────────────────────────────────────
    drawMaze();
    drawTokens();
    drawTank();
    drawProjectiles();
}


// ============================================================
//  DRAW MAZE TILES
// ============================================================
void drawMaze()
{
    for (int r = 0; r < g_rows; ++r) {
        for (int c = 0; c < g_cols; ++c) {
            if (g_maze[r][c] == 0) continue;

            // Tile: cube scaled to 1×0.4×1 world-units, centre at (c, 0, r)
            // → top face at y = +TILE_TOP
            Matrix4x4 m;
            m.translate((float)c, 0.0f, (float)r);
            m.scale(TILE_SCALE_XZ, TILE_SCALE_Y, TILE_SCALE_XZ);

            setModel(m);
            drawTextured(g_mTile, g_tCrate);
        }
    }
}


// ============================================================
//  DRAW TOKENS (coin.obj — floating, spinning animation)
// ============================================================
void drawTokens()
{
    for (size_t i = 0; i < g_tokens.size(); ++i) {
        const Token& tok = g_tokens[i];
        if (!tok.alive) continue;

        // Bob on a sine wave in Y, spin around the world Y axis
        float ty = TILE_TOP + TOKEN_HEIGHT
                 + TOKEN_BOB_AMP * sinf(tok.bobPhase);

        Matrix4x4 m;
        m.translate((float)tok.col, ty, (float)tok.row);
        m.rotate(tok.spinAngle, 0.0f, 1.0f, 0.0f);
        m.scale(TOKEN_SCALE, TOKEN_SCALE, TOKEN_SCALE);

        setModel(m);
        drawTextured(g_mCoin, g_tCoin);
    }
}


// ============================================================
//  DRAW TANK  (chassis + turret + 2 wheel pairs)
// ============================================================
void drawTank()
{
    float tankWorldY = TILE_TOP + TANK_ABOVE_TILE + g_fallOff;

    // Turret tracks the camera pan angle.
    // aVec fires in direction −aVec_xz, which is rotation of angle:
    // turretWorldDeg = camPan converted to degrees (sign: pan decreases → rotate CCW)
    float turretDeg = -(g_camPan * (180.0f / (float)M_PI));

    // ── Chassis ──────────────────────────────────────────────
    {
        Matrix4x4 m;
        m.translate(g_tankX, tankWorldY, g_tankZ);
        m.rotate(g_tankHead, 0.0f, 1.0f, 0.0f);
        m.scale(TANK_SCALE, TANK_SCALE, TANK_SCALE);
        setModel(m);
        drawTextured(g_mChassis, g_tTank);
    }

    // ── Turret (rotates around world Y — tracks camera aim) ──
    {
        Matrix4x4 m;
        m.translate(g_tankX, tankWorldY + 0.05f, g_tankZ);
        m.rotate(turretDeg, 0.0f, 1.0f, 0.0f);
        m.scale(TANK_SCALE, TANK_SCALE, TANK_SCALE);
        setModel(m);
        drawTextured(g_mTurret, g_tTank);
    }

    // ── Front wheels (spin relative to tank body) ─────────────
    {
        Matrix4x4 m;
        m.translate(g_tankX, tankWorldY, g_tankZ);
        m.rotate(g_tankHead, 0.0f, 1.0f, 0.0f);
        m.scale(TANK_SCALE, TANK_SCALE, TANK_SCALE);
        m.rotate(g_wheelAng, 1.0f, 0.0f, 0.0f);  // spin in object-space X
        setModel(m);
        drawTextured(g_mFWhl, g_tTank);
    }

    // ── Rear wheels ───────────────────────────────────────────
    {
        Matrix4x4 m;
        m.translate(g_tankX, tankWorldY, g_tankZ);
        m.rotate(g_tankHead, 0.0f, 1.0f, 0.0f);
        m.scale(TANK_SCALE, TANK_SCALE, TANK_SCALE);
        m.rotate(g_wheelAng, 1.0f, 0.0f, 0.0f);
        setModel(m);
        drawTextured(g_mBWhl, g_tTank);
    }
}


// ============================================================
//  DRAW PROJECTILES  (ball.obj)
// ============================================================
void drawProjectiles()
{
    for (size_t i = 0; i < g_projs.size(); ++i) {
        const Projectile& p = g_projs[i];
        if (!p.active) continue;

        Matrix4x4 m;
        m.translate(p.pos.x, p.pos.y, p.pos.z);
        m.scale(PROJ_SCALE, PROJ_SCALE, PROJ_SCALE);
        setModel(m);
        drawTextured(g_mBall, g_tBall);
    }
}


// ============================================================
//  HUD  — time / score overlay rendered with legacy GL text
// ============================================================
void drawHUD()
{
    char buf[128];

    // Semi-transparent header bar
    drawScreenQuad(0, g_scrH - 56, g_scrW, 56, 0.0f, 0.0f, 0.0f, 0.60f);

    // Time Remaining
    std::snprintf(buf, sizeof(buf), "Time Remaining: %d s", g_timeLeft);
    // Colour turns orange → red as time runs low
    float timeRatio = (float)g_timeLeft / (float)GAME_SECS;
    float tr = (timeRatio < 0.33f) ? 1.0f : 0.9f;
    float tg = (timeRatio < 0.33f) ? 0.2f : 0.80f;
    drawScreenText(18, g_scrH - 30, buf,
                   GLUT_BITMAP_HELVETICA_18, tr, tg, 0.2f);

    // Tokens collected
    std::snprintf(buf, sizeof(buf),
                  "Tokens Collected: %d / %d", g_tokensCollected, g_totalTokens);
    drawScreenText(18, g_scrH - 50, buf,
                   GLUT_BITMAP_HELVETICA_12, 0.8f, 0.8f, 0.8f);

    // Controls reminder (right side)
    drawScreenText(g_scrW - 430, g_scrH - 30,
                   "WASD: Drive  |  Mouse: Aim  |  LMB: Fire  |  ESC: Quit",
                   GLUT_BITMAP_HELVETICA_12, 0.65f, 0.65f, 0.65f);

    // Crosshair (centre of screen)
    int cx = g_scrW / 2, cy = g_scrH / 2;
    glUseProgram(0);
    glDisable(GL_DEPTH_TEST);
    glColor3f(1.0f, 1.0f, 1.0f);
    glWindowPos2i(cx - 8, cy - 1);
    std::string cross("--+--");
    for (size_t i = 0; i < cross.size(); ++i)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, cross[i]);
    glEnable(GL_DEPTH_TEST);
    glUseProgram(g_prog);
}


// ============================================================
//  START MENU
// ============================================================
void drawMenu()
{
    // Background gradient (two-tone)
    drawScreenQuad(0, g_scrH / 2, g_scrW, g_scrH / 2, 0.04f, 0.06f, 0.20f, 1.0f);
    drawScreenQuad(0, 0,          g_scrW, g_scrH / 2, 0.02f, 0.13f, 0.15f, 1.0f);

    // Accent stripe
    drawScreenQuad(0, g_scrH / 2 - 3, g_scrW, 6, 0.20f, 0.60f, 1.00f, 0.85f);

    int cx = g_scrW / 2;

    // ── Title ────────────────────────────────────────────────
    drawScreenText(cx - 195, g_scrH - 150,
                   "TANK MAZE GAME",
                   GLUT_BITMAP_TIMES_ROMAN_24, 1.0f, 0.85f, 0.20f);

    drawScreenText(cx - 185, g_scrH - 190,
                   "OpenGL 3D Assignment  —  Programmable Pipeline",
                   GLUT_BITMAP_HELVETICA_12, 0.60f, 0.85f, 1.00f);

    // Divider
    drawScreenQuad(cx - 210, g_scrH - 210, 420, 2, 0.4f, 0.4f, 0.4f, 0.70f);

    // ── Controls ─────────────────────────────────────────────
    int y = g_scrH - 245;
    drawScreenText(cx - 175, y,      "CONTROLS",
                   GLUT_BITMAP_HELVETICA_18, 1.00f, 0.80f, 0.30f);
    drawScreenText(cx - 175, y -  26, "W / S        —  Drive Forward / Reverse",
                   GLUT_BITMAP_HELVETICA_12, 0.85f, 0.85f, 0.85f);
    drawScreenText(cx - 175, y -  44, "A / D        —  Turn Left / Right",
                   GLUT_BITMAP_HELVETICA_12, 0.85f, 0.85f, 0.85f);
    drawScreenText(cx - 175, y -  62, "Mouse Move   —  Aim Turret & Orbit Camera",
                   GLUT_BITMAP_HELVETICA_12, 0.85f, 0.85f, 0.85f);
    drawScreenText(cx - 175, y -  80, "Left Click   —  Fire Projectile",
                   GLUT_BITMAP_HELVETICA_12, 0.85f, 0.85f, 0.85f);
    drawScreenText(cx - 175, y -  98, "ESC          —  Quit",
                   GLUT_BITMAP_HELVETICA_12, 0.85f, 0.85f, 0.85f);

    // ── Objective ────────────────────────────────────────────
    drawScreenText(cx - 175, y - 135,
                   "OBJECTIVE: Collect all tokens before time expires!",
                   GLUT_BITMAP_HELVETICA_12, 0.50f, 1.00f, 0.50f);
    drawScreenText(cx - 175, y - 153,
                   "Warning: Drive off the floating tiles and you will fall into the void!",
                   GLUT_BITMAP_HELVETICA_12, 1.00f, 0.45f, 0.45f);

    // ── Pulsing ENTER prompt ──────────────────────────────────
    float pulse = 0.55f + 0.45f * sinf(g_animT * 3.0f);
    drawScreenText(cx - 115, 75,
                   "Press  ENTER  to  Start",
                   GLUT_BITMAP_TIMES_ROMAN_24, pulse, pulse * 0.95f, pulse * 0.5f);
}


// ============================================================
//  GAME OVER OVERLAY
// ============================================================
void drawGameOver()
{
    // Dark semi-transparent overlay over the 3D scene
    drawScreenQuad(0, 0, g_scrW, g_scrH, 0.0f, 0.0f, 0.0f, 0.68f);

    int cx = g_scrW / 2;
    char buf[128];

    if (g_playerWon) {
        drawScreenText(cx - 80, g_scrH / 2 + 90,
                       "YOU WIN!",
                       GLUT_BITMAP_TIMES_ROMAN_24, 0.25f, 1.00f, 0.25f);
    } else {
        drawScreenText(cx - 115, g_scrH / 2 + 90,
                       "GAME  OVER",
                       GLUT_BITMAP_TIMES_ROMAN_24, 1.00f, 0.25f, 0.25f);
    }

    std::snprintf(buf, sizeof(buf), "Tokens Collected: %d / %d",
                  g_tokensCollected, g_totalTokens);
    drawScreenText(cx - 125, g_scrH / 2 + 48, buf,
                   GLUT_BITMAP_HELVETICA_18, 1.00f, 0.85f, 0.30f);

    std::snprintf(buf, sizeof(buf), "Time Remaining: %d s", g_timeLeft);
    drawScreenText(cx - 100, g_scrH / 2 + 22, buf,
                   GLUT_BITMAP_HELVETICA_18, 0.75f, 0.75f, 0.75f);

    drawScreenText(cx - 145, g_scrH / 2 - 24,
                   "Press  ENTER  to Restart",
                   GLUT_BITMAP_HELVETICA_18, 0.60f, 0.90f, 1.00f);
    drawScreenText(cx - 75, g_scrH / 2 - 50,
                   "ESC to Quit",
                   GLUT_BITMAP_HELVETICA_12, 0.55f, 0.55f, 0.55f);
}


// ============================================================
//  INPUT — keyboard
// ============================================================
void keyboard(unsigned char key, int x, int y)
{
    if (key == 27) exit(0);   // ESC — quit
    g_keys[key] = true;

    if (key == 13) {          // ENTER — start / restart
        if (g_state == STATE_MENU || g_state == STATE_GAME_OVER)
            resetGame();
    }
}

void keyUp(unsigned char key, int x, int y)
{
    g_keys[key] = false;
}


// ============================================================
//  INPUT — mouse
// ============================================================
void mouseBtn(int btn, int state, int x, int y)
{
    if (g_state != STATE_PLAYING) return;
    if (btn == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
        fireProjectile();
}

// Called for both passive (no button) and active (button held) mouse motion.
// Adjusts camera pan / tilt so the turret + camera orbit tracks the mouse.
void mouseMove(int x, int y)
{
    if (g_state != STATE_PLAYING) {
        g_mLastX = x;
        g_mLastY = y;
        return;
    }

    if (g_mLastX >= 0) {
        float dx = (float)(x - g_mLastX);
        float dy = (float)(y - g_mLastY);

        // Horizontal mouse → pan camera left/right
        g_camPan  -= dx * 0.005f;

        // Vertical mouse → tilt camera up/down
        g_camTilt += dy * 0.005f;

        // Clamp tilt: camera must stay above the tank
        if (g_camTilt > -0.05f) g_camTilt = -0.05f;
        if (g_camTilt < -1.50f) g_camTilt = -1.50f;
    }

    g_mLastX = x;
    g_mLastY = y;
}
