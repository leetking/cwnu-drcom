#CC := gcc
CC := mips-openwrt-linux-gcc
#CC := mipsel-openwrt-linux-gcc
#CC := mips-linux-gcc	#针对路由器的，一般的cpu架构都是mips msb吧:)，用于交叉编译
RM := rm -rf

#编译选项在这里修改
#TARGET只可以取WIN, LINUX几个选项
TARGET := LINUX
#如果是, 就取DEBUG，否则就是空
IS_DEBUG := DEBUG
#如果是, 就取GUI，否则就是空
IS_GUI := 

CONFIG   := \"./drcomrc\"
RESOURCE := resource
ICON_PATH		:= \"$(RESOURCE)/icon.png\"

CFLAGS_WIN		:= -I wpcap/include -DWINDOWS -DHAVE_REMOTE
LDFLAGSS_WIN	:= -lws2_32 -lkernel32 -L wpcap/lib -lwpcap -lpacket
CFLAGS_GUI		:= `pkg-config --cflags gtk+-2.0` -DICON_PATH=$(ICON_PATH)
LDFLAGSS_GUI	:= `pkg-config --libs gtk+-2.0`
CFLAGS_DEBUG	:= -DDEBUG -Wall -g -O0
CFLAGS_RELEASE	:= -O2

OBJS	:= md5.o config.o
CFLAGS	:= -DCONF_PATH=$(CONFIG)
LDFLAGS	:=

ifeq ($(TARGET), WIN)
	CFLAGS += $(CFLAGS_WIN)
	LDFLAGS += $(LDFLAGSS_WIN)
	OBJS += eapol_win.o
else
	CFLAGS += -DLINUX
	OBJS += eapol.o
endif
ifeq ($(IS_DEBUG), DEBUG)
	CFLAGS += $(CFLAGS_DEBUG)
else
	CFLAGS += $(CFLAGS_RELEASE)
endif
ifeq ($(IS_GUI), GUI)
	CFLAGS += $(CFLAGS_GUI)
	LDFLAGS += $(LDFLAGSS_GUI)
	OBJS += main_gui.o gui.o
else
	OBJS += main_cli.o wrap_eapol.o
endif

all: drcom $(CONFIG_WIN)

drcom: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
%.o: %.c
	$(CC) -c $^ $(CFLAGS)

zip:

clean:
	$(RM) *.o drcom
.PHONY: clean all zip
