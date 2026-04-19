CC = g++
CFLAGS = -O3 -fopenmp
LDFLAGS = -lm -fopenmp

# Targets
all: main.out input_maker.out serial_baseline.out

main.out: main.cpp utils.cpp init.cpp init.h utils.h
	$(CC) $(CFLAGS) main.cpp utils.cpp init.cpp $(LDFLAGS) -o main.out

serial_baseline.out: serial_baseline.cpp init.cpp init.h
	$(CC) -O3 serial_baseline.cpp init.cpp -lm -o serial_baseline.out

input_maker.out: input_file_maker.cpp
	$(CC) -O3 input_file_maker.cpp -o input_maker.out

clean:
	rm -f *.out *.exe Mesh.out input.bin

.PHONY: all clean
