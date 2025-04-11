#include <stdio.h>
#include <stdlib.h>


#ifdef __linux__
#include <string.h>
#include "../../memory/linux/memory_linux_impl.cpp"
#elif _WIN32
#include "../../memory/win/memory_win_impl.cpp"
#endif

#include "../../memory/memory.cpp"
#include "../../vector/vector.cpp"


struct csp_packet_t {
    U32 CRC : 1;
    U32 RDP : 1;
    U32 XTEA : 1;
    U32 HMAC : 1;
    U32 Reserved : 4;
    U32 SourcePort : 6;
    U32 DestinationPort : 6;
    U32 Destination : 5;
    U32 Source : 5;
    U32 Priority : 2;

    U8 Data[65535];
};

int main() {

	Arena* MainArena = (Arena*)malloc(sizeof(Arena));
	MainArena->Init(0, 256 << 20);

	vector<U32> MyVector;
	MyVector.Init(MainArena, 256000);

	for (U32 i = 0; i < 255999; i += 1)
	{
		MyVector.PushBack(i);
	}

	fprintf(stdout, "Length  : %d\n", MyVector.GetLength());
	fprintf(stdout, "Capacity: %d\n", MyVector.GetCapacity());

	for (U32 i = 0; i < 255998; i += 1)
	{
		MyVector.PopBack();
	}

	fprintf(stdout, "Length  : %d\n", MyVector.GetLength());
	fprintf(stdout, "Capacity: %d\n", MyVector.GetCapacity());

	for (U32 i = 0; i < 255998; i += 1)
	{
		MyVector.PushFront(i);
	}

	fprintf(stdout, "Length  : %d\n", MyVector.GetLength());
	fprintf(stdout, "Capacity: %d\n", MyVector.GetCapacity());

  U8 ucDataBlock[40] = {
	// Offset 0x00000000 to 0x00000027
	0x0B, 0x01, 0x62, 0xB0, 0x94, 0x00, 0x52, 0x7D, 0x53, 0x51, 0x0B, 0x01,
	0x63, 0xB0, 0x94, 0x00, 0x52, 0x7D, 0x53, 0x51, 0x0B, 0x01, 0x64, 0xB0,
	0x94, 0x00, 0x52, 0x7D, 0x53, 0x51, 0x0B, 0x01, 0x65, 0xB0, 0x94, 0x00,
	0x52, 0x7D, 0x53, 0x51
};


  csp_packet_t p;
  memcpy(&p, ucDataBlock, 10);

  printf("Source     : %d\n", p.Source);
  printf("Destination: %d\n", p.Destination);
  printf("Source      Port: %d\n", p.SourcePort);
  printf("Destination Port: %d\n", p.DestinationPort);

  for( U32 i = 0; i < 10; i+= 1)
  {
        U8 c = ucDataBlock[i];
        for (U8 mask = 0x80; mask > 0; mask >>= 1) {
            putchar((c & mask) ? '1' : '0');
        }
        fprintf(stdout, "\n");
  }

	return 0;
}
