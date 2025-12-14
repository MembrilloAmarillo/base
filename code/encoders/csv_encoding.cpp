fn_internal csv_encoder CSV_Init(const char* FilePath, u64 N_Columns, Arena* Arena) {
  csv_encoder Csv = {};

  Csv.File  = F_OpenFile( FilePath, static_cast<f_flags>(WRONLY | APPEND) );
  Csv.Arena = Arena;
  Csv.BackBuffer = ArenaPushWithFlags(Arena, F_FileLength(&Csv.File), MEM_COMMIT, PAGE_READWRITE );

  Csv.CurrentRow    = 0;
  Csv.CurrentColumn = 0;
  Csv.NColumns = N_Columns;
  Csv.NRows = MAX_CSV_ROWS;
  
  Csv.Rows.data = PushArray(Arena, u8, gigabyte(4));
  Csv.Rows.len = gigabyte(4);
  Csv.Rows.idx = 0;

  Csv.Delimiter = ',';

  return Csv;
}

fn_internal void CSV_SetDelimiter(csv_encoder* Csv, char Delimiter) {
  Csv->Delimiter = Delimiter;
}

fn_internal void CSV_SetTitleNames(csv_encoder* Csv, const char* names[]) {
  for (u32 i = 0; i < Csv->NColumns; i++) {
    CustomStrcpy(Csv->ColumnNames[i], names[i]);
  }

  for (u64 i = 0; i < Csv->NColumns; i++) {
    StringAppend(&Csv->Rows, Csv->ColumnNames[i]);
    
        
    // Write delimiter only if it's NOT the last column
    if (i < Csv->NColumns - 1) { 
      StringAppend(&Csv->Rows, &Csv->Delimiter);
    }
  }
  // Write a newline after the headers
  StringAppend(&Csv->Rows, "\n"); 
}

fn_internal void CSV_BeginRow(csv_encoder* Csv) {
  Csv->CurrentColumn = 0;
}

fn_internal void CSV_EndRow(csv_encoder* Csv) {
  if (Csv->CurrentColumn >= Csv->NColumns + 1) {
    fprintf(stderr, "[ERROR] Appended too many columns to row: %d\n", Csv->CurrentRow);
  }
  // Write the newline at the end of the row
  StringAppend(&Csv->Rows, "\n");

  Csv->CurrentRow += 1;
  Csv->CurrentColumn = 0;
}

fn_internal void CSV_PushValue(csv_encoder* Csv, const char* Value) {
  StringAppend(&Csv->Rows, Value);
  if (Csv->CurrentColumn < Csv->NColumns - 1) { 
    StringAppend(&Csv->Rows, &Csv->Delimiter);
  }
  Csv->CurrentColumn += 1;
}

fn_internal void CSV_WriteToFile(csv_encoder* Csv) {

  F_FileWrite(&Csv->File, Csv->Rows.data, Csv->Rows.idx);
}

fn_internal void CSV_Destroy(csv_encoder* Csv) {

  F_CloseFile(&Csv->File);
}
