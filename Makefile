ifeq ($(INSTALL_PREFIX),)
    INSTALL_PREFIX := /usr/local
endif

all:
	g++ -std=c++14 -O3 -o ttt main.cpp

clean:
	rm -rf ttt

install:
	cp ttt $(INSTALL_PREFIX)/bin/.
