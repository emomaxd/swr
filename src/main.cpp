#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "core.h"
#include "obj_loader.h"

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
WriteImage(image_u32 Image, const char *FileName)
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

/* ============================================================
 * Depth buffer
 * ============================================================ */

static depth_buffer AllocateDepthBuffer(u32 Width, u32 Height) {
    depth_buffer db = {};
    db.Width = Width;
    db.Height = Height;
    db.Data = (float *)malloc(Width * Height * sizeof(float));
    return db;
}

static void ClearDepthBuffer(depth_buffer db) {
    for (u32 i = 0; i < db.Width * db.Height; ++i) {
        db.Data[i] = FLT_MAX;
    }
}

/* ============================================================
 * Texture
 * ============================================================ */

static texture GenerateCheckerTexture(u32 Width, u32 Height, u32 CellSize, u32 Color1, u32 Color2) {
    texture tex = {};
    tex.Width = Width;
    tex.Height = Height;
    tex.Pixels = (u32 *)malloc(Width * Height * sizeof(u32));

    for (u32 y = 0; y < Height; y++) {
        for (u32 x = 0; x < Width; x++) {
            bool checker = ((x / CellSize) + (y / CellSize)) % 2 == 0;
            tex.Pixels[y * Width + x] = checker ? Color1 : Color2;
        }
    }
    return tex;
}

static u32 SampleTexture(texture tex, float u, float v) {
    /* Wrap UV coordinates to [0, 1) */
    u = u - floorf(u);
    v = v - floorf(v);

    int tx = (int)(u * (tex.Width - 1));
    int ty = (int)(v * (tex.Height - 1));

    tx = tx < 0 ? 0 : (tx >= (int)tex.Width ? (int)tex.Width - 1 : tx);
    ty = ty < 0 ? 0 : (ty >= (int)tex.Height ? (int)tex.Height - 1 : ty);

    return tex.Pixels[ty * tex.Width + tx];
}

static texture LoadBMPTexture(const char *filename) {
    texture tex = {};

    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "[WARN] Cannot open texture: %s, using fallback\n", filename);
        return GenerateCheckerTexture(64, 64, 8, 0xFFFFFFFF, 0xFF808080);
    }

    bitmap_header header;
    fread(&header, sizeof(header), 1, f);

    if (header.FileType != 0x4D42 || header.BitsPerPixel != 32) {
        fprintf(stderr, "[WARN] Unsupported BMP format in %s, using fallback\n", filename);
        fclose(f);
        return GenerateCheckerTexture(64, 64, 8, 0xFFFFFFFF, 0xFF808080);
    }

    tex.Width = header.Width;
    tex.Height = abs(header.Height);
    tex.Pixels = (u32 *)malloc(tex.Width * tex.Height * sizeof(u32));

    fseek(f, header.BitmapOffset, SEEK_SET);
    fread(tex.Pixels, sizeof(u32), tex.Width * tex.Height, f);
    fclose(f);

    printf("[INFO] Loaded texture: %s (%ux%u)\n", filename, tex.Width, tex.Height);
    return tex;
}

/* ============================================================
 * Matrix operations (fixed for column-vector convention: M * v)
 * Layout: m[row][col]
 * ============================================================ */

static mat4 IdentityMatrix() {
    mat4 result = {};
    for (int i = 0; i < 4; ++i) {
        result.m[i][i] = 1.0f;
    }
    return result;
}

static mat4 TranslationMatrix(float x, float y, float z) {
    mat4 result = IdentityMatrix();
    result.m[0][3] = x;
    result.m[1][3] = y;
    result.m[2][3] = z;
    return result;
}

static mat4 ScalingMatrix(float x, float y, float z) {
    mat4 result = {};
    result.m[0][0] = x;
    result.m[1][1] = y;
    result.m[2][2] = z;
    result.m[3][3] = 1.0f;
    return result;
}

static mat4 RotationXMatrix(float angle) {
    mat4 result = IdentityMatrix();
    float c = cosf(angle);
    float s = sinf(angle);
    result.m[1][1] = c;
    result.m[1][2] = -s;
    result.m[2][1] = s;
    result.m[2][2] = c;
    return result;
}

