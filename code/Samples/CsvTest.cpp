#include "../types.h"

#include "../memory.h"
#include "../allocator.h"
#include "../files.h"
#include "../strings.h"
#include "../encoders/csv_encoding.h"

#define MEMORY_IMPL
#include "../memory.h"
#define ALLOCATOR_IMPL
#include "../allocator.h"
#define FILES_IMPL
#include "../files.h"
#define STRINGS_IMPL
#include "../strings.h"

#include "../encoders/csv_encoding.cpp"

#include <chrono>

int main( void ) {

  printf("Creating a CSV file of 5 columns, 3 million rows\n");

  std::chrono::time_point LastFrame = std::chrono::high_resolution_clock::now();

  Arena* Arena = ArenaAllocDefault();

  csv_encoder Enc = CSV_Init("./CSVTest.csv", 5, Arena);
  const char* ColumnNames[] = { "Column1", "Column2", "Column3", "Column4", "Column5" };

  CSV_SetTitleNames(&Enc, ColumnNames);

  for (u64 i = 0; i < 3000000; i += 1) {
    CSV_BeginRow(&Enc);
    CSV_PushValue(&Enc, "My Value 1");
    CSV_PushValue(&Enc, "My Value 2");
    CSV_PushValue(&Enc, "My Value 3");
    CSV_PushValue(&Enc, "My Value 4");
    CSV_PushValue(&Enc, "My Value 5");
    CSV_EndRow(&Enc);
  }

  fprintf(stdout, "Writing...");
  CSV_WriteToFile(&Enc);

  fprintf(stdout, "Finished.\n");

  CSV_Destroy(&Enc);

  std::chrono::time_point t1 = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration<double, std::milli>(t1 - LastFrame).count();

  printf("Time in s: %.8lf\n", duration / 1000);

  return 0;
}