LIBS   = gtk+-2.0 poppler poppler-glib
FLAGS  = `pkg-config --cflags --libs $(LIBS)` 
SOURCE = zathura.c
TARGET = zathura

all: $(TARGET)

$(TARGET): zathura.c config.h
	gcc $(FLAGS) -Wall -o $(TARGET) $(SOURCE) 

clean:
	rm -f $(TARGET)

debug: $(TARGET)
	gcc $(FLAGS) -Wall -o $(TARGET) $(SOURCE) -g

valgrind: debug $(TARGET)
	valgrind --tool=memcheck --leak-check=yes --show-reachable=yes ./${TARGET}

install: all
	@echo installing executeable to /usr/bin
	@mkdir -p /usr/bin
	@cp -f ${TARGET} /usr/bin
	@chmod 755 /usr/bin/${TARGET}

uninstall:
	@echo removing executeable from /usr/bin
	@rm -f /usr/bin/${TARGET}
