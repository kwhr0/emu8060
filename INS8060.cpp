// INS8060
// Copyright 2025 © Yasuo Kuwahara
// MIT License

#include "INS8060.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

char getsc(); // emulator

// Unimplemented: sio,dly,dae,dad

#define MIE 8
#define MOV 0x40
#define MCY 0x80

static constexpr uint8_t cycletbl[] = {
//   0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
	 8, 7, 5, 5, 6, 6, 5, 6, 5, 5, 5, 5, 5, 5, 5, 5, // 0x00
	 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, // 0x10
	 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, // 0x20
	 8, 8, 8, 8, 8, 8, 8, 8, 5, 5, 5, 5, 7, 7, 7, 7, // 0x30
	 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, // 0x40
	 6, 5, 5, 5, 5, 5, 5, 5, 6, 5, 5, 5, 5, 5, 5, 5, // 0x50
	 6, 5, 5, 5, 5, 5, 5, 5,11, 5, 5, 5, 5, 5, 5, 5, // 0x60
	 7, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, // 0x70
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,13, // 0x80
	11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11, // 0x90
	10,10,10,10,10,10,10,10,22,22,22,22,10,10,10,10, // 0xa0
	10,10,10,10,10,10,10,10,22,22,22,22,10,10,10,10, // 0xb0
	18,18,18,18,10,18,18,18,18,18,18,18,10,18,18,18, // 0xc0
	18,18,18,18,10,18,18,18,18,18,18,18,10,18,18,18, // 0xd0
	18,18,18,18,10,18,18,18,23,23,23,23,23,23,23,23, // 0xe0
	19,19,19,19,11,19,19,19,20,20,20,20,12,20,20,20, // 0xf0
};

INS8060::INS8060() {
#if INS8060_TRACE
	memset(tracebuf, 0, sizeof(tracebuf));
	tracep = tracebuf;
#endif
}

void INS8060::Reset() {
	ac = e = 0;
	sr = 0x20;
	p[0] = p[1] = p[2] = p[3] = 0;
	halted = false;
}

INS8060::u16 INS8060::ea(u8 op) {
	u16 r = 0;
	if ((op & 7) != 4) {
		s8 o = imm8();
		if (o == -128) o = e;
		if (op & 4) // auto index
			if (o < 0) p[op & 3] = r = addp(op, o);
			else { r = p[op & 3]; p[op & 3] = addp(op, o); }
		else r = addp(op, o); // index
	}
	return r;
}

void INS8060::add(u8 v) {
	int r  = ac + v + ((sr & MCY) != 0);
	if (((r & ~ac & ~v) | (~r & ac & v)) & 0x80) sr |= MOV;
	else sr &= ~MOV;
	if (((ac & v) | (v & ~r) | (~r & ac)) & 0x80) sr |= MCY;
	else sr &= ~MCY;
	ac = r;
}

static int input() {
	int c = getsc();
	if (!c) c = getchar();
	if (islower(c)) c = toupper(c);
	if (c == 10) c = 13;
	return c;
}

static void output(int c) {
	putchar(c);
}

#define br(cond)	if (cond) p[0] = addp(op, imm8());\
					else { p[0]++; cycle -= 2; }

