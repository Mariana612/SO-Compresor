#include "Decompressor.h"
#include "../Commons/Codec.h"
#include "../Commons/FileList.h"

int decompress_directory(const char *directory, const char *output_directory)
{
    FileList files;
    size_t index;
    int success = 1;

    if (!file_list_load(directory, 1, &files))
        return 0;
    for (index = 0; index < files.count; index++)
        if (!common_decompress_file(files.paths[index], output_directory))
            success = 0;
    file_list_free(&files);
    return success;
}
