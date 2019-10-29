TARGET := carin


INCLUDEPATH := -I/home/xiaok/app/jpeglib/include/
INCLUDEPATH += -I /home/xiaok/app/sqlite/include/
INCLUDEPATH += -I./include
INCLUDEPATH += -I./include/freetype2/
LIBRARYPATH := -L/home/xiaok/app/jpeglib/lib/
LIBRARYPATH += -L /home/xiaok/app/sqlite/lib/
LIBRARYPATH += -L./lib -lfreetype
LINK := -ljpeg 
LINK += -lapi_v4l2_arm
LINK += -lpthread
LINK += -lsqlite3
LINK += -lm


SOUR := ./*.c 


$(TARGET) : $(SOUR)
	@-arm-linux-gcc $^ -o $@ $(INCLUDEPATH) $(LIBRARYPATH) $(LINK)
	

.PHONY : clean
clean:
	@-rm $(TARGET)