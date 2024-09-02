/** @file

    Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>
    Copyright (C) 2016 Andrei Evgenievich Warkentin

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/FbpAppUtilsLib.h>
#include <Library/DebugLib.h>

STATIC
EFI_STATUS
Usage (
  IN CHAR16  *Name
  )
{
  Print (L"Usage: %s [-i reg index|name] [-n count] [-w access width] controller offset [set value]\n", Name);
  return EFI_INVALID_PARAMETER;
}

EFI_STATUS
EFIAPI
EntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  UINTN                     Argc;
  CHAR16                    **Argv;
  EFI_STATUS                Status;
  GET_OPT_CONTEXT           GetOptContext;
  EFI_DT_IO_PROTOCOL        *DtIo;
  UINTN                     RegIndex;
  CHAR8                     *RegName;
  UINTN                     AccessWidth;
  EFI_DT_IO_PROTOCOL_WIDTH  IoWidth;
  UINTN                     Count;
  EFI_DT_REG                Reg;
  UINTN                     Offset;
  BOOLEAN                   Set;
  UINTN                     SetValue;

  Status = GetShellArgcArgv (ImageHandle, &Argc, &Argv);
  if (EFI_ERROR (Status)) {
    //
    // Already logged error.
    //
    return Status;
  }

  Count       = 1;
  AccessWidth = 1;
  RegIndex    = 0;
  RegName     = NULL;
  Set         = FALSE;
  INIT_GET_OPT_CONTEXT (&GetOptContext);
  while ((Status = GetOpt (
                     Argc,
                     Argv,
                     L"inw",
                     &GetOptContext
                     )) == EFI_SUCCESS)
  {
    switch (GetOptContext.Opt) {
      case L'i':
        if (GetOptContext.OptArg == NULL) {
          return Usage (Argv[0]);
        }

        RegIndex = StrHexOrDecToUintn (GetOptContext.OptArg);
        RegName  = UnicodeStrDupToAsciiStr (GetOptContext.OptArg);
        break;
      case L'n':
        if (GetOptContext.OptArg == NULL) {
          return Usage (Argv[0]);
        }

        Count = StrHexOrDecToUintn (GetOptContext.OptArg);
        break;
      case L'w':
        if (GetOptContext.OptArg == NULL) {
          return Usage (Argv[0]);
        }

        AccessWidth = StrHexOrDecToUintn (GetOptContext.OptArg);
        break;
      default:
        Print (L"Unknown option '%c'\n", GetOptContext.Opt);
        return Usage (Argv[0]);
    }
  }

  if (AccessWidth == 1) {
    IoWidth = EfiDtIoWidthUint8;
  } else if (AccessWidth == 2) {
    IoWidth = EfiDtIoWidthUint16;
  } else if (AccessWidth == 4) {
    IoWidth = EfiDtIoWidthUint32;
  } else if (AccessWidth == 8) {
    IoWidth = EfiDtIoWidthUint64;
  } else {
    Print (L"Bad access width parameter %u\n", AccessWidth);
    return EFI_INVALID_PARAMETER;
  }

  if (Argc - GetOptContext.OptIndex < 2) {
    return Usage (Argv[0]);
  }

  if (Argc - GetOptContext.OptIndex == 3) {
    Set      = TRUE;
    SetValue = StrHexOrDecToUintn (Argv[GetOptContext.OptIndex + 2]);
  }

  Offset = StrHexOrDecToUintn (Argv[GetOptContext.OptIndex + 1]);
  Status = FbpAppLookup (
             Argv[GetOptContext.OptIndex],
             &DtIo,
             NULL
             );
  if (EFI_ERROR (Status)) {
    //
    // Already logged the error in FbpAppLookup.
    //
    return Status;
  }

  Status = EFI_NOT_FOUND;
  if (RegName != NULL) {
    Status = DtIo->GetRegByName (DtIo, RegName, &Reg);
  }

  if (EFI_ERROR (Status)) {
    Status = DtIo->GetReg (DtIo, RegIndex, &Reg);
  }

  if (EFI_ERROR (Status)) {
    if (RegName != NULL) {
      Print (
        L"Cannot get region by name '%a' or index %u: %r\n",
        RegName,
        RegIndex,
        Status
        );
    } else {
      Print (L"Cannot get region by index %u: %r\n", RegIndex, Status);
    }

    return Usage (Argv[0]);
  }

  if (RegName != NULL) {
    FreePool (RegName);
  }

  if (!Set) {
    Print (
      L"Dumping %lu bytes at offset 0x%lx of reg ",
      AccessWidth * Count,
      Offset
      );
    PrintDtReg (&Reg, FALSE);
    Print (L":\n");
  }

  while (Count--) {
    if (Set) {
      Status = DtIo->WriteReg (DtIo, IoWidth, &Reg, Offset, 1, &SetValue);
      if (EFI_ERROR (Status)) {
        Print (L"WriteReg at offset 0x%lx failed: %r\n", Offset, Status);
        break;
      }
    } else {
      UINT64  Value;

      Value  = 0;
      Status = DtIo->ReadReg (DtIo, IoWidth, &Reg, Offset, 1, &Value);
      if (EFI_ERROR (Status)) {
        Print (L"ReadReg at offset 0x%lx failed: %r\n", Offset, Status);
        break;
      }

      Print (L"%08x: %0*lx\n", Offset, AccessWidth * 2, Value);
    }

    Offset += AccessWidth;
  }

  return Status;
}
