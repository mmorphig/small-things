#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#define DEG_TO_RAD(angle) ((angle) * M_PI / 180.0)
#define EPSILON 1e-6  // Precision threshold for duplicate removal

typedef struct {
    float x, y, z;
} Vertex;

typedef struct {
    Vertex v1, v2, v3;
} Triangle;

Vertex transform_vertex(Vertex v, float scale, float rx, float ry, float rz) {
    float rad_x = DEG_TO_RAD(rx);
    float rad_y = DEG_TO_RAD(ry);
    float rad_z = DEG_TO_RAD(rz);

    v.x *= scale;
    v.y *= scale;
    v.z *= scale;

    float y1 = v.y * cos(rad_x) - v.z * sin(rad_x);
    float z1 = v.y * sin(rad_x) + v.z * cos(rad_x);

    float x2 = v.x * cos(rad_y) + z1 * sin(rad_y);
    float z2 = -v.x * sin(rad_y) + z1 * cos(rad_y);

    float x3 = x2 * cos(rad_z) - y1 * sin(rad_z);
    float y3 = x2 * sin(rad_z) + y1 * cos(rad_z);

    return (Vertex){x3, y3, z2};
}

int float_equals(float a, float b) {
    return fabs(a - b) < EPSILON;
}

int vertices_equal(Vertex a, Vertex b) {
    return float_equals(a.x, b.x) && float_equals(a.y, b.y) && float_equals(a.z, b.z);
}

int triangles_equal(Triangle t1, Triangle t2) {
    Vertex t1_v[] = {t1.v1, t1.v2, t1.v3};
    Vertex t2_v[] = {t2.v1, t2.v2, t2.v3};

    int matched[3] = {0, 0, 0};

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (!matched[j] && vertices_equal(t1_v[i], t2_v[j])) {
                matched[j] = 1;
                break;
            }
        }
    }
    return matched[0] && matched[1] && matched[2];
}

// Dynamic array to store unique triangles
typedef struct {
    Triangle* data;
    size_t size;
    size_t capacity;
} TriangleSet;

// Initialize triangle set
void init_triangle_set(TriangleSet* set) {
    set->size = 0;
    set->capacity = 100;  // Initial capacity
    set->data = (Triangle*)malloc(set->capacity * sizeof(Triangle));
}

// Free memory allocated for triangle set
void free_triangle_set(TriangleSet* set) {
    free(set->data);
}

// Check if a triangle exists in the set
int triangle_exists(TriangleSet* set, Triangle t) {
    for (size_t i = 0; i < set->size; i++) {
        if (triangles_equal(set->data[i], t)) {
            return 1;
        }
    }
    return 0;
}

// Add a triangle to the set if it does not already exist
void add_triangle(TriangleSet* set, Triangle t) {
    if (!triangle_exists(set, t)) {
        if (set->size == set->capacity) {
            set->capacity *= 2;
            set->data = (Triangle*)realloc(set->data, set->capacity * sizeof(Triangle));
        }
        set->data[set->size++] = t;
    }
}

void convert_stl_to_bin(const char* stl_filename, const char* bin_filename, float scale, float rx, float ry, float rz) {
    FILE* stl_file = fopen(stl_filename, "rb");
    if (!stl_file) {
        printf("Failed to open STL file: %s\n", stl_filename);
        return;
    }

    char header[80];
    uint32_t num_triangles;
    fread(header, 1, 80, stl_file);
    fread(&num_triangles, sizeof(uint32_t), 1, stl_file);

    printf("STL file contains %u triangles.\n", num_triangles);

    Vertex centroid = {0.0f, 0.0f, 0.0f};
    uint32_t total_vertices = num_triangles * 3;

    // First pass: Compute the centroid
    for (uint32_t i = 0; i < num_triangles; i++) {
        float normal[3]; 
        fread(normal, sizeof(float), 3, stl_file);

        for (int j = 0; j < 3; j++) {
            Vertex v;
            fread(&v, sizeof(Vertex), 1, stl_file);

            centroid.x += v.x;
            centroid.y += v.y;
            centroid.z += v.z;
        }

        fseek(stl_file, 2, SEEK_CUR);
    }

    centroid.x /= total_vertices;
    centroid.y /= total_vertices;
    centroid.z /= total_vertices;

    printf("Model centroid: (%.3f, %.3f, %.3f)\n", centroid.x, centroid.y, centroid.z);

    fseek(stl_file, 84, SEEK_SET);

    FILE* bin_file = fopen(bin_filename, "wb");
    if (!bin_file) {
        printf("Failed to open binary file: %s\n", bin_filename);
        fclose(stl_file);
        return;
    }

    TriangleSet unique_triangles;
    init_triangle_set(&unique_triangles);

    for (uint32_t i = 0; i < num_triangles; i++) {
        float normal[3];
        Triangle tri;

        fread(normal, sizeof(float), 3, stl_file);

        for (int j = 0; j < 3; j++) {
            Vertex v;
            fread(&v, sizeof(Vertex), 1, stl_file);

            Vertex transformed_v = transform_vertex(
                (Vertex){v.x - centroid.x, v.y - centroid.y, v.z - centroid.z},
                scale, rx, ry, rz
            );

            if (j == 0) tri.v1 = transformed_v;
            if (j == 1) tri.v2 = transformed_v;
            if (j == 2) tri.v3 = transformed_v;
        }

        fseek(stl_file, 2, SEEK_CUR);

        add_triangle(&unique_triangles, tri);
    }

    printf("Unique triangles count: %zu\n", unique_triangles.size);

    for (size_t i = 0; i < unique_triangles.size; i++) {
        fwrite(&unique_triangles.data[i], sizeof(Triangle), 1, bin_file);
    }

    free_triangle_set(&unique_triangles);
    fclose(stl_file);
    fclose(bin_file);

    printf("Successfully converted STL to binary with unique triangles: %s\n", bin_filename);
}

int main(int argc, char* argv[]) {
    if (argc != 7) {
        printf("Usage: %s input.stl output.bin scale rotation_x rotation_y rotation_z\n", argv[0]);
        return 1;
    }

    convert_stl_to_bin(argv[1], argv[2], atof(argv[3]), atof(argv[4]), atof(argv[5]), atof(argv[6]));
    return 0;
}
