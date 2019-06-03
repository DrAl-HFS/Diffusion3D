# Build library flavours
CC= pgcc -c11
CXX= pgc++ -std=c++11
#MAXFLAGS= -O4 -Mautoinline -acc=verystrict -ta=host,multicore,tesla -Minfo=all -mp
#FAST= -O2 -Mautoinline -acc=verystrict
ACCFLAGS= -Mautoinline -acc=verystrict -ta=tesla

UNAME:= $(shell uname -a)
CCOUT:= $(shell $(CC) 2>&1)

SRC_DIR:= src
HDR_DIR:= $(SRC_DIR)
OBJ_DIR:= lib


# NB a wildcard in the target path supersedes the ignore pattern
# and causes relative path to be generated prefixing each file -
# this must be accounted for in SRC and OBJ replacement rules... 
#SRC:= $(shell ls --ignore="*Test*" $(SRC_DIR)/*.c)
#SRC:= $(shell ls "-I*Test*" $(SRC_DIR)/*.c)
FILES:= $(shell ls -I*Test* -I*.h $(SRC_DIR))
#FILES:= diff3D.c diff3DUtil.c cluster.c clusterMap.c mapUtil.c util.c

SRC:= $(FILES:%.c=$(SRC_DIR)/%.c)
OBJ:= $(FILES:%.c=$(OBJ_DIR)/%.o)

TARGET:= lib/libDiff3D.so
OPTFLAGS:= -O2 -fPIC
DEFINES:= -DLFD3D=1
LDFLAGS:= -shared

### Phony Targets for building variants	###
.PHONY: all opt dbg gnu 

# NB - Must specify opt level for pgcc (else garbage output)
dbg: OPTFLAGS= -g -fPIC
# -Minfo=acc 
opt: OPTFLAGS= -O4 -fPIC

# Common additions not working... OPTFLAGS+= -fPIC

all: clean $(TARGET)
opt: $(TARGET)
dbg: $(TARGET)


### Minimal rebuild rules ###

# NB - building target directly from multiple sources in one line still results
# in the object files being created, but no "tidy up" rule will be triggered...

# CAVEAT EMPTOR: it seems that when using header dependancy, 
# every source file MUST have corresponding header or something breaks...

%.o: $(SRC_DIR)/%.c $(HDR_DIR)/%.h
	$(CC) $(OPTFLAGS) $(ACCFLAGS) $(DEFINES) $< -c

%.o: $(SRC_DIR)/%.cpp $(HDR_DIR)/%.h
	$(CXX) $(OPTFLAGS) $(ACCFLAGS) $DEFINES) $< -c

$(OBJ_DIR)/%.o : %.o
	mv $< $@
 
$(TARGET): $(OBJ)
	$(CC) $(OPTFLAGS) $(ACCFLAGS) $(LDFLAGS) $^ -o $@


### Phony Targets for odd jobs	###
.PHONY: clean

clean:
	@echo 'Cleaning up...'
	@rm -rf $(TARGET) $(OBJ) *.$(OBJEXT) *.dwf *.pdb prof
