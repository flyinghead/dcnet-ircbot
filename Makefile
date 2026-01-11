#
# dependencies: libircclient-dev libdcserver
#
prefix = /usr/local
exec_prefix = $(prefix)
sbindir = $(exec_prefix)/sbin

all: ircbot

ircbot: ircbot.cpp
	c++ -std=c++17 -O3 -g ircbot.cpp -o ircbot -lircclient -lpthread -ldcserver -Wl,-rpath,/usr/local/lib

install: all
	mkdir -p $(DESTDIR)$(sbindir)
	install ircbot $(DESTDIR)$(sbindir)

installservice:
	cp ircbot.service /usr/lib/systemd/system/
	systemctl enable ircbot.service

clean:
	rm -f *.o ircbot
