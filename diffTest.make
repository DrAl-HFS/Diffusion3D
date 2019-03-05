CC = pgcc -c11
CXX = pgc++ -std=c++11
#MAXFLAGS = -O4 -Mautoinline -acc=verystrict -ta=host,multicore,tesla -Minfo=all -mp
#FAST = -O2 -Mautoinline -acc=verystrict
ACCFLAGS = -Mautoinline -acc=verystrict -ta=tesla
LDFLAGS=
OPTFLAGS= -O4

TARGET = dt
OBJEXT = o

UNAME := $(shell uname -a)
CCOUT := $(shell $(CC) 2>&1)

SRC_DIR=src
HDR_DIR=$(SRC_DIR)
OBJ_DIR=obj

SRC:= $(shell ls $(SRC_DIR)/*.c)
OBJ:= $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)


### Phony Targets for building variants	###
.PHONY: opt dbg all gnu

opt: $(TARGET)
dbg: $(TARGET)
all: clean $(TARGET)

# NB - Must specify opt level for pgcc (else garbage output)
opt: OPTFLAGS= -O4
dbg: OPTFLAGS= -g -Minfo=acc

gnu: CC= gcc -std=c11 -pedantic -Werror
gnu: ACCFLAGS= 
# -fopenacc # requires >=GCC6 ?
gnu: LDFLAGS= -lm
gnu: clean $(TARGET)


### Minimal rebuild rules ###

# CAVEAT EMPTOR: it seems that when using header dependancy, 
# every source file MUST have corresponding header or something breaks...

%.o: $(SRC_DIR)/%.c $(HDR_DIR)/%.h
	$(CC) $(OPTFLAGS) $(ACCFLAGS) $< -c

%.o: $(SRC_DIR)/%.cpp $(HDR_DIR)/%.h
	$(CXX) $(OPTFLAGS) $(ACCFLAGS) $< -c

$(OBJ_DIR)/%.o : %.o
	mv $< $@
 
$(TARGET): $(OBJ)
	$(CC) $(OPTFLAGS) $(ACCFLAGS) $(LDFLAGS) $^ -o $@


### Phony Targets for odd jobs	###
.PHONY: run map clean

run: $(TARGET)
	./$(TARGET)

map: $(TARGET)
	./$(TARGET) s256u8.raw

clean:
	@echo 'Cleaning up...'
	@rm -rf $(TARGET) $(OBJ) *.$(OBJEXT) *.dwf *.pdb prof