static mat4 RotationYMatrix(float angle) {
    mat4 result = IdentityMatrix();
    float c = cosf(angle);
    float s = sinf(angle);
    result.m[0][0] = c;
    result.m[0][2] = s;
    result.m[2][0] = -s;
    result.m[2][2] = c;
    return result;
}

static mat4 RotationZMatrix(float angle) {
    mat4 result = IdentityMatrix();
    float c = cosf(angle);
    float s = sinf(angle);
    result.m[0][0] = c;
    result.m[0][1] = -s;
    result.m[1][0] = s;
    result.m[1][1] = c;
    return result;
}

static mat4 PerspectiveMatrix(float fov, float aspect, float nearPlane, float farPlane) {
    mat4 result = {};
    float f = 1.0f / tanf(fov / 2.0f);

    result.m[0][0] = f / aspect;
    result.m[1][1] = f;
    result.m[2][2] = (farPlane + nearPlane) / (nearPlane - farPlane);
    result.m[2][3] = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
    result.m[3][2] = -1.0f;

    return result;
}

static mat4 LookAtMatrix(vec3 eye, vec3 target, vec3 up) {
    vec3 f = Vec3Normalize(Vec3Sub(target, eye));
    vec3 r = Vec3Normalize(Vec3Cross(f, up));
    vec3 u = Vec3Cross(r, f);

    mat4 result = {};
    result.m[0][0] = r.x;   result.m[0][1] = r.y;   result.m[0][2] = r.z;   result.m[0][3] = -Vec3Dot(r, eye);
    result.m[1][0] = u.x;   result.m[1][1] = u.y;   result.m[1][2] = u.z;   result.m[1][3] = -Vec3Dot(u, eye);
    result.m[2][0] = -f.x;  result.m[2][1] = -f.y;  result.m[2][2] = -f.z;  result.m[2][3] =  Vec3Dot(f, eye);
    result.m[3][3] = 1.0f;

    return result;
}

static vec4 MultiplyMatrixAndVector(mat4 mat, vec4 v) {
    vec4 result;
    result.x = mat.m[0][0] * v.x + mat.m[0][1] * v.y + mat.m[0][2] * v.z + mat.m[0][3] * v.w;
    result.y = mat.m[1][0] * v.x + mat.m[1][1] * v.y + mat.m[1][2] * v.z + mat.m[1][3] * v.w;
    result.z = mat.m[2][0] * v.x + mat.m[2][1] * v.y + mat.m[2][2] * v.z + mat.m[2][3] * v.w;
    result.w = mat.m[3][0] * v.x + mat.m[3][1] * v.y + mat.m[3][2] * v.z + mat.m[3][3] * v.w;
    return result;
}

