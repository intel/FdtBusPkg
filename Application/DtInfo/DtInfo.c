/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
    Copyright (C) 2016 Andrei Evgenievich Warkentin

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/FbpAppUtilsLib.h>
#include <Library/DebugLib.h>

STATIC
EFI_STATUS
Usage (
  IN CHAR16  *Name
  )
{
  Print (L"Usage: %s handle|handle index|alias|path\n", Name);
  return EFI_INVALID_PARAMETER;
}

STATIC
CONST CHAR8 *
DtStatusString (
  IN EFI_DT_STATUS  DtStatus
  )
{
  switch (DtStatus) {
    case EFI_DT_STATUS_BROKEN:
      return "BROKEN";
    case EFI_DT_STATUS_OKAY:
      return "OKAY";
    case EFI_DT_STATUS_DISABLED:
      return "DISABLED";
    case EFI_DT_STATUS_RESERVED:
      return "RESERVED";
    case EFI_DT_STATUS_FAIL:
      return "FAIL";
    case EFI_DT_STATUS_FAIL_WITH_CONDITION:
      return "FAIL_WITH_CONDITION";
    default:
      break;
  }

  return "<unknown>";
}

STATIC
EFI_STATUS
DtInfo (
  IN EFI_DT_IO_PROTOCOL  *DtIo
  )
{
  UINTN               Index;
  EFI_STATUS          Status;
  CONST CHAR8         *AsciiValue;
  STATIC CONST CHAR8  *ErrorValue = "[ERROR]";
  STATIC CONST CHAR8  *NoneValue  = "[NONE]";

  #define PP(x)        Print (L"%18a: ", (x))
  #define P(x, ty, y)  PP(x); Print (L"'%" #ty "'\n", (y))

  P ("ComponentName", s, DtIo->ComponentName);
  P ("Name", a, DtIo->Name);
  P (
    "DeviceType",
    a,
    AsciiStrLen (DtIo->DeviceType) == 0 ?
    NoneValue : DtIo->DeviceType
    );
  P ("DeviceStatus", a, DtStatusString (DtIo->DeviceStatus));
  P ("AddressCells", u, DtIo->AddressCells);
  P ("SizeCells", u, DtIo->SizeCells);
  P ("ChildAddressCells", u, DtIo->ChildAddressCells);
  P ("ChildSizeCells", u, DtIo->ChildSizeCells);
  P ("IsDmaCoherent", a, DtIo->IsDmaCoherent ? "yes" : "no");
  if (DtIo->ParentDevice == NULL) {
    P ("ParentDevice", a, NoneValue);
  } else {
    P ("ParentDevice", lx, DtIo->ParentDevice);
  }

  Index = 0;
  do {
    Status = DtIo->GetString (
                     DtIo,
                     "compatible",
                     Index,
                     &AsciiValue
                     );
    if (EFI_ERROR (Status)) {
      if (Status == EFI_NOT_FOUND) {
        if (Index != 0) {
          break;
        }

        AsciiValue = NoneValue;
      } else {
        AsciiValue = ErrorValue;
      }
    }

    P ("Compatible", a, AsciiValue);

    Index++;
  } while (!EFI_ERROR (Status));

  Index = 0;
  do {
    CONST CHAR8  *Name;
    EFI_DT_REG   Reg;

    Name = NULL;
    DtIo->GetString (DtIo, "reg-names", Index, &Name);

    Status = DtIo->GetReg (DtIo, Index, &Reg);
    if (EFI_ERROR (Status)) {
      if (Status == EFI_NOT_FOUND) {
        if (Index != 0) {
          break;
        }

        AsciiValue = NoneValue;
      } else {
        AsciiValue = ErrorValue;
      }

      P ("Reg", a, AsciiValue);
    } else {
      PP ("Reg");
      if (Name != NULL) {
        Print (L"%a ", Name);
      } else {
        Print (L"#%u ", Index);
      }

      PrintDtReg (&Reg);
    }

    Index++;
  } while (!EFI_ERROR (Status));

  #undef P

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
EntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  UINTN               Argc;
  CHAR16              **Argv;
  EFI_STATUS          Status;
  GET_OPT_CONTEXT     GetOptContext;
  EFI_DT_IO_PROTOCOL  *DtIo;

  Status = GetShellArgcArgv (ImageHandle, &Argc, &Argv);
  if (EFI_ERROR (Status)) {
    //
    // Already logged error.
    //
    return Status;
  }

  INIT_GET_OPT_CONTEXT (&GetOptContext);
  while ((Status = GetOpt (
                     Argc,
                     Argv,
                     L"",
                     &GetOptContext
                     )) == EFI_SUCCESS)
  {
    switch (GetOptContext.Opt) {
      default:
        Print (L"Unknown option '%c'\n", GetOptContext.Opt);
        return Usage (Argv[0]);
    }
  }

  if (Argc - GetOptContext.OptIndex < 1) {
    return Usage (Argv[0]);
  }

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

  Status = DtInfo (DtIo);
  if (EFI_ERROR (Status)) {
    Print (
      L"Can't dump info on '%s': %r\n",
      Argv[GetOptContext.OptIndex],
      Status
      );
  }

  return Status;
}
