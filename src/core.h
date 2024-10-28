#ifndef CORE_H
#define CORE_H

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
    float x; // X coordinate
    float y; // Y coordinate
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

#endif // CORE_H
