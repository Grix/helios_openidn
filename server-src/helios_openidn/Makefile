BIN=./build
PKG_NAME?=prog

SRCS_C=$(shell find . -type f -name '*.c')
SRCS_CPP=$(shell find . -type f -name '*.cpp')

CC=gcc

OBJ=$(SRCS_C:.c=.o) $(SRCS_CPP:.cpp=.o)
TARGETOBJ=$(addprefix $(BIN)/, $(OBJ))

#just so bcm isn't included in non-pi builds
ifdef PIBUILD
MyCFLAGS = -lbcm2835 -lusb-1.0 -lpthread -lm -O3 -g
BUILDFLAG=-DBUILD_PI
else
MyCFLAGS = -lpthread -lusb-1.0 -lm -O3 -g
undefine BUILDFLAG
endif


default: $(TARGETOBJ)
	$(CXX) -std=c++17  $(TARGETOBJ) -o $(PKG_NAME) $(CFLAGS) $(MyCFLAGS) $(LDFLAGS) 

$(BIN)/%.o: %.c
	mkdir -p $(@D)
	$(CC) $(BUILDFLAG) -c $< -o $@ $(CFLAGS) $(MyCFLAGS)

$(BIN)/%.o: %.cpp
	mkdir -p $(@D)
	$(CXX) -std=c++17 $(BUILDFLAG) -c $< -o $@ $(CFLAGS) $(MyCFLAGS)

clean:
	rm -f $(PKG_NAME)
	rm -rf $(BIN)
