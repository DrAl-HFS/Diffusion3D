CC = pgcc -c11
#ACCFLAGS = -O4 -Mautoinline -acc=verystrict -ta=host,multicore,tesla -Minfo=all -mp
ACCFLAGS = -O4 -Mautoinline -acc=verystrict -ta=tesla
DBGFLAGS = -g -Mautoinline -acc=verystrict -ta=tesla -Minfo=acc
#FAST = -O2 -Mautoinline -acc=verystrict

TARGET = dt
OBJEXT = o

UNAME := $(shell uname -a)
CCOUT := $(shell $(CC) 2>&1)

SRC_DIR=src
HDR_DIR=$(SRC_DIR)

SRC:= $(shell ls $(SRC_DIR)/*.c)
OBJ:= $(SRC:$(SRC_DIR)/%.c=%.o)


### Minimal rebuild rules ###

# CAVEAT EMPTOR: it seems that when using header dependancy, 
# every source file MUST have corresponding header or make breaks...

%.o: $(SRC_DIR)/%.c $(HDR_DIR)/%.h
	$(CC) $(ACCFLAGS) $< -c

$(TARGET): $(OBJ)
	$(CC) $(ACCFLAGS) $^ -o $@

all: $(TARGET)


### Phony Targets for building variants	###
.PHONY: opt dbg run map clean

opt: $(SRC)
	$(CC) $(ACCFLAGS) -o $(TARGET) $(SRC)

dbg: $(SRC)
	$(CC) $(DBGFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

map: $(TARGET)
	./$(TARGET) s256u8.raw

clean:
	@echo 'Cleaning up...'
	@rm -rf $(TARGET) $(OBJ) *.$(OBJEXT) *.dwf *.pdb prof
