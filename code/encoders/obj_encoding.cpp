fn_internal i64 OBJ_SeekLineEnd(const char* buffer, i64 idx) {
  i64 NewIdx = idx;

  for( char b = buffer[idx]; b != '\n' && b != EOF; b = buffer[NewIdx++] );

  return NewIdx;
}

fn_internal i64 OBJ_Peek(const char* buffer, i64 idx) {
  return buffer[idx+1];
}

fn_internal f32 OBJ_ParseFloat(const char* buffer, i64* idx) {
  i64 i = *idx;
    
  // 1. Skip leading whitespace
  while (buffer[i] == ' ') i++;

  // 2. Parse Sign
  f32 sign = 1.0f;
  if (buffer[i] == '-') { sign = -1.0f; i++; }
  else if (buffer[i] == '+') { i++; }

  // 3. Parse Integer Part
  f32 res = 0.0f;
  while (buffer[i] >= '0' && buffer[i] <= '9') {
    res = res * 10.0f + (f32)(buffer[i] - '0');
    i++;
  }

  // 4. Parse Fraction Part
  if (buffer[i] == '.') {
    i++;
    f32 place = 0.1f;
    while (buffer[i] >= '0' && buffer[i] <= '9') {
      res += (f32)(buffer[i] - '0') * place;
      place *= 0.1f;
      i++;
    }
  }

  // 5. Parse Exponent (Scientific Notation)
  if (buffer[i] == 'e' || buffer[i] == 'E') {
    i++;
    f32 expSign = 1.0f;
    if (buffer[i] == '-') { expSign = -1.0f; i++; }
    else if (buffer[i] == '+') { i++; }

    f32 exponent = 0.0f;
    while (buffer[i] >= '0' && buffer[i] <= '9') {
      exponent = exponent * 10.0f + (f32)(buffer[i] - '0');
      i++;
    }

    // Apply Exponent
    // (Using a loop is faster than powf() for typical OBJ small integers)
    f32 multiplier = 1.0f;
    while (exponent > 0.0f) { multiplier *= 10.0f; exponent -= 1.0f; }
        
    if (expSign < 0.0f) res /= multiplier;
    else                res *= multiplier;
  }

  *idx = i; // Update the external index offset
  return res * sign;
}

fn_internal obj_instance OBJ_InstanceInit(const char* path, obj_load_flags flags, Arena* arena) {
  obj_instance Instance = {};
  f_file ObjFile = F_OpenFile(path, RDONLY);

  i32 FileLength = F_FileLength(&ObjFile);

  i32 LineIt = 0;

  dyn_vector<vec4> 
    = dyn_vector<vec4>::Init(arena, mebibyte(25));

  u8* data = PushArray(arena, u8, FileLength);
  F_SetFileData(&ObjFile, data);
  F_FileRead(&ObjFile);

  for (i64 it = 0; it < FileLength; it++) {
    if (data[it] == '#') {
      it = OBJ_SeekLineEnd((const char*)data, it);
    } else if (data[it] == 'v') {
      if (OBJ_Peek((const char*)data, it) == ' ') {
        it += 2; // Skip "v "

        // 1. Always parse the geometry (X, Y, Z)
        f32 x = OBJ_ParseFloat((const char*)data, &it);
        f32 y = OBJ_ParseFloat((const char*)data, &it);
        f32 z = OBJ_ParseFloat((const char*)data, &it);
        
        // Defaults
        f32 w = 1.0f;
        f32 r = 1.0f, g = 1.0f, b = 1.0f; // White by default

        // 2. Parse any remaining values on the line
        f32 extras[4]; 
        i32 extra_count = 0;

        while (extra_count < 4) {
          // Manually skip spaces to check for Newline without consuming it
          while (data[it] == ' ' || data[it] == '\t') it++;
            
          // If we hit a newline or end of file, stop looking
          if (data[it] == '\n' || data[it] == '\r' || data[it] == '\0') break;

          // Otherwise, there is another number waiting
          extras[extra_count++] = OBJ_ParseFloat((const char*)data, &it);
        }

        // 3. Apply your Logic
        if (extra_count == 1) {
          // Case: v x y z w
          w = extras[0];
        } 
        else if (extra_count == 3) {
          // Case: v x y z r g b
          r = extras[0];
          g = extras[1];
          b = extras[2];
        }
        else if (extra_count == 4) {
          // Rare Case: v x y z w r g b
          w = extras[0];
          r = extras[1];
          g = extras[2];
          b = extras[3];
        }

        // Output or Store (Assuming you have a struct with color support)
        //fprintf(stdout, "[OBJ] V: %.3f %.3f %.3f | W: %.2f | C: %.2f %.2f %.2f\n", x, y, z, w, r, g, b);
        ObjVector.Append(Vec4New(x, y, z, w));
      } else if (OBJ_Peek((const char*)data, it) == 't') {
      } else if (OBJ_Peek((const char*)data, it) == 'n') {
      } else if (OBJ_Peek((const char*)data, it) == 'p') {
      }
    } else if (data[it] == 'f') {
    
    } 
    
    if (data[it] == '\n') {
      LineIt++;
    }
  }

  F_CloseFile(&ObjFile);

  Instance.Vec4Vertices = ObjVector;
  
  return Instance;
}