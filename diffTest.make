CC = pgcc -c11
CXX = pgc++ -std=c++11
#ACCFLAGS = -O4 -Mautoinline -acc=verystrict -ta=host,multicore,tesla -Minfo=all -mp
#FAST = -O2 -Mautoinline -acc=verystrict
ACCFLAGS = -Mautoinline -acc=verystrict -ta=tesla

TARGET = dt
OBJEXT = o

UNAME := $(shell uname -a)
CCOUT := $(shell $(CC) 2>&1)

SRC_DIR=src
HDR_DIR=$(SRC_DIR)

SRC:= $(shell ls $(SRC_DIR)/*.c)
OBJ:= $(SRC:$(SRC_DIR)/%.c=%.o)

opt: OPTFLAGS= -O4
dbg: OPTFLAGS= -g -Minfo=acc


### Minimal rebuild rules ###

# CAVEAT EMPTOR: it seems that when using header dependancy, 
# every source file MUST have corresponding header or make breaks...

%.o: $(SRC_DIR)/%.c $(HDR_DIR)/%.h
	$(CC) $(OPTFLAGS) $(ACCFLAGS) $< -c

%.o: $(SRC_DIR)/%.cpp $(HDR_DIR)/%.h
	$(CXX) $(OPTFLAGS) $(ACCFLAGS) $< -c

$(TARGET): $(OBJ)
	$(CC) $(OPTFLAGS) $(ACCFLAGS) $^ -o $@

all: $(TARGET)
opt: $(TARGET)
dbg: $(TARGET)


### Phony Targets for building variants	###
.PHONY: run map clean

run: $(TARGET)
	./$(TARGET)

map: $(TARGET)
	./$(TARGET) s256u8.raw

clean:
	@echo 'Cleaning up...'
	@rm -rf $(TARGET) $(OBJ) *.$(OBJEXT) *.dwf *.pdb prof
