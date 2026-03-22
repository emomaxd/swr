#ifndef OBJ_LOADER_H
#define OBJ_LOADER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core.h"

/*
 * Simple OBJ file loader.
 * Supports: v (positions), vt (texcoords), vn (normals), f (faces with triangulation)
 * Face formats: v, v/vt, v/vt/vn, v//vn
 */

/* Temporary storage during parsing */
struct obj_parse_data
{
    vec3 *positions;
    vec2 *texcoords;
    vec3 *normals;
    int posCount, texCount, normCount;
    int posCap, texCap, normCap;

    vertex3d *vertices;
    triangle3d *triangles;
    int vertCount, triCount;
    int vertCap, triCap;
};

static void obj_grow_positions(obj_parse_data *d) {
    d->posCap = d->posCap ? d->posCap * 2 : 256;
    d->positions = (vec3 *)realloc(d->positions, d->posCap * sizeof(vec3));
}
static void obj_grow_texcoords(obj_parse_data *d) {
    d->texCap = d->texCap ? d->texCap * 2 : 256;
    d->texcoords = (vec2 *)realloc(d->texcoords, d->texCap * sizeof(vec2));
}
static void obj_grow_normals(obj_parse_data *d) {
    d->normCap = d->normCap ? d->normCap * 2 : 256;
    d->normals = (vec3 *)realloc(d->normals, d->normCap * sizeof(vec3));
}
static void obj_grow_vertices(obj_parse_data *d) {
    d->vertCap = d->vertCap ? d->vertCap * 2 : 256;
    d->vertices = (vertex3d *)realloc(d->vertices, d->vertCap * sizeof(vertex3d));
}
static void obj_grow_triangles(obj_parse_data *d) {
    d->triCap = d->triCap ? d->triCap * 2 : 256;
    d->triangles = (triangle3d *)realloc(d->triangles, d->triCap * sizeof(triangle3d));
}

/* Parse a face vertex index group like "1/2/3", "1//3", "1/2", "1" */
static void obj_parse_face_vertex(const char *str, int *vi, int *ti, int *ni) {
    *vi = 0; *ti = 0; *ni = 0;

    *vi = atoi(str);

    const char *slash1 = strchr(str, '/');
    if (!slash1) return;

    if (slash1[1] != '/') {
        *ti = atoi(slash1 + 1);
    }

    const char *slash2 = strchr(slash1 + 1, '/');
    if (slash2) {
        *ni = atoi(slash2 + 1);
    }
}

static int obj_add_vertex(obj_parse_data *d, vec3 pos, vec2 tc, vec3 norm) {
    if (d->vertCount >= d->vertCap) obj_grow_vertices(d);
    d->vertices[d->vertCount].position = pos;
    d->vertices[d->vertCount].texcoord = tc;
    d->vertices[d->vertCount].normal = norm;
    return d->vertCount++;
}

static void obj_add_triangle(obj_parse_data *d, int v0, int v1, int v2) {
    if (d->triCount >= d->triCap) obj_grow_triangles(d);
    d->triangles[d->triCount].v[0] = v0;
    d->triangles[d->triCount].v[1] = v1;
    d->triangles[d->triCount].v[2] = v2;
    d->triCount++;
}

static mesh LoadOBJ(const char *filename) {
    mesh result = {};

    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "[ERROR] Cannot open OBJ file: %s\n", filename);
        return result;
    }

    obj_parse_data d = {};

    char line[512];
    while (fgets(line, sizeof(line), file)) {
        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        if (line[0] == 'v' && line[1] == ' ') {
            /* Vertex position */
            vec3 p;
            sscanf(line + 2, "%f %f %f", &p.x, &p.y, &p.z);
            if (d.posCount >= d.posCap) obj_grow_positions(&d);
            d.positions[d.posCount++] = p;
        }
        else if (line[0] == 'v' && line[1] == 't') {
            /* Texture coordinate */
            vec2 t;
            sscanf(line + 3, "%f %f", &t.x, &t.y);
            if (d.texCount >= d.texCap) obj_grow_texcoords(&d);
            d.texcoords[d.texCount++] = t;
        }
        else if (line[0] == 'v' && line[1] == 'n') {
            /* Normal */
            vec3 n;
            sscanf(line + 3, "%f %f %f", &n.x, &n.y, &n.z);
            if (d.normCount >= d.normCap) obj_grow_normals(&d);
            d.normals[d.normCount++] = n;
        }
        else if (line[0] == 'f' && line[1] == ' ') {
            /* Face - triangulate polygon (fan triangulation) */
            char *tokens[32];
            int tokenCount = 0;

            char *ptr = line + 2;
            while (*ptr && tokenCount < 32) {
                while (*ptr == ' ' || *ptr == '\t') ptr++;
                if (*ptr == '\0' || *ptr == '\n' || *ptr == '\r') break;
                tokens[tokenCount++] = ptr;
                while (*ptr && *ptr != ' ' && *ptr != '\t' && *ptr != '\n' && *ptr != '\r') ptr++;
                if (*ptr) *ptr++ = '\0';
            }

            if (tokenCount < 3) continue;

            /* Parse all face vertices */
            int faceVerts[32];
            for (int i = 0; i < tokenCount; i++) {
                int vi, ti, ni;
                obj_parse_face_vertex(tokens[i], &vi, &ti, &ni);

                vec3 pos = {0, 0, 0};
                vec2 tc = {0, 0};
                vec3 norm = {0, 0, 0};

                if (vi > 0 && vi <= d.posCount) pos = d.positions[vi - 1];
                else if (vi < 0) pos = d.positions[d.posCount + vi];

                if (ti > 0 && ti <= d.texCount) tc = d.texcoords[ti - 1];
                else if (ti < 0) tc = d.texcoords[d.texCount + ti];

                if (ni > 0 && ni <= d.normCount) norm = d.normals[ni - 1];
                else if (ni < 0) norm = d.normals[d.normCount + ni];

                faceVerts[i] = obj_add_vertex(&d, pos, tc, norm);
            }

            /* Fan triangulation for polygons */
            for (int i = 1; i < tokenCount - 1; i++) {
                obj_add_triangle(&d, faceVerts[0], faceVerts[i], faceVerts[i + 1]);
            }
        }
    }

    fclose(file);

    result.vertices = d.vertices;
    result.vertexCount = d.vertCount;
    result.triangles = d.triangles;
    result.triangleCount = d.triCount;

    /* Free temporary arrays */
    free(d.positions);
    free(d.texcoords);
    free(d.normals);

    printf("[INFO] Loaded OBJ: %d vertices, %d triangles\n", result.vertexCount, result.triangleCount);

    return result;
}

static void FreeMesh(mesh *m) {
    free(m->vertices);
    free(m->triangles);
    m->vertices = NULL;
    m->triangles = NULL;
    m->vertexCount = 0;
    m->triangleCount = 0;
}

#endif // OBJ_LOADER_H
