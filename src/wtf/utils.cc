// Axel '0vercl0k' Souchet - February 26 2020
#include "utils.h"
#include "backend.h"
#include "blake3.h"
#include "debugger.h"
#include "nlohmann/json.hpp"
#include "platform.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fmt/format.h>
#include <fstream>
#include <limits>

namespace json = nlohmann;

bool CompareTwoFileBySize(const fs::path &A, const fs::path &B) {
  return fs::file_size(A) < fs::file_size(B);
}

void Hexdump(const span_u8 Buffer) {
  Hexdump(0, Buffer.data(), Buffer.size_bytes());
}

void Hexdump(const uint64_t Address, const span_u8 Buffer) {
  Hexdump(Address, Buffer.data(), Buffer.size_bytes());
}

//
// Copied from https://github.com/pvachon/tsl/blob/master/tsl/hexdump.c.
//

void Hexdump(const uint64_t Address, const void *Buffer, size_t Len) {
  const uint8_t *ptr = (uint8_t *)Buffer;

  for (size_t i = 0; i < Len; i += 16) {
    fmt::print("{:#016x}: ", Address + i);
    for (size_t j = 0; j < 16; j++) {
      if (i + j < Len) {
        fmt::print("{:02x} ", ptr[i + j]);
      } else {
        fmt::print("   ");
      }
    }
    fmt::print(" |");
    for (size_t j = 0; j < 16; j++) {
      if (i + j < Len) {
        fmt::print("{}", isprint(ptr[i + j]) ? ptr[i + j] : '.');
      } else {
        fmt::print(" ");
      }
    }
    fmt::print("|\n");
  }
}

