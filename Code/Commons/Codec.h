#ifndef COMMONS_CODEC_H
#define COMMONS_CODEC_H

int common_compress_file(const char *filename);
int common_decompress_file(const char *compressed_filename, const char *output_directory);

#endif
