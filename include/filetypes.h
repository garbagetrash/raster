#pragma once

typedef struct BlueFile {
    char magic[4];
    char header[508];
    char* data;
} BlueFile;

typedef struct DspDataFile {
    char magic[8];
    char data_type;
    char data_format;
    uint64_t nbytes_data;
    uint64_t nbytes_meta;
    char* data;
    char* metadata;
    uint32_t crc32;
} DspDataFile;

void read_file(const char* filename, char* buffer);
void read_nbytes(FILE* fid, const uint64_t byte_pos, const uint64_t nbytes, char* output);
