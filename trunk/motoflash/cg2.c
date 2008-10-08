#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>

FILE *listfile = NULL;

char *dirname = NULL;
char *listfilename = NULL;

#define DEFAULT_LISTFILE "cg2_listfile"

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

static struct option options[] = {
	{"create", 0, NULL, 'c'},
	{"dir", 1, NULL, 'd'},
	{"extract", 1, NULL, 'x'},
	{"help", 0, NULL, 'h'},
	{"listfile", 1, NULL, 'l'},
	{"output", 1, NULL, 'o'},
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
	//printf("Wrote %d bytes to %s.\n", length, outfilename);
	fclose(out);
}

void usage(char *basename) {
	printf("Syntax: %s -x file [-d dirname] [-l listfile]\n", basename);
	printf("        %s -c -o filename [-l listfile]\n", basename);
	printf("        %s -h\n", basename);
	printf("        %s -V\n", basename);
/*	printf(
"  -r, --rewind			combine overlapping addresses into one file, rewinding over the overlapped bytes\n\
  -l, --list listfile		direct the s-record list file to listfile, the default is \"listfile\"\n\
  -f, --format format		filename format for code groups, default is \"Group %%g - %%a.bin\"\n\
  					%%g Group Number, %%a Address (in 8-char hex)\n\
  -h, --help			display this help text\n\
  -V, --version			display version information\n\
  file				input file in s-record format\n\nReport bugs to <dustin@howett.net>.\n");
  */
}

int extract_cg2(char *filename) {
	struct blockheader curblock;
	int count = 0, totalsize = 0;
	FILE *in;
	memset(&curblock, 0, 0x310);

	printf("Extracing from CG2 (%s) to folder '%s'.\n", filename, dirname);
	in = fopen(filename, "r");
	if(in == NULL) {
		fprintf(stderr, "Error opening %s.\n", filename);
		return EXIT_FAILURE;
	}

	while(fread(&curblock, 0x310, 1, in) != 0) {
		int pad = 0;

		pad = (curblock.length % 16);
		//printf("%s%s, %d bytes\n", curblock.filepath, curblock.filename, curblock.length);

		output_file(curblock.filepath, curblock.filename, curblock.length, in);
		count++; totalsize += curblock.length;

		// Pad to 16 bytes
		while(ftell(in) % 16 != 0) fseek(in, 1, SEEK_CUR);
	}

	printf("Extracted %d files totalling %d bytes.\n", count, totalsize);

	return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
	char line[16384];
	char *filename;
	char *outfilename;
	bool extract, create;
	struct blockheader cur_block;

	int opt;
	while((opt = getopt_long(argc, argv, "cdx:hl:o:V", options, NULL)) != -1) {
		switch(opt) {
			case 'x': extract = true; filename = strdup(optarg); break;
			case 'c': create = true; break;
			case 'd': dirname = strdup(optarg); break;
			case 'l': listfilename = strdup(optarg); break;
			case 'o': outfilename = strdup(optarg); break;
			case 'h':
				usage(argv[0]);
				return EXIT_SUCCESS;
			case 'V':
				printf("cg2 1.0\nCopyright (C) 2008 Dustin Howett\nNo License\n\nWritten by Dustin Howett.\n");
				return EXIT_SUCCESS;
			default:
				usage(argv[0]);
				return EXIT_FAILURE;
				break;
		}
	}

	if(listfilename == NULL) listfilename = strdup(DEFAULT_LISTFILE);

	if(extract == create) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	if(extract) {
		if(dirname == NULL) {
			dirname = malloc(1024);
			sprintf(dirname, "%s.out", filename);
		}
		return extract_cg2(filename);
	} else if(create) {
		if(outfilename == NULL) {
			fprintf(stderr, "You need to specify an output filename.\nSee %s --help for syntax help.\n", argv[0]);
			return EXIT_FAILURE;
		}
//		return create_cg2(outfilename);
	}

	return EXIT_SUCCESS;
}