static mat4 MultiplyMatrices(mat4 a, mat4 b) {
    mat4 result = {};

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

/* ============================================================
 * Edge functions & fill rule
 * ============================================================ */

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

/* ============================================================
 * 2D triangle rasterizer (original, kept for backwards compat)
 * ============================================================ */

void RasterizeTriangle(image_u32 Image, vec2 v0, vec2 v1, vec2 v2, u32 c0, u32 c1, u32 c2) {
    int minX = (int)fminf(fminf(v0.x, v1.x), v2.x);
    int maxX = (int)fmaxf(fmaxf(v0.x, v1.x), v2.x);
    int minY = (int)fminf(fminf(v0.y, v1.y), v2.y);
    int maxY = (int)fmaxf(fmaxf(v0.y, v1.y), v2.y);

    minX = (int)fmaxf((float)minX, 0.0f);
    maxX = (int)fminf((float)maxX, (float)(Image.Width - 1));
    minY = (int)fmaxf((float)minY, 0.0f);
    maxY = (int)fminf((float)maxY, (float)(Image.Height - 1));

    float delta_w0_col = (v1.y - v2.y);
    float delta_w1_col = (v2.y - v0.y);
    float delta_w2_col = (v0.y - v1.y);
    float delta_w0_row = (v2.x - v1.x);
    float delta_w1_row = (v0.x - v2.x);
    float delta_w2_row = (v1.x - v0.x);

    float bias0 = is_top_left(v1, v2) ? 0 : -0.0001f;
    float bias1 = is_top_left(v2, v0) ? 0 : -0.0001f;
    float bias2 = is_top_left(v0, v1) ? 0 : -0.0001f;

    vec2 p0 = { minX + 0.5f , minY + 0.5f };
    float w0_row = edge_cross(v1, v2, p0) + bias0;
    float w1_row = edge_cross(v2, v0, p0) + bias1;
    float w2_row = edge_cross(v0, v1, p0) + bias2;

    float totalArea = (v1.x - v0.x) * (v2.y - v0.y) - (v2.x - v0.x) * (v1.y - v0.y);
    float invTotalArea = 1.0f / totalArea;

    for (int y = minY; y <= maxY; y++) {
        float w0 = w0_row;
        float w1 = w1_row;
        float w2 = w2_row;
        for (int x = minX; x <= maxX; x++) {
            bool is_inside = w0 >= 0 && w1 >= 0 && w2 >= 0;
            if (is_inside)
            {
                float alpha = w0 * invTotalArea;
                float beta  = w1 * invTotalArea;
                float gamma = 1.0f - alpha - beta;

                int a = (int)(ALPHA(c0) * alpha + ALPHA(c1) * beta + ALPHA(c2) * gamma);
                int r = (int)(RED(c0) * alpha + RED(c1) * beta + RED(c2) * gamma);
                int g = (int)(GREEN(c0) * alpha + GREEN(c1) * beta + GREEN(c2) * gamma);
                int b = (int)(BLUE(c0) * alpha + BLUE(c1) * beta + BLUE(c2) * gamma);

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

/* ============================================================
 * 3D triangle rasterizer with:
 *   - Depth buffering (Z-buffer)
 *   - Perspective-correct texture mapping
 *   - Simple directional lighting
 * ============================================================ */

void RasterizeTriangle3D(image_u32 Image, depth_buffer DepthBuf,
                         vec4 clip0, vec4 clip1, vec4 clip2,
                         vec2 uv0, vec2 uv1, vec2 uv2,
                         vec3 n0, vec3 n1, vec3 n2,
                         texture *tex, vec3 lightDir)
{
    /* Perspective divide: clip space -> NDC */
    float inv_w0 = 1.0f / clip0.w;
    float inv_w1 = 1.0f / clip1.w;
    float inv_w2 = 1.0f / clip2.w;

    vec3 ndc0 = { clip0.x * inv_w0, clip0.y * inv_w0, clip0.z * inv_w0 };
    vec3 ndc1 = { clip1.x * inv_w1, clip1.y * inv_w1, clip1.z * inv_w1 };
    vec3 ndc2 = { clip2.x * inv_w2, clip2.y * inv_w2, clip2.z * inv_w2 };

    /* NDC -> Screen coordinates */
    float halfW = Image.Width * 0.5f;
    float halfH = Image.Height * 0.5f;

    vec2 s0 = { (ndc0.x + 1.0f) * halfW, (ndc0.y + 1.0f) * halfH };
    vec2 s1 = { (ndc1.x + 1.0f) * halfW, (ndc1.y + 1.0f) * halfH };
    vec2 s2 = { (ndc2.x + 1.0f) * halfW, (ndc2.y + 1.0f) * halfH };

    /* Bounding box */
    int minX = (int)fminf(fminf(s0.x, s1.x), s2.x);
    int maxX = (int)fmaxf(fmaxf(s0.x, s1.x), s2.x);
    int minY = (int)fminf(fminf(s0.y, s1.y), s2.y);
    int maxY = (int)fmaxf(fmaxf(s0.y, s1.y), s2.y);

    minX = (int)fmaxf((float)minX, 0.0f);
    maxX = (int)fminf((float)maxX, (float)(Image.Width - 1));
    minY = (int)fmaxf((float)minY, 0.0f);
    maxY = (int)fminf((float)maxY, (float)(Image.Height - 1));

    /* Edge function deltas for incremental traversal */
    float delta_w0_col = (s1.y - s2.y);
    float delta_w1_col = (s2.y - s0.y);
    float delta_w2_col = (s0.y - s1.y);
    float delta_w0_row = (s2.x - s1.x);
    float delta_w1_row = (s0.x - s2.x);
    float delta_w2_row = (s1.x - s0.x);

    float bias0 = is_top_left(s1, s2) ? 0 : -0.0001f;
    float bias1 = is_top_left(s2, s0) ? 0 : -0.0001f;
    float bias2 = is_top_left(s0, s1) ? 0 : -0.0001f;

    vec2 p0 = { minX + 0.5f, minY + 0.5f };
    float w0_row = edge_cross(s1, s2, p0) + bias0;
    float w1_row = edge_cross(s2, s0, p0) + bias1;
    float w2_row = edge_cross(s0, s1, p0) + bias2;

    float totalArea = (s1.x - s0.x) * (s2.y - s0.y) - (s2.x - s0.x) * (s1.y - s0.y);
    if (totalArea <= 0) return; /* Back-face culling */
    float invTotalArea = 1.0f / totalArea;

    /* Pre-divide texture coords and depth by w for perspective-correct interpolation */
    float u0_over_w = uv0.x * inv_w0, v0_over_w = uv0.y * inv_w0;
    float u1_over_w = uv1.x * inv_w1, v1_over_w = uv1.y * inv_w1;
    float u2_over_w = uv2.x * inv_w2, v2_over_w = uv2.y * inv_w2;

    float z0_over_w = ndc0.z * inv_w0, z1_over_w = ndc1.z * inv_w1, z2_over_w = ndc2.z * inv_w2;

    for (int y = minY; y <= maxY; y++) {
        float w0 = w0_row;
        float w1 = w1_row;
        float w2 = w2_row;

        for (int x = minX; x <= maxX; x++) {
            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                float alpha = w0 * invTotalArea;
                float beta  = w1 * invTotalArea;
                float gamma = 1.0f - alpha - beta;

                /* Perspective-correct interpolation */
                float one_over_w = alpha * inv_w0 + beta * inv_w1 + gamma * inv_w2;
                float w_correct = 1.0f / one_over_w;

                /* Interpolate depth */
                float depth = (alpha * z0_over_w + beta * z1_over_w + gamma * z2_over_w) * w_correct;

                /* Z-buffer test */
                int pixelIndex = y * Image.Width + x;
                if (depth < DepthBuf.Data[pixelIndex]) {
                    DepthBuf.Data[pixelIndex] = depth;

                    u32 color;
                    if (tex) {
                        /* Perspective-correct texture coordinates */
                        float u = (alpha * u0_over_w + beta * u1_over_w + gamma * u2_over_w) * w_correct;
                        float v = (alpha * v0_over_w + beta * v1_over_w + gamma * v2_over_w) * w_correct;

                        color = SampleTexture(*tex, u, v);
                    } else {
                        color = 0xFFFFFFFF;
                    }

                    /* Interpolate normal for lighting */
                    vec3 normal;
                    normal.x = alpha * n0.x + beta * n1.x + gamma * n2.x;
                    normal.y = alpha * n0.y + beta * n1.y + gamma * n2.y;
                    normal.z = alpha * n0.z + beta * n1.z + gamma * n2.z;
                    normal = Vec3Normalize(normal);

                    /* Simple diffuse lighting */
                    float ndotl = Vec3Dot(normal, lightDir);
                    float diffuse = fmaxf(ndotl, 0.0f);
                    float ambient = 0.15f;
                    float intensity = fminf(ambient + diffuse * 0.85f, 1.0f);

                    int r = (int)(RED(color) * intensity);
                    int g = (int)(GREEN(color) * intensity);
                    int b = (int)(BLUE(color) * intensity);

                    Image.Pixels[pixelIndex] = PackColor(255, r, g, b);
                }
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

/* ============================================================
 * 3D rendering pipeline
 * ============================================================ */

static void RenderMesh(image_u32 Image, depth_buffer DepthBuf,
                       mesh *m, mat4 mvp, texture *tex, vec3 lightDir)
{
    for (int i = 0; i < m->triangleCount; i++) {
        triangle3d tri = m->triangles[i];
        vertex3d v0 = m->vertices[tri.v[0]];
        vertex3d v1 = m->vertices[tri.v[1]];
        vertex3d v2 = m->vertices[tri.v[2]];

        vec4 p0 = {v0.position.x, v0.position.y, v0.position.z, 1.0f};
        vec4 p1 = {v1.position.x, v1.position.y, v1.position.z, 1.0f};
        vec4 p2 = {v2.position.x, v2.position.y, v2.position.z, 1.0f};

        vec4 clip0 = MultiplyMatrixAndVector(mvp, p0);
        vec4 clip1 = MultiplyMatrixAndVector(mvp, p1);
        vec4 clip2 = MultiplyMatrixAndVector(mvp, p2);

        /* Simple near-plane clipping: skip triangles behind camera */
        if (clip0.w < 0.001f && clip1.w < 0.001f && clip2.w < 0.001f) continue;
        if (clip0.w < 0.001f || clip1.w < 0.001f || clip2.w < 0.001f) continue;

        RasterizeTriangle3D(Image, DepthBuf,
                            clip0, clip1, clip2,
                            v0.texcoord, v1.texcoord, v2.texcoord,
                            v0.normal, v1.normal, v2.normal,
                            tex, lightDir);
    }
}

/* ============================================================
 * Built-in cube mesh for demo
 * ============================================================ */

static mesh CreateCubeMesh() {
    mesh m = {};
    m.vertexCount = 36;
    m.vertices = (vertex3d *)malloc(m.vertexCount * sizeof(vertex3d));
    m.triangleCount = 12;
    m.triangles = (triangle3d *)malloc(m.triangleCount * sizeof(triangle3d));

    struct { vec3 p; vec2 t; vec3 n; } cubeData[] = {
        /* Front face (z = +1) */
        {{-1, -1,  1}, {0, 0}, { 0,  0,  1}},
        {{ 1, -1,  1}, {1, 0}, { 0,  0,  1}},
        {{ 1,  1,  1}, {1, 1}, { 0,  0,  1}},
        {{-1, -1,  1}, {0, 0}, { 0,  0,  1}},
        {{ 1,  1,  1}, {1, 1}, { 0,  0,  1}},
        {{-1,  1,  1}, {0, 1}, { 0,  0,  1}},
        /* Back face (z = -1) */
        {{ 1, -1, -1}, {0, 0}, { 0,  0, -1}},
        {{-1, -1, -1}, {1, 0}, { 0,  0, -1}},
        {{-1,  1, -1}, {1, 1}, { 0,  0, -1}},
        {{ 1, -1, -1}, {0, 0}, { 0,  0, -1}},
        {{-1,  1, -1}, {1, 1}, { 0,  0, -1}},
        {{ 1,  1, -1}, {0, 1}, { 0,  0, -1}},
        /* Top face (y = +1) */
        {{-1,  1,  1}, {0, 0}, { 0,  1,  0}},
        {{ 1,  1,  1}, {1, 0}, { 0,  1,  0}},
        {{ 1,  1, -1}, {1, 1}, { 0,  1,  0}},
        {{-1,  1,  1}, {0, 0}, { 0,  1,  0}},
        {{ 1,  1, -1}, {1, 1}, { 0,  1,  0}},
        {{-1,  1, -1}, {0, 1}, { 0,  1,  0}},
        /* Bottom face (y = -1) */
        {{-1, -1, -1}, {0, 0}, { 0, -1,  0}},
        {{ 1, -1, -1}, {1, 0}, { 0, -1,  0}},
        {{ 1, -1,  1}, {1, 1}, { 0, -1,  0}},
        {{-1, -1, -1}, {0, 0}, { 0, -1,  0}},
        {{ 1, -1,  1}, {1, 1}, { 0, -1,  0}},
        {{-1, -1,  1}, {0, 1}, { 0, -1,  0}},
        /* Right face (x = +1) */
        {{ 1, -1,  1}, {0, 0}, { 1,  0,  0}},
        {{ 1, -1, -1}, {1, 0}, { 1,  0,  0}},
        {{ 1,  1, -1}, {1, 1}, { 1,  0,  0}},
        {{ 1, -1,  1}, {0, 0}, { 1,  0,  0}},
        {{ 1,  1, -1}, {1, 1}, { 1,  0,  0}},
        {{ 1,  1,  1}, {0, 1}, { 1,  0,  0}},
        /* Left face (x = -1) */
        {{-1, -1, -1}, {0, 0}, {-1,  0,  0}},
        {{-1, -1,  1}, {1, 0}, {-1,  0,  0}},
        {{-1,  1,  1}, {1, 1}, {-1,  0,  0}},
        {{-1, -1, -1}, {0, 0}, {-1,  0,  0}},
        {{-1,  1,  1}, {1, 1}, {-1,  0,  0}},
        {{-1,  1, -1}, {0, 1}, {-1,  0,  0}},
    };

    for (int i = 0; i < 36; i++) {
        m.vertices[i].position = cubeData[i].p;
        m.vertices[i].texcoord = cubeData[i].t;
        m.vertices[i].normal = cubeData[i].n;
    }

    for (int i = 0; i < 12; i++) {
        m.triangles[i].v[0] = i * 3;
        m.triangles[i].v[1] = i * 3 + 1;
        m.triangles[i].v[2] = i * 3 + 2;
    }

    return m;
}

/* ============================================================
 * Main
 * ============================================================ */

int main(int argc, char **argv) {
    const u32 WIDTH = 800;
    const u32 HEIGHT = 600;

    image_u32 Image = AllocateImage(WIDTH, HEIGHT);
    depth_buffer DepthBuf = AllocateDepthBuffer(WIDTH, HEIGHT);

    ClearImage(Image, PackColor(255, 30, 30, 40));
    ClearDepthBuffer(DepthBuf);

    /* Generate checkerboard texture */
    texture checkerTex = GenerateCheckerTexture(256, 256, 32, 0xFF2288DD, 0xFFEEEEEE);

    /* Load mesh: from OBJ file or built-in cube */
    mesh cubeMesh;
    bool loadedOBJ = false;
    if (argc > 1) {
        cubeMesh = LoadOBJ(argv[1]);
        if (cubeMesh.triangleCount > 0) {
            loadedOBJ = true;
        }
    }
    if (!loadedOBJ) {
        cubeMesh = CreateCubeMesh();
        printf("[INFO] Using built-in cube mesh\n");
    }

    /* Load texture from BMP file if provided */
    texture *renderTex = &checkerTex;
    texture fileTex = {};
    if (argc > 2) {
        fileTex = LoadBMPTexture(argv[2]);
        renderTex = &fileTex;
    }

    /* Camera setup */
    vec3 eye = {3.0f, 2.5f, 3.0f};
    vec3 target = {0.0f, 0.0f, 0.0f};
    vec3 up = {0.0f, 1.0f, 0.0f};

    /* Build transform pipeline: Model -> View -> Projection */
    float fov = 60.0f * 3.14159265f / 180.0f;
    float aspect = (float)WIDTH / (float)HEIGHT;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;

    mat4 model = MultiplyMatrices(RotationYMatrix(0.5f), RotationXMatrix(0.3f));
    mat4 view = LookAtMatrix(eye, target, up);
    mat4 projection = PerspectiveMatrix(fov, aspect, nearPlane, farPlane);

    mat4 mv = MultiplyMatrices(view, model);
    mat4 mvp = MultiplyMatrices(projection, mv);

    /* Light direction (normalized, pointing from top-right-front) */
    vec3 lightDir = Vec3Normalize({0.5f, 0.8f, 0.6f});

    /* Render the mesh */
    RenderMesh(Image, DepthBuf, &cubeMesh, mvp, renderTex, lightDir);

    /* Also render original 2D triangle to show backward compatibility */
    vec2 tv0 = {50.0f, 50.0f};
    vec2 tv1 = {150.0f, 50.0f};
    vec2 tv2 = {100.0f, 150.0f};
    RasterizeTriangle(Image, tv0, tv1, tv2, 0xFFFF0000, 0xFF00FF00, 0xFF0000FF);

    WriteImage(Image, "output.bmp");
    printf("[INFO] Rendered to output.bmp (%ux%u)\n", WIDTH, HEIGHT);

    /* Cleanup */
    FreeMesh(&cubeMesh);
    free(Image.Pixels);
    free(DepthBuf.Data);
    free(checkerTex.Pixels);
    if (fileTex.Pixels) free(fileTex.Pixels);

    return 0;
}
