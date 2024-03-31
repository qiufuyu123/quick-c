CC := gcc
CC_FLAGS := -Wall -std=gnu99 -O0 -g -I ../include

SRC = $(wildcard ./*.c ./lib/*.c)
OBJ = $(patsubst %.c, %.o, $(SRC))

all: $(OBJ)
	cd ../build && $(CC) $(OBJ) -o ../quick-c

%.o: %.c 
	$(CC) $(CC_FLAGS) -c $< -o ../build/$@
	echo [OK] $<
