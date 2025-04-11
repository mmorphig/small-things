#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  
  char* filename= argv[1];
  printf("Opening file: %s\n\n", filename);
 
  FILE *file = fopen(filename,"rb");
 
  if (file == NULL) {
    perror("Error while opening the file.\n");
    exit(EXIT_FAILURE);
  }
  int i = 0;
  char ch;
  int chi = 0x0000000; 
  char chBuffer[16];
  while( ( ch = fgetc(file) ) != EOF ) {
		if (!(+i % 16)) {
			printf("%08X  ",chi);
		}
		chi++;
    printf("%02X ",ch); 
    if(!((+i + 1) % 8)) putc(0x20, stdout); 
    if (ch == 0x0A || ch == 0x09) {
		  chBuffer[i % 16] = ' ';
		} else {
		  chBuffer[i % 16] = ch;
		}
    if (!(++i % 16)) {
			for (int j = 0; j < 16; j++) {
				putc(chBuffer[j], stdout);
			}
			putc(0x0A, stdout);
		}
  }
	for (int j = 0; j<16-(i % 16); j++) {      
		if (16-(i % 16) == 16) break; 
		if(!((+j + i) % 8) && !(16-(i % 16) == 8)) putc(0x20, stdout);
		printf(".. ");
	}
	putc(0x20, stdout);
	for (int j = 0; j<(i % 16); j++) {
		printf("%c",chBuffer[j]);
  }
  putc(0x0A, stdout);
}