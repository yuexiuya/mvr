TOP=../
-include ${TOP}/Makefile.rules

# -print-search-dirs
SOURCES=$(wildcard *.c)
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=mvr
SCRIPTS=$(wildcard *.xml)

ifdef CROSS
SYSROOTFS=/home/mef/iMX6/fsl-release-bsp/build-dfb/tmp/sysroots/imx6qsabresd
CFLAGS += -DCROSS
CFLAGS += -I${SYSROOTFS}/usr/include/libxml2 \
		  -I${INCLUDE_DIR}
LDFLAGS = -lxml2 -pthread -L${LIB_DIR} -lipc
else
#run pkg-config --cflags libxml-2.0 to get CFLAGS
CFLAGS += -I/usr/include/libxml2 -I${INCLUDE_DIR}
#run pkg-config --libs libxml-2.0 to get LDFLAGS
LDFLAGS = -lxml2 -pthread -L${LIB_DIR} -lipc
endif

.PHONY : all install clean

all: $(EXECUTABLE)
	
$(EXECUTABLE): $(OBJECTS) 
	@echo "---------- Build ${EXECUTABLE} application"
#BUG in ubuntu 14.04 LDFLAGS must be in the end
	-@$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "---------- Install ${EXECUTABLE} application on ${INSTALL_DIR}"
	@$ mv ${EXECUTABLE} ${INSTALL_DIR}
	-@$ cp ${SCRIPTS} ${INSTALL_DIR}

.c.o:
	@echo "---------- Build $<"
	-@$(CC) $(CFLAGS) $< -o $@

scp: $(EXECUTABLE)
ifdef CROSS
	@echo "---------- Install ${EXECUTABLE} application on ${TARGET}"
	-@$ sudo scp ${INSTALL_DIR}/${EXECUTABLE} ${TARGET}
	-@$ sudo scp ${INSTALL_DIR}/${SCRIPTS} ${TARGET}
endif

clean:
	@echo "---------- Remove ${EXECUTABLE} application"
	@$ if [ -e ${EXECUTABLE} ]; then rm ${EXECUTABLE}; fi;
	@echo "---------- Remove object files"
	@$ rm -f *.o

