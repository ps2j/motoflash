#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>

FILE *out = NULL;
long last_addr = -256;
int last_len = 0;
int groupnum = 0;

char *fnformat = "Group %g - %a.bin";
bool srewind = false;

FILE *listfile = NULL;

static struct option options[] = {
	{"rewind", 0, NULL, 'r'},
	{"list", 1, NULL, 'l'},
	{"format", 1, NULL, 'f'},
	{"help", 0, NULL, 'h'},
	{"version", 0, NULL, 'V'},
	{0, 0, 0, 0}
};

void parse_srec(char *line) {
	int rectype = 0;
	int len = 0;
	int addr;
	int i;
	char temphex[100];
	char filename[100];

	rectype = line[0] - '0';
	line++;

	memset(temphex, 0, 100);
	memcpy(temphex, line, 2);
	len = strtoul(temphex, NULL, 16);

	line += 2;

	len -= 1;

	switch(rectype) {
		case 0:
			if(out) fclose(out);
			out = NULL;
			printf("Header: ");
			fprintf(listfile, "HEADER ");
			for(i = 0; i < len; i++) {
				char byte;
				memcpy(temphex, line, len * 2);
				temphex[2] = '\0';
				byte = strtoul(temphex, NULL, 16);
				line += 2;

				printf("%c", byte);
				fprintf(listfile, "%2.2x ", byte);
			}
			printf("\n");
			fprintf(listfile, "\n");
			break;
		case 3:
			memset(temphex, 0, 100);
			memcpy(temphex, line, 8);
			addr = strtoul(temphex, NULL, 16);
			line += 8;
			len -= 4;
			if(out && srewind && addr - last_addr < last_len) {
				int rew = last_len - (addr - last_addr);
				//fseek(out, -1 * rew, SEEK_CUR);
				printf("Rewind %d bytes.\n", rew);
				fprintf(listfile, "REWIND %d\n", rew);
			}
			if(!srewind && addr - last_addr != last_len || !out) {
				if(out) fclose(out);
				printf("Dumping address %8.8x.\n", addr);
				groupnum++;
				sprintf(filename, "Group %d - %8.8x.bin", groupnum, addr);
				fprintf(listfile, "CG %d %8.8x %s\n", groupnum, addr, filename);
				out = fopen(filename, "w+b");
			}
			//printf(".");
			for(i = 0; i < len; i++) {
				unsigned char byte;
				memcpy(temphex, line, len * 2);
				temphex[2] = '\0';
				byte = strtoul(temphex, NULL, 16);
				line += 2;

				fwrite(&byte, 1, 1, out);
			}
			last_len = len;
			last_addr = addr;
			break;
		case 7:
			memset(temphex, 0, 100);
			memcpy(temphex, line, 8);
			addr = strtoul(temphex, NULL, 16);
			line += 8;
			len -= 4;
			fprintf(listfile, "ENTRY %8.8x\n", addr);
			break;
		default:
			break;
	}
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

	int opt;
	while((opt = getopt_long(argc, argv, "hrl:f:V", options, NULL)) != -1) {
		switch(opt) {
			case 'f':
				fnformat = strdup(optarg);
				customformat = true;
				break;
			case 'h':
				usage(argv[0]);
				return EXIT_SUCCESS;
			case 'l':
				listfilename = strdup(optarg);
				customlistfile = true;
				break;
			case 'r':
				srewind = true;
				break;
			case 'V':
				printf("decompile 1.0\nCopyright (C) 2008 Dustin Howett\nNo License\n\nWritten by Dustin Howett.\n");
				return EXIT_SUCCESS;
			default:
				usage(argv[0]);
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

	listfile = fopen(customlistfile ? listfilename : "listfile", "w");
	if(customlistfile) free(listfilename);

	while(fgets(line, 16384, infile) != NULL) {
		if(line[0] != 'S') continue;
		else parse_srec(line + 1);
	}

	fclose(listfile);

	if(customformat) free(fnformat);
	free(infilename);

	return EXIT_SUCCESS;
}
