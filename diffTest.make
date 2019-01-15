CC = pgcc -c11
#ACCFLAGS = -O4 -Mautoinline -acc=verystrict -ta=host,multicore,tesla -Minfo=all -mp
ACCFLAGS = -O4 -Mautoinline -acc=verystrict -ta=tesla
# -Minfo=all
#FAST = -O2 -Mautoinline -acc=verystrict

TARGET = dt
OBJEXT = o

UNAME := $(shell uname -a)
CCOUT := $(shell $(CC) 2>&1)

# top level targets
all:	build run

SRC_DIR=src
OBJ_DIR=obj

SRC = $(shell ls $(SRC_DIR)/*.c)
#SL= diffTest.c diff3D.c
#SRC:= $(SL:%.c=$(SRC_DIR)/%.c)
#OBJ:= $(SL=:%.c=$(OBJ_DIR)/%.o)

build: $(SRC)
	$(CC) $(ACCFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	@echo 'Cleaning up...'
	@rm -rf $(TARGET) $(OBJ) *.$(OBJEXT) *.dwf *.pdb prof
