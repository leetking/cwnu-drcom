# **编译选项在这里修改**
#TARGET只可以取WIN, LINUX, OPENWRT几个选项
#WIN 为windows程序编译
#LINUX 为*nix系列系统编译
#OPENWRT 专为openwrt路由器系统编译，并且会生成ipk安装包, NOTE 选择OPENWRT时，需要确定MIPS的大小端
TARGET := LINUX
#只有当TARGET为OPENWRT才定义MIPS，默认为MSB
#大端: MSB; 小端: LSB
MIPS := MSB
#下面是高级选项
#如果是, 就取DEBUG，否则就是空
IS_DEBUG :=
#如果是, 就取GUI，否则就是空
IS_GUI :=
#交叉编译，在TARGET选为OPENWRT并且制定MIPS时会自动确定CROSS的值
#e.g. CROSS := mips-openwrt-linux-
CROSS :=
# **编译选项到此**

RM := rm -rf
CP := cp -r
MKDIR := mkdir -p
SED := sed

CC := gcc

VERSION := 0.0.4.1
CONFIG	 := ./drcomrc
ifneq "$(IS_GUI)" ""
	RES := resource
	ICON_PATH := $(RES)/icon.png
endif #IS_GUI == ""
ifeq ($(TARGET), WIN)
	VERSION := $(VERSION)-win
else
ifeq ($(TARGET), OPENWRT)
ifeq ($(MIPS), MSB)
	CROSS := mips-openwrt-linux-
	VERSION := $(VERSION)-openwrt-msb
else
	CROSS := mipsel-openwrt-linux-
	VERSION := $(VERSION)-openwrt-lsb
endif #MIPS == MSB
else
	VERSION := $(VERSION)-linux-amd64
endif #CC == gcc
endif #TARGET == WIN
CC := $(CROSS)$(CC)
APP := Drcom4CWNU-$(VERSION)

CFLAGS_WIN		:= -I wpcap/include -DWINDOWS -DHAVE_REMOTE
LDFLAGSS_WIN	:= -lws2_32 -L wpcap/lib -lwpcap -lpacket
CFLAGS_GUI		:= `pkg-config --cflags gtk+-2.0` -DICON_PATH=\"$(ICON_PATH)\" -DGUI
LDFLAGSS_GUI	:= `pkg-config --libs gtk+-2.0`
CFLAGS_DEBUG	:= -DDEBUG -g -O0 -Wall -Wno-unused
LDFLAGS_DEBUG	:=
CFLAGS_RELEASE	:= -O2 -W -Wall

OBJS	:= md5.o config.o common.o
CFLAGS	:= -DCONF_PATH=\"$(CONFIG)\" -DVERSION=\"$(VERSION)\"
LDFLAGS :=

ifeq ($(findstring gcc, $(CC)), gcc)
ifeq ($(TARGET), LINUX)
	CFLAGS_DEBUG += -pg
	LDFLAGS_DEBUG += -pg
endif
endif

ifeq ($(TARGET), WIN)
	CFLAGS += $(CFLAGS_WIN)
	LDFLAGS += $(LDFLAGSS_WIN)
	OBJS += eapol_win.o
	WIN_DLLS := win_dlls
ifeq "$(IS_GUI)" ""
	LDFLAGSS_GUI += -mwindows
endif #IS_GUI
else
ifeq ($(TARGET), OPENWRT)
	IPK := ipk
endif #TARGET == OPENWRT
	CFLAGS += -DLINUX
	INSTALL := install
	OBJS += eapol.o drcom.o
endif

ifeq ($(IS_DEBUG), DEBUG)
	CFLAGS += $(CFLAGS_DEBUG)
	LDFLAGS += $(LDFLAGS_DEBUG)
else
	CFLAGS += $(CFLAGS_RELEASE)
endif

ifeq ($(IS_GUI), GUI)
	CFLAGS += $(CFLAGS_GUI)
	LDFLAGS += $(LDFLAGSS_GUI)
	OBJS += main_gui.o gui.o
else
	OBJS += main_cli.o wrap_eapol.o dhcp.o
endif

all: drcom $(IPK)

drcom: $(OBJS)
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
	$(CP) drcom drcomrc.example $(APP)
	zip -r $(APP).zip $(APP)
