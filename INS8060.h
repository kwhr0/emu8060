// INS8060
// Copyright 2025 © Yasuo Kuwahara
// MIT License

#include <cstdint>

#define INS8060_TRACE			0

#if INS8060_TRACE
#define INS8060_TRACE_LOG(adr, data, type) \
	if (tracep->index < ACSMAX) tracep->acs[tracep->index++] = { adr, (u16)data, type }
#else
#define INS8060_TRACE_LOG(adr, data, type)
#endif

class INS8060 {
	using s8 = int8_t;
	using u8 = uint8_t;
	using u16 = uint16_t;
public:
	INS8060();
	void Reset();
	void SetPC(u16 v) { p[0] = v; }
	void SetMemoryPtr(u8 *p) { m = p; }
	int Execute(int n);
	bool Halted() const { return halted; }
private:
	u8 imm8() {
		u8 o = m[++p[0]];
#if INS8060_TRACE
		if (tracep->opn < 2) tracep->op[tracep->opn++] = o;
#endif
		return o;
	}
	u8 ld(u16 adr) {
		u8 data = m[adr];
		INS8060_TRACE_LOG(adr, data, acsLoad);
		return data;
	}
	void st(u16 adr, u8 data) {
		m[adr] = data;
		INS8060_TRACE_LOG(adr, data, acsStore);
	}
	//
	u8 *m;
	u16 p[4];
	u8 ac, e, sr;
	bool halted;
	//
	u16 addp(u8 op, s8 o) { u16 t = p[op & 3]; return (t & 0xf000) | (t + o & 0xfff); }
	u16 ea(u8 op);
	void add(u8 v);
#if INS8060_TRACE
	static constexpr int TRACEMAX = 10000;
	static constexpr int ACSMAX = 1;
	enum {
		acsLoad = 1, acsStore
	};
	struct Acs {
		u16 adr, data;
		u8 type;
	};
	struct TraceBuffer {
		u16 pc;
		u8 op[2];
		u16 p[4];
		u8 ac, e, sr, index, opn;
		Acs acs[ACSMAX];
	};
	TraceBuffer tracebuf[TRACEMAX];
	TraceBuffer *tracep;
public:
	void StopTrace();
#endif
};
