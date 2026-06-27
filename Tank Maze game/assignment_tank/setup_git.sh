#!/usr/bin/env bash
# ============================================================
#  setup_git.sh — Initialise the repo and make the first commit
#  Run this once from the project root:
#    cd "/home/hemish/Tank Maze game/assignment_tank"
#    bash setup_git.sh
#
#  Then, when you have your GitHub remote URL, run:
#    git remote add origin <YOUR_GITHUB_URL>
#    git push -u origin main
# ============================================================

set -e   # exit on first error

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
echo "==> Repository root: $REPO_ROOT"
cd "$REPO_ROOT"

# ── 1. Initialise repo ──────────────────────────────────────
if [ -d ".git" ]; then
  echo "[git] Repository already initialised — skipping init."
else
  git init -b main
  echo "[git] Initialised empty repository."
fi

# ── 2. Configure identity (edit these if needed) ───────────
git config user.email "you@example.com"
git config user.name  "Hemish"

# ── 3. Stage all project files ─────────────────────────────
echo "[git] Staging files..."

# Core source
git add README.md
git add .gitignore

# Common library
git add common/

# 3D models & textures
git add models/

# Application
git add tank_assignment/main.cpp
git add tank_assignment/Makefile
git add tank_assignment/qmake.pro
git add tank_assignment/level.txt
git add tank_assignment/shaders/

# ── 4. Show what will be committed ─────────────────────────
echo ""
echo "[git] Files staged for commit:"
git status --short
echo ""

# ── 5. Initial commit ──────────────────────────────────────
git commit -m "Initial commit: OpenGL 3D Tank Maze Game

Full clean-rebuild of a university computer graphics assignment.

Features implemented:
- Dynamic M×N maze grid parsed from level.txt
- OBJ mesh loading (chassis, turret, wheels, coin, ball)
- BMP texture binding with GL_LINEAR filtering
- Tank kinematics: V_new = V_prev + dt*(a_accel - a_decel)
- Wheel spin proportional to distance / wheel_radius
- Gravity fall-off-edge detection and game-over
- Spherical orbit camera controlled by passive mouse motion
- Turret heading tracks camera pan angle
- Arc-trajectory projectile with gravity (ball.obj)
- Bounding-sphere collision (tank+projectile vs tokens)
- GLSL Blinn-Phong lighting with correct normal matrix
- Token bob+spin animation with staggered phases
- Start menu and game-over overlay
- HUD: time countdown + token score
- GNU Makefile build (no qmake dependency)

Tech stack: C++11, OpenGL 3.3, GLSL 330, GLEW, freeglut"

echo ""
echo "==> Commit created successfully!"
echo ""
echo "── Next steps ──────────────────────────────────────────"
echo "  1. Create a new repository on GitHub (empty, no README)"
echo "  2. Run:"
echo "       git remote add origin https://github.com/<YOU>/<REPO>.git"
echo "       git push -u origin main"
echo "────────────────────────────────────────────────────────"
