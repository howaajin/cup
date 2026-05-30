#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265359f

typedef struct
{
    float x, y;
} vec2;

typedef struct
{
    float x, y, z;
} vec3;

static inline vec2 vec2_new(float x, float y)
{
    vec2 v = {x, y};
    return v;
}

static inline vec3 vec3_new(float x, float y, float z)
{
    vec3 v = {x, y, z};
    return v;
}

static inline vec3 vec3_add(vec3 a, vec3 b)
{
    return vec3_new(a.x + b.x, a.y + b.y, a.z + b.z);
}

static inline vec2 vec2_add(vec2 a, vec2 b)
{
    return vec2_new(a.x + b.x, a.y + b.y);
}

static inline vec2 vec2_sub(vec2 a, vec2 b)
{
    return vec2_new(a.x - b.x, a.y - b.y);
}

static inline vec3 vec3_sub(vec3 a, vec3 b)
{
    return vec3_new(a.x - b.x, a.y - b.y, a.z - b.z);
}

static inline vec3 vec3_mul_scalar(vec3 a, float s)
{
    return vec3_new(a.x * s, a.y * s, a.z * s);
}

static inline vec2 vec2_mul_scalar(vec2 a, float s)
{
    return vec2_new(a.x * s, a.y * s);
}

static inline vec3 vec3_mul(vec3 a, vec3 b)
{
    return vec3_new(a.x * b.x, a.y * b.y, a.z * b.z);
}

