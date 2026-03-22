#ifndef CORE_H
#define CORE_H

#include <math.h>

typedef char unsigned u8;
typedef short unsigned u16;
typedef int unsigned u32;

typedef char s8;
typedef short s16;
typedef int s32;
typedef float f32;


#pragma pack(push, 1)
struct bitmap_header
{
    u16 FileType;
    u32 FileSize;
    u16 Reserved1;
    u16 Reserved2;
    u32 BitmapOffset;
    u32 Size;
    s32 Width;
    s32 Height;
    u16 Planes;
    u16 BitsPerPixel;
    u32 Compression;
    u32 SizeOfBitmap;
    s32 HorzResolution;
    s32 VertResolution;
    u32 ColorsUsed;
    u32 ColorsImportant;
};
#pragma pack(pop)

struct image_u32
{
    u32 Width;
    u32 Height;
    u32 *Pixels;
};

typedef struct {
    float x;
    float y;
} vec2;

typedef struct {
    float x, y, z;
} vec3;

typedef struct {
    float x, y, z, w;
} vec4;

typedef struct {
    float m[4][4];
} mat4;

/* Depth buffer for z-buffering */
struct depth_buffer
{
    u32 Width;
    u32 Height;
    float *Data;
};

/* Texture image */
struct texture
{
    u32 Width;
    u32 Height;
    u32 *Pixels;
};

/* 3D vertex with position, texture coordinates, and normal */
struct vertex3d
{
    vec3 position;
    vec2 texcoord;
    vec3 normal;
};

/* Triangle defined by 3 vertex indices */
struct triangle3d
{
    int v[3];
};

/* Simple dynamic array for OBJ loading */
struct mesh
{
    vertex3d *vertices;
    int vertexCount;
    triangle3d *triangles;
    int triangleCount;
};

/* vec3 helpers */
static inline vec3 Vec3Sub(vec3 a, vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
static inline vec3 Vec3Add(vec3 a, vec3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
static inline vec3 Vec3Scale(vec3 v, float s) {
    return {v.x * s, v.y * s, v.z * s};
}
static inline float Vec3Dot(vec3 a, vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
static inline vec3 Vec3Cross(vec3 a, vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
static inline float Vec3Length(vec3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}
static inline vec3 Vec3Normalize(vec3 v) {
    float len = Vec3Length(v);
    if (len > 0.0001f) return Vec3Scale(v, 1.0f / len);
    return {0, 0, 0};
}

#endif // CORE_H
