CC := gcc
#CC := mips-linux-gcc	#针对路由器的，一般的cpu架构都是mips msb吧:)，用于交叉编译
RM := rm -rf
CP := cp -r
MKDIR := mkdir -p

#编译选项在这里修改
#TARGET只可以取WIN, LINUX几个选项
TARGET := WIN
#如果是, 就取DEBUG，否则就是空
IS_DEBUG := DEBUG
#如果是, 就取GUI，否则就是空
IS_GUI := GUI

VERSION := 0.0.1
CONFIG	 := \"./drcomrc\"
ifneq "$(IS_GUI)" ""
	RES := resource
	ICON_PATH := \"$(RES)/icon.png\"
endif
APP := cwnu-drcom-$(VERSION)

CFLAGS_WIN		:= -I wpcap/include -DWINDOWS -DHAVE_REMOTE
LDFLAGSS_WIN	:= -lws2_32 -L wpcap/lib -lwpcap -lpacket
CFLAGS_GUI		:= `pkg-config --cflags gtk+-2.0` -DICON_PATH=$(ICON_PATH) -DGUI
LDFLAGSS_GUI	:= `pkg-config --libs gtk+-2.0`
CFLAGS_DEBUG	:= -DDEBUG -g -O0 -Wall -Wno-unused
CFLAGS_RELEASE	:= -O2 -W -Wall

OBJS	:= md5.o config.o
CFLAGS	:= -DCONF_PATH=$(CONFIG)
LDFLAGS :=

ifeq ($(TARGET), WIN)
	CFLAGS += $(CFLAGS_WIN)
	LDFLAGS += $(LDFLAGSS_WIN)
	OBJS += eapol_win.o
	WIN_DLLS := win_dlls
ifeq "$(IS_GUI)" ""
	NETIF_CONFIG := netif-config
	LDFLAGSS_GUI += -mwindows
endif #IS_GUI
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
	OBJS += main_cli.o
endif

all: drcom $(NETIF_CONFIG)

drcom: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
%.o: %.c
	$(CC) -c $^ $(CFLAGS)

netif-config: netif-config.o
	$(CC) -o $@ $^ $(LDFLAGS)
%.o: %.c
	$(CC) -c $^ $(CFLAGS)

#打包源代码
tar: *.c *.h drcomrc.example Makefile $(RES)
	if [ ! -e $(APP) ]; then \
		$(MKDIR) $(APP); \
	fi
	$(CP) $^ $(APP)
	tar -Jcvf $(APP).tar.xz $(APP)
	$(RM) $(APP)

#打包编译好的程序
release: all drcomrc.example $(RES) $(WIN_DLLS) $(APP)
	$(CP) drcom drcomrc.example $(NETIF_CONFIG) $(APP)
	zip -r $(APP).zip $(APP)
$(APP):
	if [ ! -e $(APP) ]; then \
		$(MKDIR) $(APP); \
	fi
$(RES): $(APP)
	$(CP) $(RES) $(APP)
$(WIN_DLLS): $(APP)
	$(CP) -L `ldd drcom $(NETIF_CONFIG)| grep -E '/mingw32|/usr|/lib|packet.dll|wpcap.dll' | awk '{print $$3}' | sort | uniq` $(APP)

help:
	@echo "make help|all|release|tar|dist-clean"
	@echo "     help: Show this page."
	@echo "     all: Build all src."
	@echo "     release: Build all src and tar executable binary program & resource files."
	@echo "     tar: tar src files & resource files."
	@echo "     dist-clean: clean all exclude original file such as src & 'Makefile'."
	@echo "set VARIABLE"
	@echo "$$ make TARGET=target IS_GUI=gui"
	@echo "     target: LINUX or WINDOWS"
	@echo "     gui: GUI or empty(don't type it. e.g. 'IS_GUI=')"

clean:
	$(RM) *.o netif-config.exe netif-config drcom
dist-clean: clean
	$(RM) cscope.* tags
	$(RM) $(APP)*
.PHONY: clean all tar dist-clean release help
