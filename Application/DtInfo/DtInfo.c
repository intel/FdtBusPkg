/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
    Copyright (C) 2016 Andrei Evgenievich Warkentin

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Protocol/DtIo.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/HandleParsingLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/FbpAppUtilsLib.h>
#include <Library/DebugLib.h>

/**
  Converts the Unicode string to ASCII string to a new allocated buffer.

  @param[in]       String       Unicode string to be converted.

  @return     Buffer points to ASCII string, or NULL if error happens.

**/
STATIC
CHAR8 *
UnicodeStrDupToAsciiStr (
  CONST CHAR16  *String
  )
{
  CHAR8       *AsciiStr;
  UINTN       BufLen;
  EFI_STATUS  Status;

  BufLen   = StrLen (String) + 1;
  AsciiStr = AllocatePool (BufLen);
  if (AsciiStr == NULL) {
    return NULL;
  }

  Status = UnicodeStrToAsciiStrS (String, AsciiStr, BufLen);
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  return AsciiStr;
}

STATIC
EFI_STATUS
Usage (
  IN CHAR16  *Name
  )
{
  Print (L"Usage: %s handle|handle index|path\n", Name);
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
  UINTN        Index;
  EFI_STATUS   Status;
  CONST CHAR8  *AsciiValue;

  #define P(x, y, z)  Print (L"%18a: '%" #y "'\n", x, (z))

  P ("ComponentName", s, DtIo->ComponentName);
  P ("Name", a, DtIo->Name);
  P ("DeviceType", a, DtIo->Name);
  P ("DeviceStatus", a, DtStatusString (DtIo->DeviceStatus));
  P ("AddressCells", u, DtIo->AddressCells);
  P ("SizeCells", u, DtIo->SizeCells);
  P ("ChildAddressCells", u, DtIo->ChildAddressCells);
  P ("ChildSizeCells", u, DtIo->ChildSizeCells);
  P ("IsDmaCoherent", a, DtIo->IsDmaCoherent ? "yes" : "no");
  if (DtIo->ParentDevice == NULL) {
    P ("ParentDevice", a, "none");
  } else {
    P ("ParentDevice", lx, DtIo->ParentDevice);
  }

  Index = 0;
  do {
    Status = DtIo->GetString (
                     DtIo,
                     "compatible",
                     Index++,
                     &AsciiValue
                     );
    if ((Status != EFI_SUCCESS) && (Status != EFI_NOT_FOUND)) {
      Print (L"Couldn't get 'compatible'\n");
      return Status;
    }

    if (!EFI_ERROR (Status)) {
      P ("Compatible", a, AsciiValue);
    }
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
  UINTN               ArgValue;
  EFI_HANDLE          Handle;
  EFI_DT_IO_PROTOCOL  *DtIo;
  EFI_DT_IO_PROTOCOL  *AnyDtIo;

  Status = GetShellArgcArgv (ImageHandle, &Argc, &Argv);
  if ((Status != EFI_SUCCESS) || (Argc < 1)) {
    Print (
      L"This program requires Microsoft Windows.\n"
      "Just kidding...only the UEFI Shell!\n"
      );
    return EFI_ABORTED;
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

  Status = gBS->LocateProtocol (
                  &gEfiDtIoProtocolGuid,
                  NULL,
                  (VOID **)&AnyDtIo
                  );
  ASSERT (Status != EFI_INVALID_PARAMETER);
  if (EFI_ERROR (Status)) {
    Print (L"No EFI_DT_IO_PROTOCOL devices present!\n");
    return Status;
  }

  ArgValue = StrHexToUintn (Argv[GetOptContext.OptIndex]);
  Handle   = ConvertHandleIndexToHandle (ArgValue);
  if (Handle == NULL) {
    Handle = (EFI_HANDLE)ArgValue;
  }

  Status = gBS->HandleProtocol (
                  Handle,
                  &gEfiDtIoProtocolGuid,
                  (VOID **)&DtIo
                  );
  if (EFI_ERROR (Status)) {
    CHAR8  *AsciiArg;

    AsciiArg = UnicodeStrDupToAsciiStr (Argv[GetOptContext.OptIndex]);
    if (AsciiArg == NULL) {
      Print (
        L"Couldn't convert '%s' to ASCII\n",
        Argv[GetOptContext.OptIndex]
        );
      return EFI_OUT_OF_RESOURCES;
    }

    Status = AnyDtIo->Lookup (AnyDtIo, "/", FALSE, &Handle);
    ASSERT_EFI_ERROR (Status);

    Status = gBS->HandleProtocol (
                    Handle,
                    &gEfiDtIoProtocolGuid,
                    (VOID **)&DtIo
                    );
    ASSERT_EFI_ERROR (Status);

    Status = DtIo->Lookup (DtIo, AsciiArg, FALSE, &Handle);
    FreePool (AsciiArg);

    if (EFI_ERROR (Status)) {
      Print (L"Bad parameter '%s'\n", Argv[GetOptContext.OptIndex]);
      return Status;
    }

    Status = gBS->HandleProtocol (
                    Handle,
                    &gEfiDtIoProtocolGuid,
                    (VOID **)&DtIo
                    );
    ASSERT_EFI_ERROR (Status);
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