HOST_CC := gcc
MIPS_CC := mips-linux-gcc	#针对路由器的，一般的cpu架构都是mips msb吧:)，用于交叉编译
CC := $(HOST_CC)
WIN_FLAG := -I wpcap/include -lws2_32 -L wpcap/lib -lwpcap -lpacket -D_WINDOWS -DHAVE_REMOTE
DEBUG_FLAG := -DDEBUG -Wall -g -O0
RELEASE_FLAG := -Wall -O2
FLAG := $(DEBUG_FLAG) -DCONF_PATH=\"./drcomrc\" $(WIN_FLAG)
RM := rm -rf

PRONAME := drcom

_OBJS := main.o md5.o eapol.o
OBJS_WIN := main.o md5.o eapol_win.o
OBJS := $(OBJS_WIN)

$(PRONAME): $(OBJS)
	$(CC) -o $@ $^ $(FLAG)
%.o: %.c
	$(CC) -c $^ $(FLAG)
clean:
	$(RM) *.o $(PRONAME) $(OBJS_WIN) win_config win_config.exe
win_config: win_config.o
	$(CC) -o $@ $^ $(FLAG)
%.o: %.c
	$(CC) -c $^ $(FLAG)
