#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>

#include <sys/stat.h>

/* mkdir_recursive from Nico Golde @ http://nion.modprobe.de/blog/archives/357-Recursive-directoriy-creation.html
   Thanks a million! */

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
	{"test", 1, NULL, 't'},
	{"listfile", 1, NULL, 'l'},
	{"output", 1, NULL, 'o'},
	{"version", 0, NULL, 'V'},
	{0, 0, 0, 0}
};

void output_file(char *file, int length, FILE *in) {
	int remain = length;
	int toread = (512 < remain) ? 512 : remain;
	char databuf[513];
	FILE *out;
	out = fopen(file, "wb");
	while(remain > 0) {
		fread(databuf, toread, 1, in);
		fwrite(databuf, toread, 1, out);
		remain -= toread;
		toread = (512 < remain) ? 512 : remain;
	}
	//printf("Wrote %d bytes to %s.\n", length, outfilename);
	fclose(out);
}

static void mkdir_recursive(const char *path) {
	char opath[256];
	char *p;
	size_t len;

	strncpy(opath, path, sizeof(opath));
	len = strlen(opath);
	if (opath[len - 1] == '/')
		opath[len - 1] = '\0';
	for (p = opath; *p; p++)
		if (*p == '/') {
			*p = '\0';
			if (access(opath, F_OK))
				mkdir(opath, S_IRWXU);
			*p = '/';
		}
	if (access(opath, F_OK))        /* if path is not terminated with / */
		mkdir(opath, S_IRWXU);
}

char *create_output_pathname(char *cgpath) {
	char *out = malloc(1024);
	if(cgpath[strlen(cgpath)-1] != '/') strcat(cgpath, "/");
	if(cgpath[0] == '/') cgpath++;

	sprintf(out, "%s/%s", dirname, cgpath);
	return out;
}

char *create_output_filename(char *cgpath, char *filename) {
	char *out = malloc(1024);
	if(cgpath[strlen(cgpath)-1] != '/') strcat(cgpath, "/");
	if(cgpath[0] == '/') cgpath++;

	sprintf(out, "%s/%s%s", dirname, cgpath, filename);
	return out;
}

void usage(char *basename) {
	printf("Syntax: %s -x file [-d dirname] [-l listfile]\n", basename);
	printf("        %s -t file\n", basename);
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
	FILE *list;
	memset(&curblock, 0, 0x310);

	printf("Extracing from CG2 (%s) to folder '%s'.\n", filename, dirname);
	in = fopen(filename, "r");
	if(in == NULL) {
		fprintf(stderr, "Error opening %s.\n", filename);
		return EXIT_FAILURE;
	}

	list = fopen(listfilename, "w");

	while(fread(&curblock, 0x310, 1, in) != 0) {
		int pad = 0;
		char *outfile = create_output_filename(curblock.filepath, curblock.filename);
		char *outpath = create_output_pathname(curblock.filepath);

		pad = (curblock.length % 16);
		//printf("%s%s, %d bytes\n", curblock.filepath, curblock.filename, curblock.length);

		mkdir_recursive(outpath);
		output_file(outfile, curblock.length, in);
		count++; totalsize += curblock.length;

		fprintf(list, "FILE %s IN %s AT %s TYPE 0x%8.8x\n", curblock.filename, curblock.filepath, outfile, curblock.blockType);

		free(outfile);
		free(outpath);

		// Pad to 16 bytes
		while(ftell(in) % 16 != 0) fseek(in, 1, SEEK_CUR);
	}

	fclose(list);

	printf("Extracted %d files totalling %d bytes.\n", count, totalsize);

	return EXIT_SUCCESS;
}

int list_cg2(char *filename) {
	struct blockheader curblock;
	int count = 0, totalsize = 0;
	FILE *in;
	memset(&curblock, 0, 0x310);

	printf("Listing from CG2 (%s).\n", filename);
	in = fopen(filename, "r");
	if(in == NULL) {
		fprintf(stderr, "Error opening %s.\n", filename);
		return EXIT_FAILURE;
	}

	while(fread(&curblock, 0x310, 1, in) != 0) {
		int pad = 0;

		printf("%s%s %d bytes, type %8.8x.\n", curblock.filepath, curblock.filename, curblock.length, curblock.blockType);
		pad = (curblock.length % 16);
		fseek(in, curblock.length, SEEK_CUR);
		count++; totalsize += curblock.length;
		// Pad to 16 bytes
		while(ftell(in) % 16 != 0) fseek(in, 1, SEEK_CUR);
	}

	printf("Listed %d files totalling %d bytes.\n", count, totalsize);

	return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
	char line[16384];
	char *filename;
	char *outfilename = NULL;
	bool extract, create, list;
	struct blockheader cur_block;

	int opt;
	while((opt = getopt_long(argc, argv, "cd:x:ht:l:o:V", options, NULL)) != -1) {
		switch(opt) {
			case 'x': extract = true; filename = strdup(optarg); break;
			case 't': list = true; filename = strdup(optarg); break;
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

	if(extract + create + list > 1) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}

	if(extract) {
		if(dirname == NULL) {
			dirname = malloc(1024);
			sprintf(dirname, "%s.out", filename);
		} else {
			if(dirname[strlen(dirname)-1] == '/') dirname[strlen(dirname)-1] = '\0';
		}
		return extract_cg2(filename);
	} else if(create) {
		if(outfilename == NULL) {
			fprintf(stderr, "You need to specify an output filename.\nSee %s --help for syntax help.\n", argv[0]);
			return EXIT_FAILURE;
		}
//		return create_cg2(outfilename);
	} else if(list) {
		return list_cg2(filename);
	}

	return EXIT_SUCCESS;
}
