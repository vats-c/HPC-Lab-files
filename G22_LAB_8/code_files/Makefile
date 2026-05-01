ifeq ($(OS),Windows_NT)
    CC = g++
    MPICC = g++
    MPI_FLAGS = -I"C:\Program Files (x86)\Microsoft SDKs\MPI\Include" -L"C:\Program Files (x86)\Microsoft SDKs\MPI\Lib\x64" -lmsmpi
    EXT = .exe
else
    CC = g++
    MPICC = mpicxx
    MPI_FLAGS = 
    EXT = .out
endif

CFLAGS = -O3
OMPFLAGS = -fopenmp

all: input_maker$(EXT) serial$(EXT) hybrid$(EXT)

input_maker$(EXT): input_file_maker.cpp
	$(CC) $(CFLAGS) input_file_maker.cpp -o input_maker$(EXT)

serial$(EXT): serial_baseline.cpp
	$(CC) $(CFLAGS) serial_baseline.cpp -o serial$(EXT)

hybrid$(EXT): main.cpp utils.cpp init.cpp
	$(MPICC) $(CFLAGS) $(OMPFLAGS) $(MPI_FLAGS) main.cpp utils.cpp init.cpp -o hybrid$(EXT)

clean:
	rm -f *$(EXT) input.bin Mesh.out
