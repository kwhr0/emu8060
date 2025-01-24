#include "File.h"

#define kUngetMargin		0x10000

struct Buf {
	Buf() : buf(nullptr), rp(nullptr), linen(0) {}
	Buf(int size) : buf(new char[size]), rp(buf + kUngetMargin), linen(0) {}
	std::string path;
	char *buf, *rp;
	int linen;
};

static char *start, *filerp, *top;
static int lineNum;
static Buf *fileBuf;

static char *Open(const char *path) {
	FILE *fi;
	if (!(fi = fopen(path, "rb"))) {
		fprintf(stderr, "cannot open %s\n", path);
		exit(1);
	}
	fseek(fi, 0, SEEK_END);
	int size = (int)ftell(fi);
	rewind(fi);
	Buf *p = new Buf(size + kUngetMargin + 1);
	char *top = p->buf + kUngetMargin, *sp = top, *dp = top;
	fread(top, size, 1, fi);
	fclose(fi);
	int c, cf = 0;
	for (; sp < top + size; sp++)
		if ((c = *sp) != 13)
			switch (cf) {
				case 0:
					if (c == '/')
						if (sp[1] == '/') cf = -1;
						else if (sp[1] == '*') cf++;
						else *dp++ = c;
					else *dp++ = c;
					break;
				case -1:
					if (c != '\n') break;
					cf = 0;
					*dp++ = '\n';
					break;
				default:
					if (c == '\n') *dp++ = '\n';
					if (c != '*' || sp[1] != '/') break;
					if (!--cf) *dp++ = ' ';
					sp++;
					break;
			}
	if (cf) {
		fprintf(stderr, "comment not closed\n");
		exit(1);
	}
	*dp = 0;
	fileBuf = p;
	return top;
}

static char getcsub() {
	char c;
	while (!(c = *filerp++)) {
		filerp--;
		return 0;
	}
	return c;
}

char _getc() {
	char c = getcsub();
	if (c == '\n') lineNum++;
	return c;
}

void _ungetc(char c) {
	if (c) {
		if (filerp > start) *--filerp = c;
		else {
			fprintf(stderr, "buffer underflow\n");
			exit(1);
		}
		if (c == '\n') lineNum--;
	}
}
static char getch() {
	char c;
	while ((c = _getc()) && c <= ' ' && c != '\n')
		;
	return c;
}

char chkch() {
	char c = getch();
	_ungetc(c);
	return c;
}

void ungets(std::string s) {
	for (int i = (int)s.length() - 1; i >= 0; i--) _ungetc(s[i]);
}

std::string getword() {
	std::string r;
	if (char c = getch()) {
		if (isalpha(c) || c == '_') {
			r += c;
			for (c = _getc(); isalnum(c) || c == '_'; c = _getc()) r += c;
		}
		_ungetc(c);
	}
	return r;
}

void LoadSource(const char *sourcename) {
	top = filerp = Open(sourcename);
	start = fileBuf->buf;
	lineNum = 1;
}

void Rewind() {
	filerp = top;
	lineNum = 1;
}
