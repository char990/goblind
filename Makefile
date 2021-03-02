TARGET = goblind
 
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ 
SOURCES = $(wildcard *.c)
OBJS = $(patsubst %.c,%.o,$(SOURCES))
$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $^
#	$(CC) $(CFLAGS) -lcrypto -lssl -o $(TARGET) $^
.PHONY: clean
clean:
	-rm -f $(TARGET) $(OBJS)
