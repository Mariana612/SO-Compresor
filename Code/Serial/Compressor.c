#include "Compressor.h"
#include "../Commons/Codec.h"
#include "../Commons/FileList.h"

int compress_directory(const char *directory)
{
    FileList files;
    size_t index;
    int success = 1;

    if (!file_list_load(directory, 0, &files))
        return 0;
    for (index = 0; index < files.count; index++)
        if (!common_compress_file(files.paths[index]))
            success = 0;
    file_list_free(&files);
    return success;
}
