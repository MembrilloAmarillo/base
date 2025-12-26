#ifndef _OBJ_ENCODING_H_
#define _OBJ_ENCODING_H_

#include "../types.h"
#include "../vector.h"

typedef struct obj_instance obj_instance;
struct obj_instance {
  dyn_vector<vec4> Vec4Vertices;      // List of geometric vertices, with (x, y, z, [w]) coordinates, w is optional and defaults to 1.0.
  vector Vec3TexCoords;     // List of texture coordinates, in (u, [v, w]) coordinates, these will vary between 0 and 1. v, w are optional and default to 0.
  vector Vec3VertexNormals; // List of vertex normals in (x,y,z) form; normals might not be unit vectors.
  vector Vec3SpaceVertices; // Parameter space vertices in (u, [v, w]) form; free form geometry statement
  vector Vec3PolyFaces;     // Polygonal face element
};

/*
  Face elements structures:
  f 1 2 3
  f 3/1 4/2 5/3
  f 6/4/1 3/5/3 7/6/5
  f 7//1 8//2 9//3
*/

typedef enum {
  OBJ_NO_TEX, 
  OBJ_NO_NORMALS,
  OBJ_NO_SPACE_VERTICES,
  OBJ_NO_POLYGONAL_FACES
}obj_load_flags;

fn_internal obj_instance OBJ_InstanceInit(const char* path, obj_load_flags flags, Arena* Allocator);

#endif //_OBJ_ENCODING_H_