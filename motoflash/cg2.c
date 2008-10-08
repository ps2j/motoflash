#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>

FILE *out = NULL;
FILE *listfile = NULL;

struct blockheader {
	int fill1[10];
	int blockType;
	int fill2[46];
	char filename[255];
	char filepath[255];
	char fill3[2];
	int length;
	int unknown1;
	int unknown2;
	int unknown3;
	int unknown4;
	int unknown5;
	int unknown6;
	char pad[16];
};

//struct

static struct option options[] = {
	{"help", 0, NULL, 'h'},
	{"dir", 1, NULL, 'd'},
	{"version", 0, NULL, 'V'},
	{0, 0, 0, 0}
};

void fixpath(char *path) {
	while(*path) {
		if(*path == '/') *path='_';
		path++;
	}
}

void output_file(char *path, char *name, int length, FILE *in) {
	int remain = length;
	int toread = (512 < remain) ? 512 : remain;
	char outfilename[512];
	char databuf[513];
	FILE *out;
	fixpath(path);
	sprintf(outfilename, "%s%s", path, name);
	out = fopen(outfilename, "wb");
	while(remain > 0) {
		fread(databuf, toread, 1, in);
		fwrite(databuf, toread, 1, out);
		remain -= toread;
		toread = (512 < remain) ? 512 : remain;
	}
	printf("Wrote %d bytes to %s.\n", length, outfilename);
	fclose(out);
}

void usage(char *basename) {
	printf("Syntax: %s [-r] [-l listfile] [-f format] file\n", basename);
	printf(
"  -r, --rewind			combine overlapping addresses into one file, rewinding over the overlapped bytes\n\
  -l, --list listfile		direct the s-record list file to listfile, the default is \"listfile\"\n\
  -f, --format format		filename format for code groups, default is \"Group %%g - %%a.bin\"\n\
  					%%g Group Number, %%a Address (in 8-char hex)\n\
  -h, --help			display this help text\n\
  -V, --version			display version information\n\
  file				input file in s-record format\n\nReport bugs to <dustin@howett.net>.\n");
}

int main(int argc, char **argv) {
	char line[16384];
	bool customlistfile = false;
	bool customformat = false;
	char *listfilename;
	char *infilename;
	FILE *infile;
	struct blockheader cur_block;

	int opt;
	while((opt = getopt_long(argc, argv, "hV", options, NULL)) != -1) {
		switch(opt) {
			case 'h':
				usage(argv[0]);
				return EXIT_SUCCESS;
			case 'V':
				printf("decompile 1.0\nCopyright (C) 2008 Dustin Howett\nNo License\n\nWritten by Dustin Howett.\n");
				return EXIT_SUCCESS;
			default:
		//		usage(argv[0]);
				return EXIT_FAILURE;
				break;
		}
	}

	if(optind >= argc) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	infilename = strdup(argv[optind]);
	printf("Reading: %s\n", infilename);
	infile = fopen(infilename, "r");
	if(infile == NULL) {
		fprintf(stderr, "Error opening %s.\n", infilename);
		return EXIT_FAILURE;
	}

	while(fread(&cur_block, 0x310, 1, infile) != 0) {
		int pad = 0;

		pad = (cur_block.length % 16); // +4
		printf("%s%s, %d bytes\n", cur_block.filepath, cur_block.filename, cur_block.length);

		output_file(cur_block.filepath, cur_block.filename, cur_block.length, infile);

		// Pad to 16 bytes
		while(ftell(infile) % 16 != 0) fseek(infile, 1, SEEK_CUR);
	}

	free(infilename);

	return EXIT_SUCCESS;
}
