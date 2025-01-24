#include "INS8060.h"
#include "File.h"
#include <math.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <map>

#define INTERVAL		1
#define MARK			'&'

static INS8060 cpu;
static uint8_t m[0x10000];
static size_t srcIndex;
static std::string srcBuf;
static int entry;

char getsc() { return srcIndex < srcBuf.size() ? srcBuf[srcIndex++] : 0; }

static void LoadBinary(const char *path, int ofs) {
	FILE *fi = fopen(path, "rb");
	if (!fi) {
		fprintf(stderr, "open error: %s", path);
		exit(1);
	}
	fseek(fi, 0, SEEK_END);
	size_t len = ftell(fi);
	rewind(fi);
	if (ofs + len > 0x10000) len = 0x10000 - ofs;
	if (fread(&m[ofs], len, 1, fi) != 1) {
		fprintf(stderr, "read error: %s", path);
		exit(1);
	}
	entry = ofs;
}

static int getHex(char *&p, int n) {
	int r = 0;
	do {
		int c = *p++ - '0';
		if (c > 10) c -= 'A' - '0' - 10;
		if (c >= 0 && c < 16) r = r << 4 | c;
	} while (--n > 0);
	return r;
}

static void loadIntelHex(FILE *fi) {
	int ofs = 0;
	char s[256];
	while (fgets(s, sizeof(s), fi)) if (*s == ':') {
		char *p = s + 1;
		int n = getHex(p, 2), a = getHex(p, 4), t = getHex(p, 2);
		entry = a;
		if (!t)
			while (--n >= 0) {
				if (ofs + a < 0x10000) m[ofs + a++] = getHex(p, 2);
			}
		else if (t == 2)
			ofs = getHex(p, 4) << 4;
//		else if (t == 4)
//			ofs = getHex(p, 4) << 16;
		else break;
	}
}

static void preprocess() {
	std::map<std::string, int> labels;
	int c = 0, l = INTERVAL;
	// pass1
	do {
		c = chkch();
		if (c == '\n') c = _getc();
		else if (c == MARK) {
			_getc();
			std::string name = getword().c_str();
			labels[name] = l;
			printf("%s %d\n", name.c_str(), l);
		}
		else {
			l += INTERVAL;
			while ((c = _getc()))
				if (c == '\n') break;
		}
	} while (c);
	// pass2
	Rewind();
	char s[8];
	l = 0;
	do {
		c = chkch();
		if (c == '\n') c = _getc();
		else if (c == MARK) {
			while ((c = _getc()))
				if (c == '\n') break;
		}
		else if (c) {
			l += INTERVAL;
			snprintf(s, sizeof(s), "%d ", l);
			srcBuf += s;
			while ((c = _getc())) {
				if (c == MARK) {
					std::string name = getword();
					if (labels.count(name)) {
						snprintf(s, sizeof(s), "%d", labels[name]);
						srcBuf += s;
					}
				}
				else srcBuf += c;
				if (c == '\n') break;
			}
		}
	} while (c);
}

struct TimeSpec : timespec {
	TimeSpec() {}
	TimeSpec(double t) {
		tv_sec = floor(t);
		tv_nsec = long(1e9 * (t - tv_sec));
	}
	TimeSpec(time_t s, long n) {
		tv_sec = s;
		tv_nsec = n;
	}
	TimeSpec operator+(const TimeSpec &t) const {
		time_t s = tv_sec + t.tv_sec;
		long n = tv_nsec + t.tv_nsec;
		if (n < 0) {
			s++;
			n -= 1000000000;
		}
		return TimeSpec(s, n);
	}
	TimeSpec operator-(const TimeSpec &t) const {
		time_t s = tv_sec - t.tv_sec;
		long n = tv_nsec - t.tv_nsec;
		if (n < 0) {
			s--;
			n += 1000000000;
		}
		return TimeSpec(s, n);
	}
};

int main(int argc,char *argv[]) {
	if (argc <= 1) {
		fprintf(stderr, "Usage: emu8060 [-c <clock [MHz]>] [-b] [-r] [<binary file>] [<source file>]\n");
		return 0;
	}
	int mhz = 0;
	bool opt_b = false, opt_r = false;
	for (int c; (c = getopt(argc, argv, "bc:r")) != -1;) {
		std::string name, content;
		switch (c) {
			case 'c':
				sscanf(optarg, "%d", &mhz);
				break;
			case 'b':
				opt_b = true;
				break;
			case 'r':
				opt_r = opt_b = true;
				break;
		}
	}
	if (opt_b) {
		FILE *fi = fopen("NIBLFP.hex", "rb");
		if (!fi) {
			fprintf(stderr, "BASIC interpreter (NIBLFP.hex) not found.\n");
			exit(1);
		}
		loadIntelHex(fi);
		fclose(fi);
	}
	for (int i = optind; i < argc; i++) {
		const char *s = argv[i];
		if (strstr(s, ".bin")) LoadBinary(s, 0);
		else {
			LoadSource(s);
			if (isdigit(chkch())) {
				int c;
				while ((c = _getc())) srcBuf += c;
			}
			else preprocess();
			if (opt_r) srcBuf += "run\n";
		}
	}
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	(uint32_t &)m[0xcffc] = (uint32_t)ts.tv_sec; // for srand
	cpu.SetMemoryPtr(m);
	cpu.Reset();
	cpu.SetPC(entry);
	do {
		if (mhz) {
			const int SLICE = 100;
			TimeSpec tstart, tend;
			clock_gettime(CLOCK_MONOTONIC, &tstart);
			cpu.Execute(1000000L * mhz / SLICE);
			clock_gettime(CLOCK_MONOTONIC, &tend);
			TimeSpec duration = tstart + TimeSpec(1. / SLICE) - tend;
			if (duration.tv_sec >= 0)
				nanosleep(&duration, NULL);
		}
		else cpu.Execute(1000000);
	} while (!cpu.Halted());
	return 0;
}
