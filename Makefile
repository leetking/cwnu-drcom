CC := gcc
#CC := mips-linux-gcc	#针对路由器的，一般的cpu架构都是mips msb吧:)，用于交叉编译
RM := rm -rf

#编译选项在这里修改
#编译环境在msys里, TARGET只可以取WIN, LINUX几个选项
TARGET := WIN
#是就取DEBUG，否则就是空
IS_DEBUG := DEBUG
#是就取GUI，否则就是空
IS_GUI :=

CFLAG_WIN := -I wpcap/include -D_WINDOWS -DHAVE_REMOTE
LIBS_WIN := -lws2_32 -L wpcap/lib -lwpcap -lpacket
CFLAG_GUI := `pkg-config --flags gtk+-2.0`
LIBS_GUI := `pkg-config --libs gkt+-2.0`
DEBUG_CFLAG := -DDEBUG -Wall -g -O0
RELEASE_CFLAG := -O2

OBJS := md5.o
CFLAG := -DCONF_PATH=\"./drcomrc\"
LIBS :=

ifeq ($(TARGET), WIN)
	CFLAG += $(CFLAG_WIN)
	LIBS += $(LIBS_WIN)
	OBJS += eapol_win.o
else
	OBJS += eapol.o
endif
ifeq ($(IS_DEBUG), DEBUG)
	CFLAG += $(DEBUG_CFLAG)
else
	CFLAG += $(RELEASE_CFLAG)
endif
ifeq ($(IS_GUI), GUI)
	CFLAG += $(CFLAG_GUI)
	LIBS += $(LIBS_GUI)
	OBJS += main_gui.o gui.o
else
	OBJS += main.o
endif

PRONAME := drcom

$(PRONAME): $(OBJS)
	$(CC) -o $@ $^ $(LIBS)
%.o: %.c
	$(CC) -c $^ $(CFLAG)

config_win: config_win.o
	$(CC) -o $@ $^ $(LIBS)
%.o: %.c
	$(CC) -c $^ $(CFLAG)
clean:
	$(RM) *.o $(PRONAME) $(OBJS_WIN) config_win.exe config_win
