// bftool: disassemble / execute original OP-1 Blackfin code (via bcore).
//
//   bftool dis  IMG START END            linear disassembly of IMG (flat, base 0)
//   bftool run  IMG PC [opts]            call the function at PC like the
//                                        firmware would, return at RETS sentinel
//     --reg NAME=VAL      R0..R7, P0..P5, SP, FP  (hex or dec)
//     --load ADDR FILE    copy FILE into guest memory at ADDR
//     --poke32 ADDR=VAL
//     --dump ADDR LEN FILE  write guest memory to FILE after the call
//     --max-runs N        abort after N basic blocks (default 50e6)
//     --trace             print each executed block entry PC
//     --bench N           call it N times (timing only)
// The image is a flat dump of guest memory starting at address 0 (see
// tools/ldr/ldrimg.py). Nothing outside the image is mapped: any access to
// L1 / MMRs that bcore does not model is reported as a fault.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include "core.h"
#include "flat_memory.h"

static std::vector<uint8_t> readf(const char* p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) { fprintf(stderr, "cannot read %s\n", p); exit(2); }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}
static uint32_t num(const std::string& s) { return (uint32_t)strtoul(s.c_str(), nullptr, 0); }

static constexpr uint32_t MEM_SIZE = 0x08000000;     // 128 MiB flat guest space
static constexpr uint32_t STACK_TOP = 0x07FFF000;
static constexpr uint32_t RET_SENTINEL = 0x07FFFF00;  // RETS: stop when PC lands here

int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "see source header for usage\n"); return 2; }
    std::string cmd = argv[1];
    auto img = readf(argv[2]);
    CpuState cpu; cpu_state_init(&cpu);
    FlatMemory mem(0, MEM_SIZE, &cpu);
    mem.load(0, img.data(), (uint32_t)std::min<size_t>(img.size(), MEM_SIZE));
    Core core(&cpu, &mem);
    if (cmd == "dis") {
        uint32_t a = num(argv[3]), end = num(argv[4]);
        while (a < end) {
            auto [t, n] = core.disassemble(a);
            printf("%08x: ", a);
            for (uint32_t i = a; i < n && i < a + 8; i += 2) printf("%04x ", mem.read16(i));
            printf("%*s%s\n", (int)(4 - (n - a) / 2) * 5 + 1, "", t.c_str());
            a = n > a ? n : a + 2;
        }
        return 0;
    }
    if (cmd != "run") return 2;
    uint32_t pc = num(argv[3]);
    uint64_t max_runs = 50000000, bench = 0; bool trace = false;
    struct Dump { uint32_t a, n; std::string f; }; std::vector<Dump> dumps;
    cpu.dpregs[14] = STACK_TOP; cpu.dpregs[15] = STACK_TOP;
    for (int i = 4; i < argc; i++) {
        std::string o = argv[i];
        if (o == "--reg") {
            std::string kv = argv[++i]; auto e = kv.find('=');
            std::string k = kv.substr(0, e); uint32_t v = num(kv.substr(e + 1));
            if (k[0] == 'R') cpu.dpregs[k[1] - '0'] = v;
            else if (k[0] == 'P') cpu.dpregs[8 + k[1] - '0'] = v;
            else if (k == "SP") cpu.dpregs[14] = v; else if (k == "FP") cpu.dpregs[15] = v;
        } else if (o == "--load") { uint32_t a = num(argv[++i]); auto b = readf(argv[++i]); mem.load(a, b.data(), b.size()); }
        else if (o == "--poke32") { std::string kv = argv[++i]; auto e = kv.find('='); mem.write32(num(kv.substr(0, e)), num(kv.substr(e + 1))); }
        else if (o == "--dump") { uint32_t a = num(argv[++i]); uint32_t n = num(argv[++i]); dumps.push_back({a, n, argv[++i]}); }
        else if (o == "--max-runs") max_runs = strtoull(argv[++i], nullptr, 0);
        else if (o == "--trace") trace = true;
        else if (o == "--bench") bench = strtoull(argv[++i], nullptr, 0);
    }
    if (!core.init(2)) { fprintf(stderr, "JIT init failed\n"); return 1; }
    CpuState start = cpu;
    auto call = [&](bool verbose) -> int {
        cpu = start; cpu.rets = RET_SENTINEL; cpu.pc = pc;
        uint64_t runs = 0, packets = 0;
        while (cpu.pc != RET_SENTINEL && !cpu.halted) {
            if (trace && verbose) fprintf(stderr, "bb %08x\n", cpu.pc);
            if (!core.run(cpu.pc)) { fprintf(stderr, "run failed at %08x\n", cpu.pc); return 1; }
            packets += cpu.packets;
            if (++runs > max_runs) { fprintf(stderr, "max runs exceeded pc=%08x\n", cpu.pc); return 1; }
        }
        if (verbose) {
            printf("returned=%d runs=%llu packets=%llu\n", cpu.pc == RET_SENTINEL, (unsigned long long)runs, (unsigned long long)packets);
            for (int r = 0; r < 8; r++) printf("R%d=0x%08x%c", r, cpu.dpregs[r], r == 7 ? '\n' : ' ');
            for (int r = 0; r < 6; r++) printf("P%d=0x%08x%c", r, cpu.dpregs[8 + r], r == 5 ? '\n' : ' ');
            printf("SP=0x%08x A0=0x%02x%08x A1=0x%02x%08x CC=%u\n", cpu.dpregs[14], cpu.ax[0] & 0xff, cpu.aw[0], cpu.ax[1] & 0xff, cpu.aw[1], cpu.cc);
        }
        return 0;
    };
    // Snapshot memory regions to restore between bench calls is the caller's
    // job: only use --bench on functions that do not modify their inputs.
    int rc = call(true);
    for (auto& d : dumps) {
        std::vector<uint8_t> b(d.n);
        for (uint32_t k = 0; k < d.n; k++) b[k] = mem.read8(d.a + k);
        std::ofstream(d.f, std::ios::binary).write((char*)b.data(), b.size());
    }
    if (bench) {
        auto t0 = std::chrono::steady_clock::now();
        for (uint64_t k = 0; k < bench; k++) call(false);
        double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        printf("bench calls=%llu seconds=%.6f us_per_call=%.3f\n", (unsigned long long)bench, s, s * 1e6 / bench);
    }
    return rc;
}
