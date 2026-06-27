# ============================================================
#  Makefile for Tank Maze Game
#  Usage:  make          — build the game
#          make clean    — remove build artefacts
#          make run      — build and launch
# ============================================================

CXX      = g++
CXXFLAGS = -std=c++11 -Wall -g -O2
INCLUDES = -I. -I../common
LIBS     = -lGLEW -lglut -lGL

TARGET   = TankAssignment
BUILDDIR = build

# All source files
SOURCES = main.cpp \
          ../common/Shader.cpp \
          ../common/Vector.cpp \
          ../common/Matrix.cpp \
          ../common/Mesh.cpp \
          ../common/Texture.cpp \
          ../common/SphericalCameraManipulator.cpp

# Derive object file paths inside BUILDDIR
OBJECTS = $(patsubst %.cpp, $(BUILDDIR)/%.o, $(notdir $(SOURCES)))

# --- Targets ------------------------------------------------

.PHONY: all clean run

all: $(BUILDDIR) $(TARGET)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)
	@echo ""
	@echo "  ✓  Build successful  →  ./$(TARGET)"
	@echo ""

# Compile each source into build/ using vpath to find files
vpath %.cpp . ../common

$(BUILDDIR)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILDDIR) $(TARGET)

run: all
	./$(TARGET)
