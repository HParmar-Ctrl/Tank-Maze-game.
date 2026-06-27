TEMPLATE = app

# Executable name
TARGET   = TankAssignment
CONFIG   = debug
CONFIG  += c++11

# Output directories
DESTDIR     = ./
OBJECTS_DIR = ./build/

# ── Headers ──────────────────────────────────────────────────
HEADERS +=  ../common/Shader.h                    \
            ../common/Vector.h                    \
            ../common/Matrix.h                    \
            ../common/Mesh.h                      \
            ../common/Texture.h                   \
            ../common/SphericalCameraManipulator.h \

# ── Sources ──────────────────────────────────────────────────
SOURCES +=  main.cpp                                      \
            ../common/Shader.cpp                          \
            ../common/Vector.cpp                          \
            ../common/Matrix.cpp                          \
            ../common/Mesh.cpp                            \
            ../common/Texture.cpp                         \
            ../common/SphericalCameraManipulator.cpp      \

# ── Include paths ─────────────────────────────────────────────
INCLUDEPATH +=  ./            \
                ../common/    \

# ── Libraries ────────────────────────────────────────────────
LIBS +=     -lGLEW    \
            -lglut    \
            -lGL      \

# ── IDE awareness for shaders and assets ─────────────────────
OTHER_FILES +=  shaders/main.vert  \
                shaders/main.frag  \
                level.txt          \
