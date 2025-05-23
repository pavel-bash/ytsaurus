# Generated by devtools/yamaker from nixpkgs 24.05.

LIBRARY()

LICENSE(
    Apache-2.0 AND
    Apache-2.0 WITH LLVM-exception AND
    BSL-1.0
)

LICENSE_TEXTS(.yandex_meta/licenses.list.txt)

VERSION(1.88.0)

ORIGINAL_SOURCE(https://github.com/boostorg/charconv/archive/boost-1.88.0.tar.gz)

PEERDIR(
    contrib/restricted/boost/assert
    contrib/restricted/boost/config
    contrib/restricted/boost/core
)

ADDINCL(
    GLOBAL contrib/restricted/boost/charconv/include
)

NO_COMPILER_WARNINGS()

NO_UTIL()

END()
