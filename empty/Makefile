BINARY=main
OBJ=${BINARY}.o
CXXFLAGS=-std=c++14 -Wall -Wextra -nostdlib -m32 -static -fno-PIC -g

all: $(BINARY)

run: $(BINARY)
	./$(BINARY)

$(BINARY): $(OBJ)
	$(CXX) $(CXXFLAGS) $< -o $(BINARY)

clean:
	$(RM) $(OBJ) $(BINARY)
