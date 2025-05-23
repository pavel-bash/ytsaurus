# See https://github.com/llvm/llvm-project/blob/master/compiler-rt/cmake/builtin-config-ix.cmake

IF (ARCH_I386 OR ARCH_I686)
    SET(CLANG_ARCH i386)
ELSEIF (ARCH_X86_64)
    SET(CLANG_ARCH x86_64)
ELSEIF (ARCH_ARM6)
    SET(CLANG_ARCH armv6)
ELSEIF (ARCH_ARM7 OR ARCH_ARM7_NEON)
    SET(CLANG_ARCH armv7)
ELSEIF (ARCH_ARM64 OR ARCH_AARCH64)
    IF (OS_DARWIN OR OS_IOS)
        SET(CLANG_ARCH arm64)
    ELSE()
        SET(CLANG_ARCH aarch64)
    ENDIF()
ELSEIF (ARCH_PPC64LE)
    SET(CLANG_ARCH powerpc64le)
ELSE()
    MESSAGE(FATAL_ERROR "Unknown architecture")
ENDIF()

IF (OS_DARWIN)
    SET(CLANG_RT_SUFFIX "_osx")
    SET(CLANG_RT_DLLSUFFIX "_osx_dynamic")
ELSE()
    SET(CLANG_RT_SUFFIX "-${CLANG_ARCH}")
    SET(CLANG_RT_DLLSUFFIX "-${CLANG_ARCH}")
ENDIF()
