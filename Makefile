HOST_CC := gcc
MIPS_CC := mips-linux-gcc	#针对路由器的，一般的cpu架构都是mips msb吧:)，用于交叉编译
CC := $(HOST_CC)
DEBUG_FLAG := -DDEBUG -Wall -g -O0
RELEASE_FLAG := -Wall -g -O2
FLAG := $(DEBUG_FLAG) -DCONF_PATH=\"./drcomrc\"
RM := rm -rf

PRONAME := drcom

OBJS := main.o md5.o eapol.o

$(PRONAME): $(OBJS)
	$(CC) -o $@ $^ $(FLAG)
%.o: %.c
	$(CC) -c $^ $(FLAG)
clean:
	$(RM) $(OBJS) $(PRONAME)

