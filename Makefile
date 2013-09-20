RELEASE := y

SHELL := bash
CXX := clang++ -std=c++11
UPX := upx --best --ultra-brute --lzma -qqq

ifeq ($(RELEASE), y)
	OPTIMIZE_FLAGS += -Oz
	WARNINGS_FLAGS += -w
	DEBUG_FLAGS += -g0
	LFLAGS += -s
else
	OPTIMIZE_FLAGS += -O0
	WARNINGS_FLAGS += -Weverything -Wno-padded -Wno-c++98-compat-pedantic -Wno-unused-member-function
	DEBUG_FLAGS += -ggdb3
	LFLAGS +=
endif

CXXFLAGS = -pipe -fPIE $(OPTIMIZE_FLAGS) $(DEBUG_FLAGS) $(LFLAGS) $(WARNINGS_FLAGS) $(shell pkg-config --cflags --libs Qt5Gui Qt5Widgets Qt5Multimedia | sed 's/-I\//-isystem\ \//g') -lboost_system


.PHONY : all dist clean

all: abyss-player

abyss-player: abyss.cpp Makefile
	@echo "BUILD $@"
	@cat <(cat abyss.cpp) <(moc abyss.cpp) | $(CXX) -x c++ $(CXXFLAGS) -o $@ -
ifeq ($(RELEASE), y)
	@echo "UPX $@"
	@$(UPX) $@
endif

dist:
	tar -cJf mdc-1.0.tar.xz $(DISTFILES)

clean:
	rm -f abyss-player

