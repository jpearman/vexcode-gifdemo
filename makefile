include vex/mkenv.mk

# location of the project source cpp and c files
SRC_C  = $(wildcard src/*.cpp) 
SRC_C += $(wildcard src/*.c)
SRC_C += $(wildcard src/*/*.cpp) 
SRC_C += $(wildcard src/*/*.c)

# location of include files that c and cpp files depend on
SRC_H  = $(wildcard include/*.h)

# project header file locations
INC_F  = include

OBJ = $(addprefix $(BUILD)/, $(addsuffix .o, $(basename $(SRC_C))) )

all: $(BUILD)/$(PROJECT).bin

include vex/mkrules.mk