static inline float vec3_dot(vec3 a, vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline float vec2_dot(vec2 a, vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

static inline float vec3_length(vec3 v)
{
    return sqrtf(vec3_dot(v, v));
}

static inline float vec2_length(vec2 v)
{
    return sqrtf(vec2_dot(v, v));
}

static inline vec3 vec3_normalize(vec3 v)
{
    float len = vec3_length(v);
    if (len == 0.0f) return vec3_new(0, 0, 0);
    return vec3_mul_scalar(v, 1.0f / len);
}

static inline float f_clamp(float x, float minVal, float maxVal)
{
    return fmaxf(minVal, fminf(x, maxVal));
}

static inline vec3 vec3_clamp(vec3 v, float minVal, float maxVal)
{
    return vec3_new(f_clamp(v.x, minVal, maxVal), f_clamp(v.y, minVal, maxVal), f_clamp(v.z, minVal, maxVal));
}

static inline vec3 vec3_mix(vec3 x, vec3 y, float a)
{
    return vec3_add(vec3_mul_scalar(x, 1.0f - a), vec3_mul_scalar(y, a));
}

static inline float f_step(float edge, float x)
{
    return x < edge ? 0.0f : 1.0f;
}

static inline float f_smoothstep(float edge0, float edge1, float x)
{
    float t = f_clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static inline vec2 vec2_abs(vec2 a)
{
    return vec2_new(fabsf(a.x), fabsf(a.y));
}

static inline vec2 vec2_max_scalar(vec2 a, float s)
{
    return vec2_new(fmaxf(a.x, s), fmaxf(a.y, s));
}

static inline float f_sign(float x)
{
    return (x > 0.0f) ? 1.0f : ((x < 0.0f) ? -1.0f : 0.0f);
}

static vec3 current_bg_color = {0.18f, 0.20f, 0.25f};

static inline vec3 get_bg_color()
{
    return current_bg_color;
}
static inline vec3 get_c_color()
{
    return vec3_new(0.35f, 0.45f, 0.60f);
}
static inline vec3 get_core_color()
{
    return vec3_new(0.85f, 0.87f, 0.90f);
}
static inline vec3 get_hole_color()
{
    return current_bg_color;
}

static inline vec2 rotate2d(vec2 p, float angle)
{
    float c = cosf(angle);
    float s = sinf(angle);
    return vec2_new(c * p.x - s * p.y, s * p.x + c * p.y);
}

static inline float sdf_box(vec2 p, vec2 b)
{
    vec2 d = vec2_sub(vec2_abs(p), b);
    vec2 max_d = vec2_max_scalar(d, 0.0f);
    float len = vec2_length(max_d);
    float inside = fminf(fmaxf(d.x, d.y), 0.0f);
    return len + inside;
}

static inline float sdf_hexagon(vec2 p, float r)
{
    const vec3 k = vec3_new(-0.866025404f, 0.5f, 0.577350269f);
    p = vec2_abs(p);
    vec2 kxy = vec2_new(k.x, k.y);
    float dot_val = vec2_dot(kxy, p);
    float factor = 2.0f * fminf(dot_val, 0.0f);
    p = vec2_sub(p, vec2_mul_scalar(kxy, factor));
    float kzr = k.z * r;
    vec2 offset = vec2_new(f_clamp(p.x, -kzr, kzr), r);
    p = vec2_sub(p, offset);
    return vec2_length(p) * f_sign(p.y);
}

static inline float sdf_letter_c(vec2 p, float r, float thickness)
{
    float len = vec2_length(p);
    float d = fabsf(len - r) - thickness;
    vec2 boxPos = vec2_sub(p, vec2_new(r, 0.0f));
    vec2 boxSize = vec2_new(r * 0.7f, r * 0.55f);
    float cutBox = sdf_box(boxPos, boxSize);
    return fmaxf(d, -cutBox);
}

static inline vec3 draw_shape(vec3 baseColor, vec3 shapeColor, float dist, float aaWidth)
{
    float alpha = 1.0f - f_smoothstep(-aaWidth, aaWidth, dist);
    return vec3_mix(baseColor, shapeColor, alpha);
}

typedef struct
{
    float width;
    float height;
    float iTime;
} Uniforms;

static vec3 shader_func_cup_icon(vec2 uv, Uniforms u)
{
    vec2 p = vec2_sub(uv, vec2_new(0.5f, 0.5f));
    vec3 col = get_bg_color();
    float px = 1.0f / u.height;
    vec2 pCore = p;
    pCore = rotate2d(pCore, 0.0f);
    float dHex = sdf_hexagon(pCore, 0.18f);
    col = draw_shape(col, get_core_color(), dHex, px * 1.5f);
    float dHole = sdf_hexagon(pCore, 0.08f);
    col = draw_shape(col, get_hole_color(), dHole, px * 1.5f);
    vec2 pC = vec2_add(p, vec2_new(0.02f, 0.0f));
    float dC = sdf_letter_c(pC, 0.35f, 0.09f);
    col = draw_shape(col, get_c_color(), dC, px * 1.5f);
    return col;
}

#pragma pack(push, 1)
typedef struct
{
    uint16_t id_reserved;
    uint16_t id_type;
    uint16_t id_count;
} IconDir;

typedef struct
{
    uint8_t width;
    uint8_t height;
    uint8_t color_count;
    uint8_t reserved;
    uint16_t planes;
    uint16_t bit_count;
    uint32_t bytes_in_res;
    uint32_t image_offset;
} IconDirEntry;

typedef struct
{
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bit_count;
    uint32_t compression;
    uint32_t size_image;
    int32_t x_pixels_per_meter;
    int32_t y_pixels_per_meter;
    uint32_t color_used;
    uint32_t color_important;
} BitmapInfoHeader;
#pragma pack(pop)

typedef vec3 (*shader_func_t)(vec2 uv, Uniforms u);

static void render_pixel_rgba(int width, int height, int x, int render_y, shader_func_t shader, uint8_t rgba[4])
{
    Uniforms u;
    u.width = (float)width;
    u.height = (float)height;
    u.iTime = 0.0f;

    float u_coord = (float)x / u.width;
    float v_coord = 1.0f - ((float)render_y / u.height);
    float aspect = u.width / u.height;
    u_coord = (u_coord - 0.5f) * aspect + 0.5f;
    vec2 uv = vec2_new(u_coord, v_coord);

    current_bg_color = vec3_new(0.0f, 0.0f, 0.0f);
    vec3 c_black = shader(uv, u);
    c_black = vec3_clamp(c_black, 0.0f, 1.0f);

    current_bg_color = vec3_new(1.0f, 1.0f, 1.0f);
    vec3 c_white = shader(uv, u);
    c_white = vec3_clamp(c_white, 0.0f, 1.0f);

    vec3 diff = vec3_sub(c_white, c_black);
    float alpha_f = 1.0f - (diff.x + diff.y + diff.z) / 3.0f;
    alpha_f = f_clamp(alpha_f, 0.0f, 1.0f);

    vec3 final_rgb;
    if (alpha_f > 0.001f)
    {
        final_rgb = vec3_mul_scalar(c_black, 1.0f / alpha_f);
        final_rgb = vec3_clamp(final_rgb, 0.0f, 1.0f);
    }
    else
    {
        final_rgb = vec3_new(0, 0, 0);
    }

    rgba[0] = (uint8_t)(final_rgb.x * 255.0f);
    rgba[1] = (uint8_t)(final_rgb.y * 255.0f);
    rgba[2] = (uint8_t)(final_rgb.z * 255.0f);
    rgba[3] = (uint8_t)(alpha_f * 255.0f);
}

static int generate_icon(int width, int height, shader_func_t shader, const char* filename)
{
    FILE* f = fopen(filename, "wb");
    if (!f)
    {
        fprintf(stderr, "fopen failed: %s\n", filename);
        return EXIT_FAILURE;
    }

    int pixel_stride = width * 4;
    uint32_t xor_size = pixel_stride * height;

    int mask_row_bytes = ((width + 31) / 32) * 4;
    uint32_t mask_size = mask_row_bytes * height;

    uint32_t total_image_size = sizeof(BitmapInfoHeader) + xor_size + mask_size;

    IconDir dir = {0, 1, 1};
    fwrite(&dir, sizeof(dir), 1, f);

    IconDirEntry entry;
    entry.width = (width == 256) ? 0 : (uint8_t)width;
    entry.height = (height == 256) ? 0 : (uint8_t)height;
    entry.color_count = 0;
    entry.reserved = 0;
    entry.planes = 1;
    entry.bit_count = 32;
    entry.bytes_in_res = total_image_size;
    entry.image_offset = sizeof(IconDir) + sizeof(IconDirEntry);
    fwrite(&entry, sizeof(entry), 1, f);

    BitmapInfoHeader bmi = {0};
    bmi.size = sizeof(BitmapInfoHeader);
    bmi.width = width;
    bmi.height = height * 2;
    bmi.planes = 1;
    bmi.bit_count = 32;
    bmi.size_image = xor_size + mask_size;
    fwrite(&bmi, sizeof(bmi), 1, f);

    uint8_t* row_buffer = (uint8_t*)malloc(pixel_stride);

    for (int y = 0; y < height; ++y)
    {
        int render_y = height - 1 - y;

        for (int x = 0; x < width; ++x)
        {
            uint8_t rgba[4];
            render_pixel_rgba(width, height, x, render_y, shader, rgba);

            int idx = x * 4;
            row_buffer[idx + 0] = rgba[2];
            row_buffer[idx + 1] = rgba[1];
            row_buffer[idx + 2] = rgba[0];
            row_buffer[idx + 3] = rgba[3];
        }
        fwrite(row_buffer, 1, pixel_stride, f);
    }
    free(row_buffer);

    uint8_t* mask_buffer = (uint8_t*)calloc(1, mask_size);
    fwrite(mask_buffer, 1, mask_size, f);
    free(mask_buffer);

    fclose(f);
    return EXIT_SUCCESS;
}

static void write_u32_be(FILE* f, uint32_t value)
{
    uint8_t bytes[4] = {
        (uint8_t)((value >> 24) & 0xff),
        (uint8_t)((value >> 16) & 0xff),
        (uint8_t)((value >> 8) & 0xff),
        (uint8_t)(value & 0xff),
    };
    fwrite(bytes, 1, sizeof(bytes), f);
}

static uint32_t crc32_update(uint32_t crc, uint8_t const* data, size_t size)
{
    static uint32_t table[256];
    static int table_ready = 0;
    if (!table_ready)
    {
        for (uint32_t i = 0; i < 256; ++i)
        {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k)
            {
                c = (c & 1) ? (0xedb88320u ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        table_ready = 1;
    }

    crc = ~crc;
    for (size_t i = 0; i < size; ++i)
    {
        crc = table[(crc ^ data[i]) & 0xff] ^ (crc >> 8);
    }
    return ~crc;
}

static uint32_t adler32(uint8_t const* data, size_t size)
{
    uint32_t a = 1;
    uint32_t b = 0;
    for (size_t i = 0; i < size; ++i)
    {
        a = (a + data[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

static void png_write_chunk(FILE* f, char const type[4], uint8_t const* data, uint32_t size)
{
    write_u32_be(f, size);
    fwrite(type, 1, 4, f);
    if (size)
    {
        fwrite(data, 1, size, f);
    }

    uint32_t crc = 0;
    crc = crc32_update(crc, (uint8_t const*)type, 4);
    if (size)
    {
        crc = crc32_update(crc, data, size);
    }
    write_u32_be(f, crc);
}

static int generate_png(int width, int height, shader_func_t shader, char const* filename)
{
    size_t row_size = (size_t)width * 4 + 1;
    size_t raw_size = row_size * (size_t)height;
    uint8_t* raw = (uint8_t*)malloc(raw_size);
    if (!raw)
    {
        return EXIT_FAILURE;
    }

    uint8_t* p = raw;
    for (int y = 0; y < height; ++y)
    {
        *p++ = 0;
        for (int x = 0; x < width; ++x)
        {
            uint8_t rgba[4];
            render_pixel_rgba(width, height, x, y, shader, rgba);
            memcpy(p, rgba, sizeof(rgba));
            p += sizeof(rgba);
        }
    }

    size_t num_blocks = (raw_size + 65534) / 65535;
    size_t zlib_size = 2 + raw_size + num_blocks * 5 + 4;
    uint8_t* zlib = (uint8_t*)malloc(zlib_size);
    if (!zlib)
    {
        free(raw);
        return EXIT_FAILURE;
    }

    p = zlib;
    *p++ = 0x78;
    *p++ = 0x01;
    size_t remaining = raw_size;
    uint8_t* src = raw;
    while (remaining)
    {
        uint16_t block_size = (remaining > 65535) ? 65535 : (uint16_t)remaining;
        int final_block = remaining == block_size;
        *p++ = final_block ? 1 : 0;
        *p++ = (uint8_t)(block_size & 0xff);
        *p++ = (uint8_t)(block_size >> 8);
        uint16_t nlen = (uint16_t)~block_size;
        *p++ = (uint8_t)(nlen & 0xff);
        *p++ = (uint8_t)(nlen >> 8);
        memcpy(p, src, block_size);
        p += block_size;
        src += block_size;
        remaining -= block_size;
    }
    uint32_t adler = adler32(raw, raw_size);
    *p++ = (uint8_t)((adler >> 24) & 0xff);
    *p++ = (uint8_t)((adler >> 16) & 0xff);
    *p++ = (uint8_t)((adler >> 8) & 0xff);
    *p++ = (uint8_t)(adler & 0xff);

    FILE* f = fopen(filename, "wb");
    if (!f)
    {
        fprintf(stderr, "fopen failed: %s\n", filename);
        free(zlib);
        free(raw);
        return EXIT_FAILURE;
    }

    uint8_t signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    fwrite(signature, 1, sizeof(signature), f);

    uint8_t ihdr[13] = {0};
    ihdr[0] = (uint8_t)((width >> 24) & 0xff);
    ihdr[1] = (uint8_t)((width >> 16) & 0xff);
    ihdr[2] = (uint8_t)((width >> 8) & 0xff);
    ihdr[3] = (uint8_t)(width & 0xff);
    ihdr[4] = (uint8_t)((height >> 24) & 0xff);
    ihdr[5] = (uint8_t)((height >> 16) & 0xff);
    ihdr[6] = (uint8_t)((height >> 8) & 0xff);
    ihdr[7] = (uint8_t)(height & 0xff);
    ihdr[8] = 8;
    ihdr[9] = 6;
    png_write_chunk(f, "IHDR", ihdr, sizeof(ihdr));
    png_write_chunk(f, "IDAT", zlib, (uint32_t)(p - zlib));
    png_write_chunk(f, "IEND", NULL, 0);

    fclose(f);
    free(zlib);
    free(raw);
    return EXIT_SUCCESS;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        return EXIT_FAILURE;
    }
    if (generate_icon(256, 256, shader_func_cup_icon, argv[1]) != EXIT_SUCCESS)
    {
        return EXIT_FAILURE;
    }
    for (int i = 2; i < argc; ++i)
    {
        if (generate_png(256, 256, shader_func_cup_icon, argv[i]) != EXIT_SUCCESS)
        {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
