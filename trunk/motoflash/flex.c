#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>

#include <sys/types.h>
#include <sys/stat.h>

/* mkdir_recursive from Nico Golde @ http://nion.modprobe.de/blog/archives/357-Recursive-directoriy-creation.html
   Thanks a million! */

char *dirname = NULL;
char *listfilename = NULL;

#define DEFAULT_LISTFILE "flex_listfile"

// Start at 0x12?

struct fileheader {
	int unknown1; // 0x00000000
	int unknown2; // 0xB1
	int unknown3; // 0x81040200
	int unknown4; // 0x01000000
	short numseems;
};

struct seemheader { // 14 bytes
	int unknown1; // Seems to be Length + 16 (unless mult. copies)
	short skip_next; // Length + 8? (standard)
	short seem;
	short record;
	short unknown3; // No idea. Noticed it's 0 unless there're multiple copies,.
	short length;
};

static struct option options[] = {
	{"create", 0, NULL, 'c'},
	{"dir", 1, NULL, 'd'},
	{"extract", 1, NULL, 'x'},
	{"help", 0, NULL, 'h'},
	{"listfile", 1, NULL, 'l'},
	{"test", 1, NULL, 't'},
	{"output", 1, NULL, 'o'},
	{"version", 0, NULL, 'V'},
	{0, 0, 0, 0}
};

void output_file(short seem, short record, int length, FILE *in) {
	int toread = length;
	char *databuf = malloc(length+2);
	char filename[512];
	FILE *out;

	sprintf(filename, "%4.4x_%4.4x.seem", seem, record);
	out = fopen(filename, "wb");
	fread(databuf, toread, 1, in);
	fwrite(databuf, toread, 1, out);

	fclose(out);
	free(databuf);
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

int extract_flex(char *filename) {
	struct seemheader curseem;
	struct fileheader curfile;
	int count = 0, totalsize = 0;
	FILE *in;
	FILE *list;
	memset(&curseem, 0, 14);

	printf("Extracing from Flex File (%s).\n", filename, dirname);
	in = fopen(filename, "r");
	if(in == NULL) {
		fprintf(stderr, "Error opening %s.\n", filename);
		return EXIT_FAILURE;
	}

	list = fopen(listfilename, "w");

	fread(&curfile, 0x12, 1, in);

	while(fread(&curseem, 14, 1, in) != 0) {
		int pad = 0;

		output_file(curseem.seem, curseem.record, curseem.length, in);
		count++; totalsize += curseem.length;

		fprintf(list, "SEEM %4.4x REC %4.4x\n", curseem.seem, curseem.record);
	}

	fclose(list);

	printf("Extracted %d seems totalling %d bytes.\n", count, totalsize);

	return EXIT_SUCCESS;
}

int list_flex(char *filename) {
	struct seemheader curseem;
	struct fileheader curfile;
	int count = 0, totalsize = 0;
	FILE *in;
	memset(&curseem, 0, 14);

	printf("Listing from Flex File (%s).\n", filename, dirname);
	in = fopen(filename, "r");
	if(in == NULL) {
		fprintf(stderr, "Error opening %s.\n", filename);
		return EXIT_FAILURE;
	}

	fread(&curfile, 0x12, 1, in);

	while(fread(&curseem, 14, 1, in) != 0) {
		int pad = 0;

		fseek(in, curseem.length, SEEK_CUR);
		count++; totalsize += curseem.length;

		printf("Seem %4.4x record %4.4x, length %4.4x. Unknowns: %d, %d, %d\n", curseem.seem, curseem.record, curseem.length,
			curseem.unknown1, curseem.skip_next, curseem.unknown3);
	}

	printf("Listed %d seems totalling %d bytes.\n", count, totalsize);

	return EXIT_SUCCESS;
}

int create_flex(char *filename) {
	FILE *list;
	FILE *out;
	char line[1024];
	struct fileheader curfile;
	int numseems = 0;

	list = fopen(listfilename, "r");
	if(!list) {
		fprintf(stderr, "Error opening listfile %s.\n", listfilename);
		return EXIT_FAILURE;
	}

	out = fopen(filename, "wb+");
	if(!out) {
		fprintf(stderr, "Error opening output file %s.\n", filename);
		return EXIT_FAILURE;
	}

	curfile.unknown1 = 0x00000000;
	curfile.unknown2 = 0x000000B1;
	curfile.unknown3 = 0x81040200;
	curfile.unknown4 = 0x01000000;
	fwrite(&curfile, 0x12, 1, out);

	while(fgets(line, 1024, list)) {
		char *lptr = line, *lptr_new;
		int seemnum;
		int record;
		int seemlen;
		struct seemheader curseem;
		struct stat statbuf;
		char seemfile[128];
		FILE *inseem;
		char *inbuffer;

		if(strncmp(line, "SEEM ", 5) != 0) continue;
		lptr += 5;
		seemnum = strtoul(lptr, &lptr_new, 16);
		lptr = lptr_new + 1;
		lptr += 4;
		record = strtoul(lptr, &lptr_new, 16);

		sprintf(seemfile, "%4.4x_%4.4x.seem", seemnum, record);

		inseem = fopen(seemfile, "rb");
		if(!inseem) {
			fprintf(stderr, "Error opening seem %s.\n", seemfile);
		}

		stat(seemfile, &statbuf);
		seemlen = statbuf.st_size;

		curseem.unknown1 = seemlen + 16;
		curseem.skip_next = seemlen + 8;
		curseem.seem = seemnum;
		curseem.record = record;
		curseem.unknown3 = 0;
		curseem.length = seemlen;
		fwrite(&curseem, 14, 1, out);

		inbuffer = malloc(seemlen);
		fread(inbuffer, seemlen, 1, inseem);
		fwrite(inbuffer, seemlen, 1, out);
		free(inbuffer);

		fclose(inseem);
		numseems++;
	}

	fseek(out, 0, SEEK_SET);
	curfile.numseems = numseems;
	fwrite(&curfile, 0x12, 1, out);

	fclose(list);
	fclose(out);
	return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
	char line[16384];
	char *filename;
	char *outfilename = NULL;
	bool extract, create, list;

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
				printf("flex 1.0\nCopyright (C) 2008 Dustin Howett\nNo License\n\nWritten by Dustin Howett.\n");
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
		return extract_flex(filename);
	} else if(create) {
		if(outfilename == NULL) {
			fprintf(stderr, "You need to specify an output filename.\nSee %s --help for syntax help.\n", argv[0]);
			return EXIT_FAILURE;
		}
		return create_flex(outfilename);
	} else if(list) {
		return list_flex(filename);
	}

	return EXIT_SUCCESS;
}
