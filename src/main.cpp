#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "core.h"

/* Color unpacking macros */
#define RED(color)   ((color >> 16) & 0xFF)
#define GREEN(color) ((color >> 8) & 0xFF)
#define BLUE(color)  (color & 0xFF)
#define ALPHA(color) ((color >> 24) & 0xFF)

/* Pack color from ARGB components */
u32 PackColor(int a, int r, int g, int b) {
    return ((a & 0xFF) << 24) | ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF);
}

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

static mat4 IdentityMatrix() {
    mat4 result = {0};
    for (int i = 0; i < 4; ++i) {
        result.m[i][i] = 1.0f;
    }
    return result;
}

static mat4 TranslationMatrix(float x, float y, float z) {
    mat4 result = IdentityMatrix();
    result.m[3][0] = x;
    result.m[3][1] = y;
    result.m[3][2] = z;
    return result;
}

static mat4 ScalingMatrix(float x, float y, float z) {
    mat4 result = {0};
    result.m[0][0] = x;
    result.m[1][1] = y;
    result.m[2][2] = z;
    result.m[3][3] = 1.0f;
    return result;
}

static mat4 RotationXMatrix(float angle) {
    mat4 result = IdentityMatrix();
    float c = cos(angle);
    float s = sin(angle);
    result.m[1][1] = c;
    result.m[1][2] = -s;
    result.m[2][1] = s;
    result.m[2][2] = c;
    return result;
}

static mat4 RotationYMatrix(float angle) {
    mat4 result = IdentityMatrix();
    float c = cos(angle);
    float s = sin(angle);
    result.m[0][0] = c;
    result.m[0][2] = s;
    result.m[2][0] = -s;
    result.m[2][2] = c;
    return result;
}

static mat4 RotationZMatrix(float angle) {
    mat4 result = IdentityMatrix();
    float c = cos(angle);
    float s = sin(angle);
    result.m[0][0] = c;
    result.m[0][1] = -s;
    result.m[1][0] = s;
    result.m[1][1] = c;
    return result;
}

static mat4 PerspectiveMatrix(float fov, float aspect, float near, float far) {
    mat4 result = {0};
    float f = 1.0f / tan(fov / 2);
    
    result.m[0][0] = f / aspect;
    result.m[1][1] = f;
    result.m[2][2] = (far + near) / (near - far);
    result.m[2][3] = (2 * far * near) / (near - far);
    result.m[3][2] = -1;
    
    return result;
}

static vec4 MultiplyMatrixAndVector(mat4 mat, vec4 vec) {
    vec4 result = {0};
    for (int i = 0; i < 4; ++i) {
        result.x += mat.m[i][0] * vec.x;
        result.y += mat.m[i][1] * vec.x;
        result.z += mat.m[i][2] * vec.x;
        result.w += mat.m[i][3] * vec.x;
    }
    return result;
}

static mat4 MultiplyMatrices(mat4 a, mat4 b) {
    mat4 result = {0};
    
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result.m[i][j] = a.m[i][0] * b.m[0][j] +
                             a.m[i][1] * b.m[1][j] +
                             a.m[i][2] * b.m[2][j] +
                             a.m[i][3] * b.m[3][j];
        }
    }
    
    return result;
}

bool is_top_left(vec2 start, vec2 end) {
    vec2 edge = { end.x - start.x, end.y - start.y };
    bool is_top_edge = edge.y == 0 && edge.x > 0;
    bool is_left_edge = edge.y < 0;
    return is_left_edge || is_top_edge;
}

float edge_cross(vec2 a, vec2 b, vec2 p) {
    vec2 ab = { b.x - a.x, b.y - a.y };
    vec2 ap = { p.x - a.x, p.y - a.y };
    return ab.x * ap.y - ab.y * ap.x;
}

static void DrawLine(image_u32 Image, vec2 start, vec2 end, u32 Color) {
    int x0 = (int)start.x;
    int y0 = (int)start.y;
    int x1 = (int)end.x;
    int y1 = (int)end.y;

    // Calculate the differences and error
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1; // Step in x direction
    int sy = (y0 < y1) ? 1 : -1; // Step in y direction
    int err = dx - dy; // Error variable

    while (1) {
        // Set the pixel
        if (x0 >= 0 && x0 < Image.Width && y0 >= 0 && y0 < Image.Height) {
            Image.Pixels[y0 * Image.Width + x0] = Color;
        }

        // Check if we've reached the end point
        if (x0 == x1 && y0 == y1) break;

        // Calculate error and adjust coordinates
        int err2 = err * 2;
        if (err2 > -dy) { // Move x
            err -= dy;
            x0 += sx;
        }
        if (err2 < dx) { // Move y
            err += dx;
            y0 += sy;
        }
    }
}

