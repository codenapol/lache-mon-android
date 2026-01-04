EXE = lache_mon_android
IMGUI_DIR = imgui

SOURCES = main.cpp outils.cpp securite.cpp uad.cpp alternatives.cpp \
	$(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_draw.cpp \
	$(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/imgui_widgets.cpp \
	$(IMGUI_DIR)/backends/imgui_impl_sdl2.cpp \
	$(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp \
	$(IMGUI_DIR)/misc/cpp/imgui_stdlib.cpp

CXXFLAGS = -std=c++17 -Wall -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends -I$(IMGUI_DIR)/misc/cpp `sdl2-config --cflags`
LIBS = -lGL -ldl -pthread -lcrypto -lSDL2_image `sdl2-config --libs`

$(EXE): $(SOURCES) outils.h securite.h uad.h alternatives.h json.h
	$(CXX) $(CXXFLAGS) -o $@ $(SOURCES) $(LIBS)

clean:
	rm -f $(EXE)
