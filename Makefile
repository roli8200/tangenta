# TANGENTA – bauen, testen, Pakete schnüren
#
#   Linux:    make <ziel>
#   Windows:  mingw32-make <ziel>     (liegt in %USERPROFILE%\Qt\Tools\mingw1310_64\bin)
#
# Ziele:
#   help        diese Übersicht
#   deps        Werkzeuge und Qt einrichten (Windows: aqtinstall; Linux: zeigt den apt-Befehl)
#   configure   CMake einrichten (build/release bzw. build/debug)
#   build       übersetzen
#   test        übersetzen und alle Tests laufen lassen
#   run         übersetzen und starten
#   install     Linux: nach $(PREFIX) installieren (sudo)
#   dist        Windows: startfähige Fassung samt Qt nach build/dist/TANGENTA
#   packages    Windows: MSI und Setup.exe; Linux: .deb für Debian 12/13 und Ubuntu 24.04/26.04
#   clean       Build-Verzeichnis löschen
#
# Einstellungen: BUILD_TYPE=Release|Debug, QT_DIR=<Qt-Präfix> (Linux, wenn nicht das System-Qt),
#                DEB_TARGETS="debian12 debian13 ubuntu24.04 ubuntu26.04", PREFIX=/usr/local

BUILD_TYPE  ?= Release
DEB_TARGETS ?= debian12 debian13 ubuntu24.04 ubuntu26.04
PREFIX      ?= /usr/local

.PHONY: help deps configure build test run install dist packages clean

ifeq ($(OS),Windows_NT)

PS := powershell -NoProfile -ExecutionPolicy Bypass -File

help:
	@powershell -NoProfile -Command "Get-Content Makefile -TotalCount 22 | Select-Object -Skip 2"

deps configure build test run clean:
	$(PS) tools/dev.ps1 $@ -BuildType $(BUILD_TYPE)

dist:
	$(PS) tools/build-windows.ps1 -Target build/dist/TANGENTA

packages:
	$(PS) tools/package-windows.ps1

install:
	@echo "Unter Windows: mingw32-make packages und dann build/packages/TANGENTA-*-setup.exe ausführen"

else

help:
	@sed -n '3,22p' Makefile

BUILD_DIR := build/$(shell echo $(BUILD_TYPE) | tr A-Z a-z)
CMAKE_ARGS := -G Ninja -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_INSTALL_PREFIX=$(PREFIX)
ifdef QT_DIR
CMAKE_ARGS += -DCMAKE_PREFIX_PATH=$(QT_DIR) -DCMAKE_INSTALL_RPATH=$(QT_DIR)/lib
endif

deps:
	@echo "Debian 13 / Ubuntu 26.04 (Qt 6.8 oder neuer aus dem System):"
	@echo "  sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-svg-dev qt6-declarative-dev \\"
	@echo "       qt6-image-formats-plugins libgl1-mesa-dev libxkbcommon-dev"
	@echo "Debian 12 / Ubuntu 24.04 (System-Qt 6.4 ist zu alt): Qt 6.10 über aqtinstall, dann make QT_DIR=..."
	@echo "  sudo apt install build-essential cmake ninja-build python3-venv libgl1-mesa-dev libxkbcommon-dev libxcb-cursor0"
	@echo "  python3 -m venv ~/.aqt && ~/.aqt/bin/pip install aqtinstall"
	@echo "  ~/.aqt/bin/aqt install-qt linux desktop 6.10.3 linux_gcc_64 -m qtimageformats -O ~/Qt"
	@echo "  make build QT_DIR=~/Qt/6.10.3/gcc_64"

$(BUILD_DIR)/build.ninja:
	cmake -S . -B $(BUILD_DIR) $(CMAKE_ARGS)

configure: $(BUILD_DIR)/build.ninja

build: configure
	cmake --build $(BUILD_DIR)

test: build
	cd $(BUILD_DIR) && QT_QPA_PLATFORM=offscreen ctest --output-on-failure

run: build
	./$(BUILD_DIR)/tangenta

install: build
	sudo cmake --install $(BUILD_DIR)

dist:
	@echo "Unter Linux: make packages (fertige .deb) oder make install"

packages:
	bash packaging/linux/package-linux.sh $(DEB_TARGETS)

clean:
	rm -rf build

endif
