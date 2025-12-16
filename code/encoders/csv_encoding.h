#ifndef _CSV_ENCODING_H_
#define _CSV_ENCODING_H_

#include "../types.h"
//#include "../memory.h"

//#include "../strings.h"

#define MAX_CSV_COLUMNS      256
#define CSV_COLUMN_NAME_SIZE 256
#define MAX_CSV_ROWS         (((U64)256) << 20)

typedef struct csv_encoder csv_encoder;
struct csv_encoder {
  // File handler
  //
  f_file  File;
  Arena*  Arena;
  void*   BackBuffer;

  // CSV handler
  //
  char ColumnNames[MAX_CSV_COLUMNS][CSV_COLUMN_NAME_SIZE];
  U8_String Rows;

  u64  CurrentRow;
  u64  CurrentColumn;
  u64  NColumns;
  u64  NRows;
  char Delimiter;
};

fn_internal csv_encoder CSV_Init(const char* FilePath, u64 N_Columns, Arena* Arena);

fn_internal void CSV_SetTitleNames(csv_encoder* Csv, const char* names[]);

fn_internal void CSV_BeginRow(csv_encoder* Csv);

fn_internal void CSV_EndRow(csv_encoder* Csv);

fn_internal void CSV_PushValue(csv_encoder* Csv, const char* Value);

fn_internal void CSV_Destroy(csv_encoder* Csv);

fn_internal void CSV_WriteToFile(csv_encoder* Csv);

#endif // _CSV_ENCODING_H_