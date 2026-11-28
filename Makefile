EXE = lache_mon_android
IMGUI_DIR = imgui

SOURCES = main.cpp adb.cpp association.cpp conseils_espion.cpp catalogue_apps.cpp analyse.cpp rapport_analyse.cpp outils.cpp securite.cpp uad.cpp alternatives.cpp decouverte.cpp rapport.cpp usb.cpp \
	$(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_draw.cpp \
	$(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/imgui_widgets.cpp \
	$(IMGUI_DIR)/backends/imgui_impl_sdl2.cpp \
	$(IMGUI_DIR)/backends/imgui_impl_opengl3.cpp \
	$(IMGUI_DIR)/misc/cpp/imgui_stdlib.cpp

CXXFLAGS = -std=c++17 -Wall -I$(IMGUI_DIR) -I$(IMGUI_DIR)/backends -I$(IMGUI_DIR)/misc/cpp `sdl2-config --cflags`
LIBS = -lGL -ldl -pthread -lcrypto -lSDL2_image `sdl2-config --libs`

$(EXE): $(SOURCES) adb.h association.h conseils_espion.h catalogue_apps.h analyse.h rapport_analyse.h outils.h securite.h uad.h alternatives.h decouverte.h rapport.h usb.h json.h
	$(CXX) $(CXXFLAGS) -o $@ $(SOURCES) $(LIBS)

clean:
	rm -f $(EXE) tests/test_usb tests/test_association tests/test_conseils_espion

# Tests sans téléphone ni interface : make test
test: tests/test_usb.cpp usb.cpp usb.h outils.cpp outils.h \
      tests/test_association.cpp association.cpp association.h decouverte.cpp decouverte.h \
      tests/test_conseils_espion.cpp conseils_espion.cpp conseils_espion.h securite.h
	$(CXX) -std=c++17 -Wall -o tests/test_usb tests/test_usb.cpp usb.cpp outils.cpp -pthread
	$(CXX) -std=c++17 -Wall -o tests/test_association tests/test_association.cpp association.cpp decouverte.cpp outils.cpp -pthread
	$(CXX) -std=c++17 -Wall -o tests/test_conseils_espion tests/test_conseils_espion.cpp conseils_espion.cpp
	./tests/test_usb
	./tests/test_association
	./tests/test_conseils_espion

.PHONY: clean test
