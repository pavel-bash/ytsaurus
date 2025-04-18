#pragma once

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

//===- COFFImportFile.h - COFF short import file implementation -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// COFF short import file is a special kind of file which contains
// only symbol names for DLL-exported symbols. This class implements
// exporting of Symbols to create libraries and a SymbolicFile
// interface for the file type.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_OBJECT_COFFIMPORTFILE_H
#define LLVM_OBJECT_COFFIMPORTFILE_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/Object/COFF.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Object/SymbolicFile.h"
#include "llvm/Support/MemoryBufferRef.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {
namespace object {

constexpr std::string_view ImportDescriptorPrefix = "__IMPORT_DESCRIPTOR_";
constexpr std::string_view NullImportDescriptorSymbolName =
    "__NULL_IMPORT_DESCRIPTOR";
constexpr std::string_view NullThunkDataPrefix = "\x7f";
constexpr std::string_view NullThunkDataSuffix = "_NULL_THUNK_DATA";

class COFFImportFile : public SymbolicFile {
private:
  enum SymbolIndex { ImpSymbol, ThunkSymbol, ECAuxSymbol, ECThunkSymbol };

public:
  COFFImportFile(MemoryBufferRef Source)
      : SymbolicFile(ID_COFFImportFile, Source) {}

  static bool classof(Binary const *V) { return V->isCOFFImportFile(); }

  void moveSymbolNext(DataRefImpl &Symb) const override { ++Symb.p; }

  Error printSymbolName(raw_ostream &OS, DataRefImpl Symb) const override {
    switch (Symb.p) {
    case ImpSymbol:
      OS << "__imp_";
      break;
    case ECAuxSymbol:
      OS << "__imp_aux_";
      break;
    }
    const char *Name = Data.getBufferStart() + sizeof(coff_import_header);
    if (Symb.p != ECThunkSymbol && COFF::isArm64EC(getMachine())) {
      if (std::optional<std::string> DemangledName =
              getArm64ECDemangledFunctionName(Name)) {
        OS << StringRef(*DemangledName);
        return Error::success();
      }
    }
    OS << StringRef(Name);
    return Error::success();
  }

  Expected<uint32_t> getSymbolFlags(DataRefImpl Symb) const override {
    return SymbolRef::SF_Global;
  }

  basic_symbol_iterator symbol_begin() const override {
    return BasicSymbolRef(DataRefImpl(), this);
  }

  basic_symbol_iterator symbol_end() const override {
    DataRefImpl Symb;
    if (isData())
      Symb.p = ImpSymbol + 1;
    else if (COFF::isArm64EC(getMachine()))
      Symb.p = ECThunkSymbol + 1;
    else
      Symb.p = ThunkSymbol + 1;
    return BasicSymbolRef(Symb, this);
  }

  bool is64Bit() const override { return false; }

  const coff_import_header *getCOFFImportHeader() const {
    return reinterpret_cast<const object::coff_import_header *>(
        Data.getBufferStart());
  }

  uint16_t getMachine() const { return getCOFFImportHeader()->Machine; }

  StringRef getFileFormatName() const;
  StringRef getExportName() const;

private:
  bool isData() const {
    return getCOFFImportHeader()->getType() == COFF::IMPORT_DATA;
  }
};

struct COFFShortExport {
  /// The name of the export as specified in the .def file or on the command
  /// line, i.e. "foo" in "/EXPORT:foo", and "bar" in "/EXPORT:foo=bar". This
  /// may lack mangling, such as underscore prefixing and stdcall suffixing.
  std::string Name;

  /// The external, exported name. Only non-empty when export renaming is in
  /// effect, i.e. "foo" in "/EXPORT:foo=bar".
  std::string ExtName;

  /// The real, mangled symbol name from the object file. Given
  /// "/export:foo=bar", this could be "_bar@8" if bar is stdcall.
  std::string SymbolName;

  /// Creates a weak alias. This is the name of the weak aliasee. In a .def
  /// file, this is "baz" in "EXPORTS\nfoo = bar == baz".
  std::string AliasTarget;

  uint16_t Ordinal = 0;
  bool Noname = false;
  bool Data = false;
  bool Private = false;
  bool Constant = false;

  friend bool operator==(const COFFShortExport &L, const COFFShortExport &R) {
    return L.Name == R.Name && L.ExtName == R.ExtName &&
            L.Ordinal == R.Ordinal && L.Noname == R.Noname &&
            L.Data == R.Data && L.Private == R.Private;
  }

  friend bool operator!=(const COFFShortExport &L, const COFFShortExport &R) {
    return !(L == R);
  }
};

Error writeImportLibrary(StringRef ImportName, StringRef Path,
                         ArrayRef<COFFShortExport> Exports,
                         COFF::MachineTypes Machine, bool MinGW);

} // namespace object
} // namespace llvm

#endif

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