bool LoadCpuStateFromJSON(CpuState_t &CpuState, const fs::path &CpuStatePath) {
  std::ifstream File(CpuStatePath);
  json::json Json;
  File >> Json;

  memset(&CpuState, 0, sizeof(CpuState));

#define REGISTER(_Bdmp_, _Wtf_)                                                \
  {                                                                            \
    CpuState._Wtf_ = decltype(CpuState._Wtf_)(                                 \
        std::strtoull(Json[#_Bdmp_].get<std::string>().c_str(), nullptr, 0));  \
  }

  REGISTER(rax, Rax)
  REGISTER(rbx, Rbx)
  REGISTER(rcx, Rcx)
  REGISTER(rdx, Rdx)
  REGISTER(rsi, Rsi)
  REGISTER(rdi, Rdi)
  REGISTER(rip, Rip)
  REGISTER(rsp, Rsp)
  REGISTER(rbp, Rbp)
  REGISTER(r8, R8)
  REGISTER(r9, R9)
  REGISTER(r10, R10)
  REGISTER(r11, R11)
  REGISTER(r12, R12)
  REGISTER(r13, R13)
  REGISTER(r14, R14)
  REGISTER(r15, R15)
  REGISTER(rflags, Rflags)
  REGISTER(tsc, Tsc)
  REGISTER(apic_base, ApicBase)
  REGISTER(sysenter_cs, SysenterCs)
  REGISTER(sysenter_esp, SysenterEsp)
  REGISTER(sysenter_eip, SysenterEip)
  REGISTER(pat, Pat)
  REGISTER(efer, Efer.Flags)
  REGISTER(star, Star)
  REGISTER(lstar, Lstar)
  REGISTER(cstar, Cstar)
  REGISTER(sfmask, Sfmask)
  REGISTER(kernel_gs_base, KernelGsBase)
  REGISTER(tsc_aux, TscAux)
  REGISTER(fpcw, Fpcw)
  REGISTER(fpsw, Fpsw)
  REGISTER(fptw, Fptw)
  REGISTER(cr0, Cr0.Flags)
  REGISTER(cr2, Cr2)
  REGISTER(cr3, Cr3)
  REGISTER(cr4, Cr4.Flags)
  REGISTER(cr8, Cr8)
  REGISTER(xcr0, Xcr0)
  REGISTER(dr0, Dr0)
  REGISTER(dr1, Dr1)
  REGISTER(dr2, Dr2)
  REGISTER(dr3, Dr3)
  REGISTER(dr6, Dr6)
  REGISTER(dr7, Dr7)
  REGISTER(mxcsr, Mxcsr)
  REGISTER(mxcsr_mask, MxcsrMask)
  REGISTER(fpop, Fpop)
#undef REGISTER

#define SEGMENT(_Bdmp_, _Wtf_)                                                 \
  {                                                                            \
    CpuState._Wtf_.Present = Json[#_Bdmp_]["present"].get<bool>();             \
    CpuState._Wtf_.Selector = decltype(CpuState._Wtf_.Selector)(std::strtoull( \
        Json[#_Bdmp_]["selector"].get<std::string>().c_str(), nullptr, 0));    \
    CpuState._Wtf_.Base = std::strtoull(                                       \
        Json[#_Bdmp_]["base"].get<std::string>().c_str(), nullptr, 0);         \
    CpuState._Wtf_.Limit = decltype(CpuState._Wtf_.Limit)(std::strtoull(       \
        Json[#_Bdmp_]["limit"].get<std::string>().c_str(), nullptr, 0));       \
    CpuState._Wtf_.Attr = decltype(CpuState._Wtf_.Attr)(std::strtoull(         \
        Json[#_Bdmp_]["attr"].get<std::string>().c_str(), nullptr, 0));        \
  }

  SEGMENT(es, Es)
  SEGMENT(cs, Cs)
  SEGMENT(ss, Ss)
  SEGMENT(ds, Ds)
  SEGMENT(fs, Fs)
  SEGMENT(gs, Gs)
  SEGMENT(tr, Tr)
  SEGMENT(ldtr, Ldtr)
#undef SEGMENT

#define GLOBALSEGMENT(_Bdmp_, _Wtf_)                                           \
  {                                                                            \
    CpuState._Wtf_.Base = decltype(CpuState._Wtf_.Base)(std::strtoull(         \
        Json[#_Bdmp_]["base"].get<std::string>().c_str(), nullptr, 0));        \
    CpuState._Wtf_.Limit = decltype(CpuState._Wtf_.Limit)(std::strtoull(       \
        Json[#_Bdmp_]["limit"].get<std::string>().c_str(), nullptr, 0));       \
  }

  GLOBALSEGMENT(gdtr, Gdtr)
  GLOBALSEGMENT(idtr, Idtr)
#undef GLOBALSEGMENT

  for (uint64_t Idx = 0; Idx < 8; Idx++) {
    const std::string &Value = Json["fpst"][Idx].get<std::string>();
    if (Value == "0x-Infinity") {
      CpuState.Fpst[Idx] = 0;
    } else {
      CpuState.Fpst[Idx] = std::strtoull(Value.c_str(), nullptr, 0);
    }
  }

  return true;
}

bool SanitizeCpuState(CpuState_t &CpuState) {

  //
  // Force CR8 to 0 if RIP is usermode.
  //

  if (CpuState.Rip < 0x7fffffff0000ULL && CpuState.Cr8 != 0) {
    CpuState.Cr8 = 0;
    fmt::print("Force cr8 to 0 as rip is in usermode.\n");
  }

  //
  // Nullify DR0-DR3 if breakpoints were defined.
  //

  uint64_t *HardwareBreakpoints[] = {&CpuState.Dr0, &CpuState.Dr1,
                                     &CpuState.Dr2, &CpuState.Dr3};

  uint32_t *Drs[] = {&CpuState.Dr6, &CpuState.Dr7};

  for (uint64_t *Ptr : HardwareBreakpoints) {
    if (*Ptr != 0) {
      fmt::print("Setting debug register to zero.\n");
      *Ptr = 0;
    }
  }

  for (uint32_t *Ptr : Drs) {
    if (*Ptr != 0) {
      fmt::print("Setting debug register status to zero.\n");
      *Ptr = 0;
    }
  }

  //
  // Validate that the Reserved field of each segment contains bits 16-19 of the
  // Limit
  //
  /*
  Seg_t *Segments[] = {&CpuState.Es, &CpuState.Fs, &CpuState.Cs,
                       &CpuState.Gs, &CpuState.Ss, &CpuState.Ds};

  for (Seg_t *Seg : Segments) {
    if (Seg->Reserved != ((Seg->Limit >> 16) & 0xF)) {
      fmt::print("Segment with selector {:x} has invalid attributes.\n",
                 Seg->Selector);
      return false;
    }
  }
  */
  return true;
}

std::unique_ptr<uint8_t[]> ReadFile(const fs::path &Path, size_t &FileSize) {
  std::ifstream File(Path, std::ios::in | std::ios::binary);
  if (!File.is_open()) {
    return nullptr;
  }

  File.seekg(0, File.end);
  FileSize = File.tellg();
  File.seekg(0, File.beg);

  auto Buffer = std::make_unique<uint8_t[]>(FileSize);
  if (Buffer == nullptr) {
    return nullptr;
  }

  File.read((char *)Buffer.get(), FileSize);
  return Buffer;
}

std::string Blake3HexDigest(const uint8_t *Data, const size_t DataSize) {
  const size_t HashSize = 16;
  const char HexDigits[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                            '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
  uint8_t Hash[HashSize];
  blake3_hasher Hasher;
  blake3_hasher_init(&Hasher);
  blake3_hasher_update(&Hasher, Data, DataSize);
  blake3_hasher_finalize(&Hasher, Hash, HashSize);

  std::string Hexdigest;
  Hexdigest.reserve(HashSize * 2);
  for (size_t Idx = 0; Idx < HashSize; Idx++) {
    const uint8_t Byte = Hash[Idx];
    const uint8_t Low = Byte & 0xf;
    const uint8_t High = Byte >> 4;
    Hexdigest.push_back(HexDigits[Low]);
    Hexdigest.push_back(HexDigits[High]);
  }

  return Hexdigest;
}

const Gva_t DecodePointer(const uint64_t Cookie, const uint64_t Value) {
  return Gva_t(std::rotr(Value, 0x40 - (Cookie & 0x3F)) ^ Cookie);
}

std::string u16stringToString(const std::u16string &S) {
  std::string Out;
  for (const char16_t &C : S) {
    Out.push_back(char(C));
  }
  return Out;
}

std::optional<tsl::robin_map<Gva_t, Gpa_t>>
ParseCovFiles(const Backend_t &Backend, const fs::path &CovFilesDir) {
  tsl::robin_map<Gva_t, Gpa_t> CovBreakpoints;

  //
  // Iterate through the files in the directory.
  //

  const fs::directory_iterator DirIt(CovFilesDir);
  for (const auto &CovFile : DirIt) {
    if (!CovFile.path().string().ends_with(".cov")) {
      continue;
    }

    //
    // Read the file.
    //

    fmt::print("Parsing {}..\n", CovFile.path().string());
    std::ifstream File(CovFile.path());
    json::json Json;
    File >> Json;

    const std::string &ModuleName = Json["name"].get<std::string>();
    const uint64_t Base = g_Dbg.GetModuleBase(ModuleName.c_str());
    if (Base == 0) {
      fmt::print("Failed to find the base of {}\n", ModuleName);
      return std::nullopt;
    }

    for (const auto &Item : Json["addresses"]) {
      const uint64_t Rva = Item.get<uint64_t>();
      const Gva_t Gva = Gva_t(Base + Rva);

      //
      // To set a breakpoint, we need to know the GPA associated with it, so
      // let's get it.
      //

      Gpa_t Gpa;
      const bool Translate = Backend.VirtTranslate(
          Gva, Gpa, MemoryValidate_t::ValidateReadExecute);

      if (!Translate) {
        fmt::print("Failed to translate GVA {:#x}, skipping..\n", Gva);
        continue;
      }

      CovBreakpoints.emplace(Gva, Gpa);
    }
  }

  return CovBreakpoints;
}

std::optional<bool> SaveFile(const fs::path &Path, const uint8_t *Buffer,
                             const size_t BufferSize) {

  //
  // If the file already exists, let's not write anything.
  //

  if (fs::exists(Path)) {
    return false;
  }

  //
  // Time to write the file!
  //

  FILE *OutputFile = fopen(Path.string().c_str(), "wb");
  if (OutputFile == nullptr) {

    //
    // This is an error, so let's return nullopt.
    //

    fmt::print("Could not create the destination file.\n");
    return std::nullopt;
  }

  //
  // Write the content.
  //

  fwrite(Buffer, BufferSize, 1, OutputFile);
  fclose(OutputFile);
  return true;
}

//
// Utility that calculates the number of seconds sine a time point.
//

[[nodiscard]] chrono::seconds
SecondsSince(const chrono::system_clock::time_point &Since) {
  const auto &Now = chrono::system_clock::now();
  return chrono::duration_cast<chrono::seconds>(Now - Since);
}

//
// Utility that is used to print seconds for human.
//

[[nodiscard]] std::pair<double, const char *>
SecondsToHuman(const chrono::seconds &Seconds) {
  const char *Unit = "s";
  double SecondsNumber = Seconds.count();
  if (SecondsNumber > 86'400) {
    Unit = "d";
    SecondsNumber /= 86'400;
  } else if (SecondsNumber > 3'600) {
    Unit = "hr";
    SecondsNumber /= 3'600;
  } else if (SecondsNumber > 60) {
    Unit = "min";
    SecondsNumber /= 60;
  }

  return std::make_pair(SecondsNumber, Unit);
}

//
// Utility that is used to print bytes for human.
//

[[nodiscard]] std::pair<double, const char *>
BytesToHuman(const uint64_t Bytes_) {
  const char *Unit = "b";
  double Bytes = double(Bytes_);
  if (Bytes > 1'048'576) {
    Unit = "mb";
    Bytes /= 1'048'576;
  } else if (Bytes > 1024) {
    Unit = "kb";
    Bytes /= 1024;
  }

  return std::make_pair(Bytes, Unit);
}

[[nodiscard]] std::pair<double, const char *> NumberToHuman(const double N_) {
  const char *Unit = "";
  double N = N_;
  if (N > 1'000'000) {
    Unit = "m";
    N /= 1'000'000;
  } else if (N > 1000) {
    Unit = "k";
    N /= 1000;
  }

  return std::make_pair(N, Unit);
}

[[nodiscard]] std::string_view
ExceptionCodeToStr(const uint32_t ExceptionCode) {
  switch (ExceptionCode) {
  case EXCEPTION_ACCESS_VIOLATION:
    return "EXCEPTION_ACCESS_VIOLATION";
  case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
  case EXCEPTION_BREAKPOINT:
    return "EXCEPTION_BREAKPOINT";
  case EXCEPTION_DATATYPE_MISALIGNMENT:
    return "EXCEPTION_DATATYPE_MISALIGNMENT";
  case EXCEPTION_FLT_DENORMAL_OPERAND:
    return "EXCEPTION_FLT_DENORMAL_OPERAND";
  case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
  case EXCEPTION_FLT_INEXACT_RESULT:
    return "EXCEPTION_FLT_INEXACT_RESULT";
  case EXCEPTION_FLT_INVALID_OPERATION:
    return "EXCEPTION_FLT_INVALID_OPERATION";
  case EXCEPTION_FLT_OVERFLOW:
    return "EXCEPTION_FLT_OVERFLOW";
  case EXCEPTION_FLT_STACK_CHECK:
    return "EXCEPTION_FLT_STACK_CHECK";
  case EXCEPTION_FLT_UNDERFLOW:
    return "EXCEPTION_FLT_UNDERFLOW";
  case EXCEPTION_ILLEGAL_INSTRUCTION:
    return "EXCEPTION_ILLEGAL_INSTRUCTION";
  case EXCEPTION_IN_PAGE_ERROR:
    return "EXCEPTION_IN_PAGE_ERROR";
  case EXCEPTION_INT_DIVIDE_BY_ZERO:
    return "EXCEPTION_INT_DIVIDE_BY_ZERO";
  case EXCEPTION_INT_OVERFLOW:
    return "EXCEPTION_INT_OVERFLOW";
  case EXCEPTION_INVALID_DISPOSITION:
    return "EXCEPTION_INVALID_DISPOSITION";
  case EXCEPTION_NONCONTINUABLE_EXCEPTION:
    return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
  case EXCEPTION_PRIV_INSTRUCTION:
    return "EXCEPTION_PRIV_INSTRUCTION";
  case EXCEPTION_SINGLE_STEP:
    return "EXCEPTION_SINGLE_STEP";
  case EXCEPTION_STACK_OVERFLOW:
    return "EXCEPTION_STACK_OVERFLOW";
  case STATUS_STACK_BUFFER_OVERRUN:
    return "EXCEPTION_STACK_BUFFER_OVERRUN";

  case EXCEPTION_ACCESS_VIOLATION_READ:
    return "EXCEPTION_ACCESS_VIOLATION_READ";
  case EXCEPTION_ACCESS_VIOLATION_WRITE:
    return "EXCEPTION_ACCESS_VIOLATION_WRITE";
  case EXCEPTION_ACCESS_VIOLATION_EXECUTE:
    return "EXCEPTION_ACCESS_VIOLATION_EXECUTE";
  }
  return "UNKNOWN";
}
