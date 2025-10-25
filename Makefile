CC = gcc
# CCFLAGS = -Wall -Wextra -std=c99 -O3 -march=native -ffast-math -funroll-loops -pthread
# CCFLAGS = -Wall -Wextra -std=c99 -pthread -g
CCFLAGS = -Wall -Wextra -std=c99 -pthread -O3
CCLINKFLAGS = -L/opt/homebrew/opt/libomp/lib
INC = -I/opt/homebrew/opt/libomp/include

SRCDIR = src
INCLUDEDIR = include
BINDIR = bin

TARGET = $(BINDIR)/splat

SRC = $(wildcard $(SRCDIR)/*.c)
SRC += $(wildcard extern/**/*.c)
OBJ = $(subst $(SRCDIR), $(BINDIR), $(SRC:.c=.o))
INCLUDES = $(wildcard $(INCLUDEDIR)/**/*.h)

INC += -I ./include 
INC += -I $(HOME)/software/glfw-3.4.bin.MACOS/include
INC += -I ./extern/rply
CCLINKFLAGS += -lGL -lglfw -lm
# CCLINKFLAGS += -L $(HOME)/software/glfw-3.4.bin.MACOS/lib-arm64
# CCLINKFLAGS += -lglfw3
# CCLINKFLAGS += -framework OpenGL -framework IOKit -framework Cocoa

all: $(TARGET)

$(BINDIR):
	mkdir -p $(BINDIR)

$(BINDIR)/%.o: $(SRCDIR)/%.c $(INCLUDES)
	@echo $(INCLUDES)
	$(CC) -o $@ -g $(CCFLAGS) $(INC) -c $<

$(TARGET): $(BINDIR) $(OBJ)
	$(CC) -o $@ $(OBJ) $(CCFLAGS) $(CCLINKFLAGS)
	./bin/splat ~/Downloads/Rose.ply
	# rm -f $(OBJ)

clean:
	rm -f $(TARGET) $(OBJ)
	rm -r $(BINDIR)

.PHONY: all clean

