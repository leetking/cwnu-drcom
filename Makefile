CC := gcc
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

