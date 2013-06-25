CC = gcc

TARGET = ft

INC_PATH = -pthread -I/usr/include/gtk-3.0 -I/usr/include/atk-1.0 \
			-I/usr/include/at-spi2-atk/2.0 -I/usr/include/pango-1.0 \
			-I/usr/include/gio-unix-2.0/ -I/usr/include/cairo \
			-I/usr/include/gdk-pixbuf-2.0 -I/usr/include/glib-2.0 \
			-I/usr/lib/i386-linux-gnu/glib-2.0/include -I/usr/include/harfbuzz \
			-I/usr/include/freetype2 -I/usr/include/pixman-1 -I/usr/include/libpng12 \
			-I/usr/include/libxml2  

LIB_PATH = -lgtk-3 -lgdk-3 -latk-1.0 -lgio-2.0 -lpangocairo-1.0 -lgdk_pixbuf-2.0 \
			-lcairo-gobject -lpango-1.0 -lcairo -lgobject-2.0 -lglib-2.0 -lxml2  

			
CFLAGS += -g $(INC_PATH) 
LDFLAGS += -export-dynamic $(LIB_PATH)

OBJS = main.o install_software.o common.o profile.o

all:$(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)

