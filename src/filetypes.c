#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct Buffer {
    uint64_t nbytes;
    char* data;
} Buffer;

void read_file(const char* filename, char* buffer)
{
    FILE* fid = fopen(filename, "rb");
    char magic[9];
    fread(magic, sizeof(char), 8, fid);
    if (fseek(fid, 0, SEEK_END) != 0)
    {
        perror("fseek");
        return;
    }
    size_t nbytes = ftell(fid);
    if (nbytes == -1)
    {
        perror("ftell");
        return;
    }

    if (strncmp(magic, "BLUE", 4) == 0)
    {
        // BLUE file
        if (fseek(fid, 512, SEEK_SET) != 0)
        {
            perror("fseek");
            return;
        }
        fread(buffer, sizeof(char), nbytes - 512, fid);
    } else if (strncmp(magic, "DSP_DATA", 8) == 0) {
        // DSP_DATA file
        if (fseek(fid, 0, SEEK_SET) != 0)
        {
            perror("fseek");
            return;
        }
        uint64_t data_type, data_format, data_length, meta_length;
        fread(&data_type, sizeof(uint64_t), 1, fid);
        fread(&data_format, sizeof(uint64_t), 1, fid);
        fread(&data_length, sizeof(uint64_t), 1, fid);
        fread(&meta_length, sizeof(uint64_t), 1, fid);
        fread(buffer, sizeof(char), data_length, fid);
        // Just ignore metadata and checksum for now
    } else {
        // Just assume pure data otherwise
        fseek(fid, 0, SEEK_SET);
        fread(buffer, sizeof(char), nbytes, fid);
    }
    fclose(fid);
}

void read_nbytes(FILE* fid, const uint64_t byte_pos, const uint64_t nbytes, char* output)
{
    if (fseek(fid, byte_pos, SEEK_SET) != 0)
    {
        perror("fseek");
        return;
    }
    fread(output, sizeof(char), nbytes, fid);
}
