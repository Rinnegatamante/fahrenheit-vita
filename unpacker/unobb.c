#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <zlib.h>
#include <sys/stat.h>

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

void mkdir_rec(char *path) {
	char *p = strrchr(path, '/');
	if (p) {
		*p = '\0';
		mkdir_rec(path);
#ifdef MINGW
		mkdir(path);
#else
		mkdir(path, 0777);
#endif
		*p = '/';
	}
}

int main(int argc, char *argv[]) {
	z_stream z;
	char path[PATH_MAX];
	char name[PATH_MAX];
	char buf[4096];
	uint64_t i;
	uint64_t remain;
	uint64_t name_len;
	uint64_t file_len;
	uint64_t file_off;
	uint64_t idx_off;
	uint64_t idx_comp_len;
	uint64_t idx_uncomp_len;
	uint8_t *idx_comp;
	uint8_t *idx_uncomp;
	uint8_t *idx;
	uint8_t *idx_end;
	FILE *fin;
	FILE *fout;

	if (argc < 3) {
		printf("Usage: ./unobb in_obb out_dir");
		return 1;
	}

	fin = fopen(argv[1], "rb");
	if (!fin) {
		printf("Could not open %s\n", argv[1]);
		return 1;
	}

	fseek(fin, -(sizeof(idx_off) + sizeof(idx_uncomp_len)), SEEK_END);
	idx_comp_len = ftell(fin);

	fread(&idx_off, 1, sizeof(idx_off), fin);
	fread(&idx_uncomp_len, 1, sizeof(idx_uncomp_len), fin);

	idx_comp = malloc(idx_comp_len);
	idx_uncomp = malloc(idx_uncomp_len);
	if (!idx_comp || !idx_uncomp) {
		printf("Could not allocate index buffers\n");
		return 1;
	}

	fseek(fin, idx_off, SEEK_SET);
	fread(idx_comp, 1, idx_comp_len, fin);

	memset(&z, 0, sizeof(z_stream));
	z.avail_in = idx_comp_len;
	z.next_in = idx_comp;
	z.avail_out = idx_uncomp_len;
	z.next_out = idx_uncomp;

	if (inflateInit(&z) != Z_OK) {
		printf("inflateInit failed\n");
		return 1;
	}

	if (inflate(&z, Z_FINISH) != Z_STREAM_END) {
		printf("inflate failed\n");
		return 1;
	}

	inflateEnd(&z);

	idx = idx_uncomp;
	idx_end = idx + idx_uncomp_len;
	while (idx + sizeof(uint64_t) < idx_end) {
		name_len = *(uint64_t *)(idx + 0x08);

		if (name_len >= PATH_MAX) {
			printf("Invalid size\n");
			return 1;
		}

		strncpy(name, idx + 0x10, name_len);
		name[name_len] = '\0';

		snprintf(path, sizeof(path), "%s/%s", argv[2], name);

		printf("Writing %s...\n", path);

		file_off = *(uint64_t *)(idx + 0x10 + name_len);
		file_len = *(uint64_t *)(idx + 0x18 + name_len);

		fseek(fin, file_off, SEEK_SET);

		mkdir_rec(path);

		fout = fopen(path, "wb");
		if (!fout) {
			printf("Could not open %s\n", path);
			return 1;
		}

		i = 0;
		while (i < file_len) {
			remain = MIN(file_len - i, sizeof(buf));
			fread(buf, 1, remain, fin);
			fwrite(buf, 1, remain, fout);
			i += remain;
		}

		fclose(fout);

		idx += 0x20 + name_len;
	}


	free(idx_uncomp);
	free(idx_comp);
	fclose(fin);

	return 0;
}