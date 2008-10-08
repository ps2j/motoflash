#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>

FILE *out = NULL;

static struct option options[] = {
	{"output", 1, NULL, 'o'},
	{"help", 0, NULL, 'h'},
	{"version", 0, NULL, 'V'},
	{0, 0, 0, 0}
};

int sum_addr(long addr) {
	return ((addr & 0xFF000000) >> 24) +
		((addr & 0xFF0000) >> 16) +
		((addr & 0xFF00) >> 8) +
		(addr & 0xFF);
}

void write_cg(long addr, char *filename) {
	char outline[1024];
	int sizeleft;
	struct stat buf;
	FILE *cg;
	int process;
	unsigned char linelen, checksum, byte;

	stat(filename, &buf);
	sizeleft = buf.st_size;
	cg = fopen(filename, "rb");

	while(sizeleft > 0) {
		process = (sizeleft > 16 ? 16 : sizeleft);
		linelen = process + 5;
		checksum = 0;

		sizeleft -= process;
		memset(outline, 0, 1024);
		sprintf(outline, "S3%2.2x%8.8x", linelen, addr);
		checksum += sum_addr(addr);
		checksum += linelen;
		addr += process;
		while(process > 0) {
			fread(&byte, 1, 1, cg);
			checksum += byte;
			sprintf(outline, "%s%2.2x", outline, byte);
			process--;
		}
		sprintf(outline, "%s%2.2x", outline, (~checksum) & 0xFF);
		fprintf(out, "%s\n", outline);
	}
}

void write_entry(long addr) {
	int len = 5;
	int checksum = len + sum_addr(addr);
	char outline[100];
	fprintf(out, "S7%2.2x%8.8x%2.2x\n", len, addr, (~checksum) & 0xFF);
}

//void write_hdr

FILE *listfile = NULL;

void usage(char *basename) {
	printf("Syntax: %s [-o outfile] [file]\n", basename);
	printf(
"  -o, --output file		set the s-record output filename; if not set, s-records are printed to stdout\n\
  -h, --help			display this help text\n\
  -V, --version			display version information\n\
  file				input filename; if not set, \"listfile\" is assumed\n\nReport bugs to <dustin@howett.net>.\n");
}

int main(int argc, char **argv) {
	char line[16384];
	char *lineptr;
	char *newptr;
	char *listfilename = NULL;
	char *outfilename = NULL;
	bool customoutfilename = false;
	long addr;
	int groupnum = 0;

	int opt;
	while((opt = getopt_long(argc, argv, "ho:V", options, NULL)) != -1) {
		switch(opt) {
			case 'h':
				usage(argv[0]);
				return EXIT_SUCCESS;
			case 'o':
				outfilename = strdup(optarg);
				customoutfilename = true;
				break;
			case 'V':
				printf("compile 1.0\nCopyright (C) 2008 Dustin Howett\nNo License\n\nWritten by Dustin Howett.\n");
				return EXIT_SUCCESS;
			default:
				usage(argv[0]);
				return EXIT_FAILURE;
				break;
		}
	}

	if(customoutfilename) {
		out = fopen(outfilename, "w+");
	} else {
		out = stdout;
	}

	if(optind < argc) {
		listfilename = strdup(argv[optind]);
		fprintf(stderr, "Reading: %s\n", listfilename);
	} else {
		fprintf(stderr, "Reading: listfile\n");
	}

	listfile = fopen(listfilename != NULL ? listfilename : "listfile", "r");
	if(listfile == NULL) {
		fprintf(stderr, "Error opening %s.\n", listfilename != NULL ? listfilename : "listfile");
		return EXIT_FAILURE;
	}

	while(fgets(line, 16384, listfile) != NULL) {
		line[strlen(line) - 1] = '\0';
		lineptr = line;
		if(!strncmp(lineptr, "CG ", 3)) {
			int data = 0;
			lineptr += 3;
			groupnum = strtoul(lineptr, &newptr, 10);
			lineptr = newptr + 1;
			addr = strtoul(lineptr, &newptr, 16);
			lineptr = newptr + 1;
			fprintf(stderr, "Group %d data at %8.8x and is in file \"%s\"\n", groupnum, addr, lineptr);
			write_cg(addr, lineptr);
		} else if(!strncmp(lineptr, "ENTRY ", 6)) {
			lineptr += 6;
			addr = strtoul(lineptr, &newptr, 16);
			fprintf(stderr, "Entry at %8.8x.\n", addr);
			write_entry(addr);
		}
	}
	fclose(listfile);

	return EXIT_SUCCESS;
}
