all:
	gcc `pkg-config --cflags gtk4` -O2 -Wall -o main main.c -I include include/kissfft/kiss_fft.c include/kissfft/kiss_fftr.c -L lib -lm -l SDL2-2.0.0 `pkg-config --libs gtk4`
	gcc -O2 -Wall -o histo histogram.c -I include include/kissfft/kiss_fft.c include/kissfft/kiss_fftr.c -L lib -lm -l SDL2-2.0.0
	gcc `pkg-config --cflags gtk4` -o ui ui_test.c `pkg-config --libs gtk4`
	g++ -O2 -Wall -o test test.cpp -I include include/kissfft/kiss_fft.c include/kissfft/kiss_fftr.c -L lib -lm -l SDL2-2.0.0

clean:
	rm -rf main ui histo test a.out *.o *.gch
