#ifndef FORK_DECOMPRESSOR_H
#define FORK_DECOMPRESSOR_H

int decompress_file(const char *compressed_filename, const char *output_directory);
int decompress_directory(const char *directory, const char *output_directory);

#endif
