all: local

local:
	gcc netpong.c -lncurses -lpthread -o netpong