$(APP):
	if [ ! -e $(APP) ]; then \
		$(MKDIR) $(APP); \
	fi
$(RES): $(APP)
	$(CP) $(RES) $(APP)
$(WIN_DLLS): $(APP) winpcap.exe scripts/drcom-login.bat docs/HOW-TO-USE.txt
	dlls= `ldd drcom | grep -E '/mingw32|/usr|/lib' | awk '{print $$3}' | sort | uniq`
	if [ ${dlls} ]; then \
		$(CP) -L ${dlls} $(APP); \
	fi
	$(CP) winpcap.exe scripts/drcom-login.bat docs/HOW-TO-USE.txt $(APP)
$(INSTALL): drcom
	if [ ! -e dist ]; then \
		$(MKDIR) dist; \
	fi
	$(CP) drcom drcomrc.example dist
$(IPK): drcom random_mac
	$(MKDIR) ./usr/lib/lua/luci/controller/
	$(MKDIR) ./usr/lib/lua/luci/model/cbi/
	$(MKDIR) ./etc/config/
	$(MKDIR) ./etc/init.d/
	$(MKDIR) ./overlay/Drcom4CWNU/
	$(MKDIR) ./usr/bin/
	$(CP) openwrt/luci/Drcom4CWNU.reg.lua     ./usr/lib/lua/luci/controller/Drcom4CWNU.lua
	$(CP) openwrt/luci/Drcom4CWNU.cbi.lua     ./usr/lib/lua/luci/model/cbi/Drcom4CWNU.lua
	$(CP) openwrt/luci/drcomrc.etc            ./etc/config/drcomrc
	$(CP) openwrt/scripts/drcom.sh            ./etc/init.d/drcom.sh
	chmod +x                                  ./etc/init.d/drcom.sh
	$(CP) openwrt/scripts/drcom-daemon.sh     ./etc/init.d/drcom-daemon
	chmod +x                                  ./etc/init.d/drcom-daemon
	$(CP) openwrt/scripts/wr2drcomrc.sh       ./overlay/Drcom4CWNU/wr2drcomrc.sh
	chmod +x                                  ./overlay/Drcom4CWNU/wr2drcomrc.sh
	$(CP) drcom                               ./overlay/Drcom4CWNU/drcom
	$(CP) drcomrc.example                     ./overlay/Drcom4CWNU/drcomrc
	$(CP) random_mac                          ./usr/bin/random_mac
	tar -czf data.tar.gz ./usr ./etc ./overlay
	$(CP) openwrt/ipk/control                 ./control
	$(SED) -i "s/Version.*/Version: $(VERSION)/"                                  ./control
	$(SED) -i "s/Installed-Size.*/Installed-Size: `du -b data.tar.gz | cut -f1`/" ./control
	$(SED) -i "s/Architecture.*/Architecture: MIPS-$(MIPS)/"                      ./control
	tar -czf ./control.tar.gz ./control
	$(CP) openwrt/ipk/debian-binary           ./debian-binary
	tar -cf $(APP) ./data.tar.gz ./debian-binary ./control.tar.gz
	cat $(APP) | gzip -c > $(APP).ipk
	$(RM) ./usr ./etc ./overlay ./data.tar.gz ./debian-binary ./control.tar.gz ./control
	$(RM) $(APP)
random_mac: openwrt/random_mac.c
	$(CC) -o $@ $^ $(CFLAGS)

help:
	@echo "make help|all|release|tar|dist-clean"
	@echo "     help: Show this page."
	@echo "     all: Build all src."
	@echo "     release: Build all src and tar executable binary program & resource files."
	@echo "     tar: tar src files & resource files."
	@echo "     dist-clean: clean all exclude original file such as src & 'Makefile'."
	@echo "     install: install soft to current 'dist' folder. NOTE: just for linux!"
	@echo "set VARIABLE"
	@echo "$$ make TARGET=target IS_GUI=gui"
	@echo "     target: LINUX or WINDOWS"
	@echo "     gui: GUI or empty(don't type it. e.g. 'IS_GUI=')"

clean:
	$(RM) *.o netif-config.exe netif-config drcom dist gmon.out random_mac
dist-clean: clean
	$(RM) cscope.* tags dist
	$(RM) $(APP)* *.ipk
.PHONY: clean all tar dist-clean release help install ipk