void RasterizeTriangle(image_u32 Image, vec2 v0, vec2 v1, vec2 v2, u32 c0, u32 c1, u32 c2) {
    /* Calculate bounding box */
    int minX = (int)fminf(fminf(v0.x, v1.x), v2.x);
    int maxX = (int)fmaxf(fmaxf(v0.x, v1.x), v2.x);
    int minY = (int)fminf(fminf(v0.y, v1.y), v2.y);
    int maxY = (int)fmaxf(fmaxf(v0.y, v1.y), v2.y);

    /* Clamp to image bounds */
    minX = fmax(minX, 0);
    maxX = fmin(maxX, Image.Width - 1);
    minY = fmax(minY, 0);
    maxY = fmin(maxY, Image.Height - 1);
  
    // Compute the constant delta_s that will be used for the horizontal and vertical steps
    float delta_w0_col = (v1.y - v2.y);
    float delta_w1_col = (v2.y - v0.y);
    float delta_w2_col = (v0.y - v1.y);
    float delta_w0_row = (v2.x - v1.x);
    float delta_w1_row = (v0.x - v2.x);
    float delta_w2_row = (v1.x - v0.x);
    
    // Rasterization fill rule, not 100% precise due to floating point innacuracy
    float bias0 = is_top_left(v1, v2) ? 0 : -0.0001;
    float bias1 = is_top_left(v2, v0) ? 0 : -0.0001;
    float bias2 = is_top_left(v0, v1) ? 0 : -0.0001;
    
    vec2 p0 = { minX + 0.5f , minY + 0.5f };
    float w0_row = edge_cross(v1, v2, p0) + bias0;
    float w1_row = edge_cross(v2, v0, p0) + bias1;
    float w2_row = edge_cross(v0, v1, p0) + bias2;   
    
    /* Precompute total area of the triangle */
    float totalArea = (v1.x - v0.x) * (v2.y - v0.y) - (v2.x - v0.x) * (v1.y - v0.y);
    float invTotalArea = 1.0f / totalArea;

    /* Rasterize the triangle */
    for (int y = minY; y <= maxY; y++) {
        float w0 = w0_row;
        float w1 = w1_row;
        float w2 = w2_row;
        for (int x = minX; x <= maxX; x++) {
            bool is_inside = w0 >= 0 && w1 >= 0 && w2 >= 0;
            if (is_inside)
            {
                /* Calculate barycentric coordinates */
                float alpha = w0 * invTotalArea;
                float beta  = w1 * invTotalArea;
                float gamma = 1.0f - alpha - beta;

                /* Interpolate color using alpha, beta, gamma */
                int a = (int)(ALPHA(c0) * alpha + ALPHA(c1) * beta + ALPHA(c2) * gamma);
                int r = (int)(RED(c0) * alpha + RED(c1) * beta + RED(c2) * gamma);
                int g = (int)(GREEN(c0) * alpha + GREEN(c1) * beta + GREEN(c2) * gamma);
                int b = (int)(BLUE(c0) * alpha + BLUE(c1) * beta + BLUE(c2) * gamma);

                /* Pack interpolated color and set the pixel */
                Image.Pixels[y * Image.Width + x] = PackColor(a, r, g, b);
            }
            w0 += delta_w0_col;
            w1 += delta_w1_col;
            w2 += delta_w2_col;
        }
    w0_row += delta_w0_row;
    w1_row += delta_w1_row;
    w2_row += delta_w2_row;
    }
}


int main() {
    image_u32 Image = AllocateImage(800, 600);
    ClearImage(Image, 0xFFFFFFFF); // Clear to white

    vec2 v0 = {200.0f, 100.0f};
    vec2 v1 = {600.0f, 300.0f};
    vec2 v2 = {300.0f, 500.0f};

    RasterizeTriangle(Image, v0, v1, v2, 0xFFFF0000, 0xFF00FF00, 0xFF0000FF); // A-R-G-B

    WriteImage(Image, "output.bmp");

    free(Image.Pixels);

    return 0;
}
