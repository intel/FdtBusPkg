/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
    Copyright (C) 2016 Andrei Evgenievich Warkentin

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/HandleParsingLib.h>
#include <Library/FbpAppUtilsLib.h>

STATIC
EFI_STATUS
Usage (
  IN CHAR16  *Name
  )
{
  Print (L"Usage: %s handle|handle index|name\n", Name);
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

  return Status;
}
