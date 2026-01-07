#
# needs libircclient-dev
#
INSTALL_DIR = /usr/local/sbin

all: ircbot

ircbot: ircbot.cpp
	c++ -std=c++17 -O3 -g ircbot.cpp -o ircbot -lircclient -ldcserver -Wl,-rpath,/usr/local/lib

install: all
	install ircbot $(INSTALL_DIR)

installservice:
	cp ircbot.service /usr/lib/systemd/system/
	systemctl enable ircbot.service

clean:
	rm -f *.o ircbot
