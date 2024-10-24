#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "core.h"

static u32 GetTotalPixelSize(image_u32 Image) {
    return Image.Width * Image.Height * sizeof(u32);
}

static image_u32 AllocateImage(u32 Width, u32 Height) {
    image_u32 Image = {};
    Image.Width = Width;
    Image.Height = Height;
    Image.Pixels = (u32 *)malloc(GetTotalPixelSize(Image));
    return Image;
}

static void ClearImage(image_u32 Image, u32 Color) {
    for (u32 i = 0; i < Image.Width * Image.Height; ++i) {
        Image.Pixels[i] = Color;
    }
}

static void
WriteImage(image_u32 Image, char *FileName)
{
    u32 OutputPixelSize = GetTotalPixelSize(Image);

    bitmap_header Header = {};
    Header.FileType = 0x4D42;
    Header.FileSize = sizeof(Header) + OutputPixelSize;
    Header.Reserved1 = 0;
    Header.Reserved2 = 0;
    Header.BitmapOffset = sizeof(Header);
    Header.Size = sizeof(Header) - 14;
    Header.Width = Image.Width;
    Header.Height = Image.Height;
    Header.Planes = 1;
    Header.BitsPerPixel = 32;
    Header.Compression = 0;
    Header.SizeOfBitmap = OutputPixelSize;
    Header.HorzResolution = 0;
    Header.VertResolution = 0;
    Header.ColorsUsed = 0;
    Header.ColorsImportant = 0;

    FILE *OutFile = fopen(FileName, "wb");
    if(OutFile)
    {
        fwrite(&Header, sizeof(Header), 1, OutFile);
        fwrite(Image.Pixels, OutputPixelSize, 1, OutFile);
        fclose(OutFile);
    }
    else
    {
        fprintf(stderr, "[ERROR] Unable to write output file %s.\n",FileName);
    }


}


static void RasterizeTriangle(image_u32 Image, vec2 v0, vec2 v1, vec2 v2, u32 Color) {
    // Find the bounding box of the triangle
    int minX = (int)fminf(fminf(v0.x, v1.x), v2.x);
    int maxX = (int)fmaxf(fmaxf(v0.x, v1.x), v2.x);
    int minY = (int)fminf(fminf(v0.y, v1.y), v2.y);
    int maxY = (int)fmaxf(fmaxf(v0.y, v1.y), v2.y);

    // Clamp to image bounds
    minX = fmax(minX, 0);
    maxX = fmin(maxX, Image.Width - 1);
    minY = fmax(minY, 0);
    maxY = fmin(maxY, Image.Height - 1);

    // Rasterize the triangle
    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            // Barycentric coordinates
            float area = 0.5f * (-v1.y * v2.x + v0.y * (-v1.x + v2.x) + v0.x * (v1.x - v2.x) + v1.x * v2.y);
            float s = 1.0f / (2.0f * area) * (v0.y * v2.x - v0.x * v2.y + (v2.y - v0.y) * x + (v0.x - v2.x) * y);
            float t = 1.0f / (2.0f * area) * (v0.x * v1.y - v0.y * v1.x + (v0.y - v1.y) * x + (v1.x - v0.x) * y);
            
            if (s >= 0 && t >= 0 && (s + t) <= 1) {
                Image.Pixels[y * Image.Width + x] = Color;
            }
        }
    }
}

int main() {
    image_u32 Image = AllocateImage(800, 600);
    ClearImage(Image, 0xFFFFFFFF); // Clear to white

    vec2 v0 = {200.0f, 100.0f};
    vec2 v1 = {600.0f, 300.0f};
    vec2 v2 = {300.0f, 500.0f};

    RasterizeTriangle(Image, v0, v1, v2, 0xFFFF0000); // A-R-G-B

    WriteImage(Image, "output.bmp");

    free(Image.Pixels);

    return 0;
}
