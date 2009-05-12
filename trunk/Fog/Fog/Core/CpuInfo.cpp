// [Fog/Core Library - C++ API]
//
// [Licence] 
// MIT, See COPYING file in package

// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// [Precompiled headers]
#if defined(FOG_PRECOMP)
#include FOG_PRECOMP
#endif // FOG_PRECOMP

// [Dependencies]
#include <Fog/Core/Assert.h>
#include <Fog/Core/Constants.h>
#include <Fog/Core/CpuInfo.h>
#include <Fog/Core/Error.h>

#if defined(FOG_OS_WINDOWS)
# include <windows.h>
#endif // FOG_OS_WINDOWS

// 2009-02-05: Thanks to Mike Tajmajer for supporting VC7.1 compiler. This
// shouldn't affect x64 compilation, because x64 compiler starts with
// VS2005 (VC8.0).
#if defined(_MSC_VER)
# if _MSC_VER >= 1400
#  include <intrin.h>
# endif // _MSC_VER >= 1400 (>= VS2005)
#endif // _MSC_VER

#if defined(FOG_OS_POSIX)
# include <errno.h>
# include <string.h>
# include <sys/statvfs.h>
# include <sys/utsname.h>
# include <unistd.h>
#endif // FOG_OS_POSIX

namespace Fog {

// [Fog::Cpu]

static uint32_t detectNumberOfProcessors(void)
{
#if defined(FOG_OS_WINDOWS)
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  return info.dwNumberOfProcessors;
#elif defined(FOG_OS_POSIX)
  // It seems that sysconf returns the number of "logical" processors on both
  // mac and linux.  So we get the number of "online logical" processors.
  long res = sysconf(_SC_NPROCESSORS_ONLN);
  if (res == -1)
  {
    FOG_ASSERT(0);
    return 1;
  }

  return static_cast<uint32_t>(res);
#else
  return 1;
#endif
}

FOG_CVAR_DECLARE CpuInfo* cpuInfo;

#if defined(FOG_ARCH_X86) || defined(FOG_ARCH_X86_64)
void cpuid(uint32_t in, CpuId* out)
{
#if defined(_MSC_VER)
# if _MSC_VER >= 1400
  // done by intrinsics
  __cpuid(reinterpret_cast<int*>(out->i), in);
# else // _MSC_VER < 1400
  uint32_t cpuid_in = in;
  uint32_t* cpuid_out = out->i;

  __asm
  {
    mov     eax, cpuid_in
    mov     edi, cpuid_out
    cpuid
    mov     dword ptr[edi +  0], eax
    mov     dword ptr[edi +  4], ebx
    mov     dword ptr[edi +  8], ecx
    mov     dword ptr[edi + 12], edx
  }
# endif // _MSC_VER < 1400

#elif defined(__GNUC__)

// Note, need to preserve ebx/rbx register!
# if defined(ASMJIT_X86)
#  define __mycpuid(a, b, c, d, inp) \
  asm ("mov %%ebx, %%edi\n"    \
       "cpuid\n"               \
       "xchg %%edi, %%ebx\n"   \
       : "=a" (a), "=D" (b), "=c" (c), "=d" (d) : "a" (inp))
# else
#  define __mycpuid(a, b, c, d, inp) \
  asm ("mov %%rbx, %%rdi\n"    \
       "cpuid\n"               \
       "xchg %%rdi, %%rbx\n"   \
       : "=a" (a), "=D" (b), "=c" (c), "=d" (d) : "a" (inp))
# endif
  __mycpuid(out->eax, out->ebx, out->ecx, out->edx, in);

#endif // compiler
}
#endif // FOG_ARCH_X86) || FOG_ARCH_X86_64

void detectCpuInfo(CpuInfo* i)
{
  uint32_t a;

  // First clear our struct
  memset(i, 0, sizeof(CpuInfo));

  memcpy(i->vendor, "Unknown", 8);
  i->numberOfProcessors = detectNumberOfProcessors();

  // First clear our struct
  memset(i, 0, sizeof(CpuInfo));
  memcpy(i->vendor, "Unknown", 8);

  i->numberOfProcessors = detectNumberOfProcessors();

#if defined(FOG_ARCH_X86) || defined(FOG_ARCH_X64)
  CpuId out;

  // Get vendor string
  cpuid(0, &out);

  memcpy(i->vendor, &out.ebx, 4);
  memcpy(i->vendor + 4, &out.edx, 4);
  memcpy(i->vendor + 8, &out.ecx, 4);

  // get feature flags in ecx/edx, and family/model in eax
  cpuid(1, &out);

  // family and model fields
  i->family   = (out.eax >> 8) & 0x0F;
  i->model    = (out.eax >> 4) & 0x0F;
  i->stepping = (out.eax     ) & 0x0F;

  // use extended family and model fields
  if (i->family == 0x0F)
  {
    i->family += ((out.eax >> 20) & 0xFF);
    i->model  += ((out.eax >> 16) & 0x0F) << 4;
  }

  if (out.ecx & 0x00000001U) i->features |= CpuInfo::Feature_SSE3;
  if (out.ecx & 0x00000008U) i->features |= CpuInfo::Feature_MotitorMWait;
  if (out.ecx & 0x00000200U) i->features |= CpuInfo::Feature_SSSE3;
  if (out.ecx & 0x00002000U) i->features |= CpuInfo::Feature_CMPXCHG16B;
  if (out.ecx & 0x00080000U) i->features |= CpuInfo::Feature_SSE4_1;
  if (out.ecx & 0x00100000U) i->features |= CpuInfo::Feature_SSE4_2;
  if (out.ecx & 0x00800000U) i->features |= CpuInfo::Feature_POPCNT;

  if (out.edx & 0x00000010U) i->features |= CpuInfo::Feature_RDTSC;
  if (out.edx & 0x00000100U) i->features |= CpuInfo::Feature_CMPXCHG8B;
  if (out.edx & 0x00008000U) i->features |= CpuInfo::Feature_CMOV;
  if (out.edx & 0x00800000U) i->features |= CpuInfo::Feature_MMX;
  if (out.edx & 0x01000000U) i->features |= CpuInfo::Feature_FXSR;
  if (out.edx & 0x02000000U) i->features |= CpuInfo::Feature_SSE | CpuInfo::Feature_MMXExt;
  if (out.edx & 0x04000000U) i->features |= CpuInfo::Feature_SSE | CpuInfo::Feature_SSE2;
  if (out.edx & 0x10000000U) i->features |= CpuInfo::Feature_MultiThreading;

  if (strcmp(i->vendor, "AuthenticAMD") == 0 && 
      (out.edx & 0x10000000U))
  {
    // AMD sets Multithreading to ON if it has more cores.
    if (i->numberOfProcessors == 1) i->numberOfProcessors = 2;
  }

  // This comment comes from V8 and I think that its important:
  //
  // Opteron Rev E has a bug in which on very rare occasions a locked
  // instruction doesn't act as a read-acquire barrier if followed by a
  // non-locked read-modify-write instruction.  Rev F has this bug in 
  // pre-release versions, but not in versions released to customers,
  // so we test only for Rev E, which is family 15, model 32..63 inclusive.

  if (strcmp(i->vendor, "AuthenticAMD") == 0 && 
      i->family == 15 && i->model >= 32 && i->model <= 63) 
  {
    i->bugs |= CpuInfo::Bug_AmdLockMB;
  }

  // Calling cpuid with 0x80000000 as the in argument
  // gets the number of valid extended IDs.

  cpuid(0x80000000, &out);

  uint32_t exIds = out.eax;

  for (a = 0x80000001; a < exIds && a <= (0x80000001); a++)
  {
    cpuid(a, &out);

    switch (a)
    {
      case 0x80000001:
        if (out.ecx & 0x00000001U) i->features |= CpuInfo::Feature_LAHF_SAHF;
        if (out.ecx & 0x00000020U) i->features |= CpuInfo::Feature_LZCNT;
        if (out.ecx & 0x00000040U) i->features |= CpuInfo::Feature_SSE4_A;
        if (out.ecx & 0x00000080U) i->features |= CpuInfo::Feature_MSSE;
        if (out.ecx & 0x00000100U) i->features |= CpuInfo::Feature_PREFETCH;
        if (out.ecx & 0x00000800U) i->features |= CpuInfo::Feature_SSE5;

        if (out.edx & 0x00100000U) i->features |= CpuInfo::Feature_ExecuteDisableBit;
        if (out.edx & 0x00200000U) i->features |= CpuInfo::Feature_FFXSR;
        if (out.edx & 0x00400000U) i->features |= CpuInfo::Feature_MMXExt;
        if (out.edx & 0x08000000U) i->features |= CpuInfo::Feature_RDTSCP;
        if (out.edx & 0x20000000U) i->features |= CpuInfo::Feature_64Bit;
        if (out.edx & 0x40000000U) i->features |= CpuInfo::Feature_3dNowExt | CpuInfo::Feature_MMXExt;
        if (out.edx & 0x80000000U) i->features |= CpuInfo::Feature_3dNow;

        break;

      // there are more informations that can be implemented in the future
    }
  }

#endif // FOG_ARCH_X86 || FOG_ARCH_X64
}

} // Fog namespace

// ============================================================================
// [Library Initializers]
// ============================================================================

FOG_INIT_DECLARE err_t fog_cpuinfo_init(void)
{
  static Fog::CpuInfo staticCpuInfo;

  Fog::cpuInfo = &staticCpuInfo;
  Fog::detectCpuInfo(Fog::cpuInfo);

  return Error::Ok;
}

FOG_INIT_DECLARE void fog_cpuinfo_shutdown(void)
{
}