int INS8060::Execute(int n) {
	n >>= 1;
	int cycle = 0;
	do {
#if INS8060_TRACE
		tracep->pc = p[0] + 1;
		tracep->index = tracep->opn = 0;
#endif
		u16 t16;
		u8 t8, op = imm8();
		switch (op) {
			case 0x00: halted = true; break; // halt
			case 0x01: std::swap(ac, e); break; // xae
			case 0x02: sr &= ~MCY; break; // ccl
			case 0x03: sr |= MCY; break; // scl
			case 0x04: sr &= ~MIE; break; // dint
			case 0x05: sr |= MIE; break; // ien
			case 0x06: ac = sr; break; // csa
			case 0x07: sr = ac; break; // cas
			case 0x08: break; // nop
				// 0x19 sio
			case 0x1c: ac >>= 1; break; // sr
			case 0x1d: ac = (sr & MCY) | ac >> 1; break; // srl
			case 0x1e: ac = ac << 7 | ac >> 1; break; // rr
			case 0x1f: t8 = ac; ac = (sr & MCY) | t8 >> 1; sr = t8 & 1 ? sr | MCY : sr & ~MCY; break; // rrl
			case 0x20: output(ac & 0x7f); break; // putc (emulator)
			case 0x21: ac = e = input(); break; // getc (emulator)
			case 0x30: case 0x31: case 0x32: case 0x33: std::swap(ac, (u8 &)p[op & 3]); break; // xpal
			case 0x34: case 0x35: case 0x36: case 0x37: std::swap(ac, *((u8 *)&p[op & 3] + 1)); break; // xpah
			case 0x3c: case 0x3d: case 0x3e: case 0x3f: std::swap(p[0], p[op & 3]); break; // xppc
			case 0x40: ac = e; break; // lde
			case 0x50: ac &= e; break; // ane
			case 0x58: ac |= e; break; // ore
			case 0x60: ac ^= e; break; // xre
				//68 dae
			case 0x70: add(e); break; // ade
			case 0x78: add(~e); break; // cae
				//8f dly
			case 0x90: case 0x91: case 0x92: case 0x93: br(true); break; // jmp
			case 0x94: case 0x95: case 0x96: case 0x97: br(!(ac & 0x80)); break; // jp
			case 0x98: case 0x99: case 0x9a: case 0x9b: br(!ac); break; // jz
			case 0x9c: case 0x9d: case 0x9e: case 0x9f: br(ac); break; // jnz
			case 0xa8: case 0xa9: case 0xaa: case 0xab: ac = ld(t16 = ea(op)) + 1; st(t16, ac); break; // ild
			case 0xb8: case 0xb9: case 0xba: case 0xbb: ac = ld(t16 = ea(op)) - 1; st(t16, ac); break; // dld
			case 0xc0: case 0xc1: case 0xc2: case 0xc3: case 0xc5: case 0xc6: case 0xc7:
				ac = ld(ea(op)); break; // ld
			case 0xc4: ac = imm8(); break; // ldi
			case 0xc8: case 0xc9: case 0xca: case 0xcb: case 0xcd: case 0xce: case 0xcf:
				st(ea(op), ac); break; // st
			case 0xd0: case 0xd1: case 0xd2: case 0xd3: case 0xd5: case 0xd6: case 0xd7:
				ac &= ld(ea(op)); break; // and
			case 0xd4: ac &= imm8(); break; // ani
			case 0xd8: case 0xd9: case 0xda: case 0xdb: case 0xdd: case 0xde: case 0xdf:
				ac |= ld(ea(op)); break; // or
			case 0xdc: ac |= imm8(); break; // ori
			case 0xe0: case 0xe1: case 0xe2: case 0xe3: case 0xe5: case 0xe6: case 0xe7:
				ac ^= ld(ea(op)); break; // xor
			case 0xe4: ac ^= imm8(); break; // xri
			//e8-ef dad
			case 0xf0: case 0xf1: case 0xf2: case 0xf3: case 0xf5: case 0xf6: case 0xf7:
				add(ld(ea(op))); break; // add
			case 0xf4: add(imm8()); break; // adi
			case 0xf8: case 0xf9: case 0xfa: case 0xfb: case 0xfd: case 0xfe: case 0xff:
				add(~ld(ea(op))); break; // cad
			case 0xfc: add(~imm8()); break; // cai
			default: if (op & 0x80) p[0]++;
#if INS8060_TRACE
				fprintf(stderr, "Illegal op: PC=%04x OP=%02x\n", p[0], op);
				StopTrace();
#endif
				break;
		}
#if INS8060_TRACE
		memcpy(tracep->p, p, sizeof(tracep->p));
		tracep->ac = ac;
		tracep->e = e;
		tracep->sr = sr;
#if INS8060_TRACE > 1
		if (++tracep >= tracebuf + TRACEMAX - 1) StopTrace();
#else
		if (++tracep >= tracebuf + TRACEMAX) tracep = tracebuf;
#endif
#endif
		cycle += cycletbl[op];
	} while (!halted && cycle < n);
	return halted ? 0 : (cycle - n) << 1;
}

#if INS8060_TRACE
#include <string>
void INS8060::StopTrace() {
	TraceBuffer *endp = tracep;
	int i = 0, j;
	FILE *fo;
	if (!(fo = fopen((std::string(getenv("HOME")) + "/Desktop/trace.txt").c_str(), "w"))) exit(1);
	do {
		if (++tracep >= tracebuf + TRACEMAX) tracep = tracebuf;
		fprintf(fo, "%4d %04X ", i++, tracep->pc);
		for (j = 0; j < 2; j++) fprintf(fo, j < tracep->opn ? "%02X " : "   ", tracep->op[j]);
		fprintf(fo, "%04X %04X %04X %04X ", tracep->p[0], tracep->p[1], tracep->p[2], tracep->p[3]);
		fprintf(fo, "%02X %02X ", tracep->ac, tracep->e);
		fprintf(fo, "%c%c ", tracep->sr & 0x80 ? 'C' : '-', tracep->sr & 0x40 ? 'V' : '-');
		for (Acs *p = tracep->acs; p < tracep->acs + tracep->index; p++) {
			switch (p->type) {
				case acsLoad:
					fprintf(fo, "L %04X %02X ", p->adr, p->data);
					break;
				case acsStore:
					fprintf(fo, "S %04X %02X ", p->adr, p->data);
					break;
			}
		}
		fprintf(fo, "\n");
	} while (tracep != endp);
	fclose(fo);
	fprintf(stderr, "trace dumped.\n");
	exit(1);
}
#endif	// INS8060_TRACE
