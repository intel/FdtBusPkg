/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
    Copyright (C) 2017 Andrei Evgenievich Warkentin

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <PiDxe.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/FbpAppUtilsLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/EfiShellInterface.h>
#include <Protocol/ShellParameters.h>
#include <Library/FbpUtilsLib.h>
#include <Library/DebugLib.h>
#include <Library/HandleParsingLib.h>
#include <Library/DxeServicesTableLib.h>

EFI_STATUS
GetOpt (
  IN UINTN                Argc,
  IN CHAR16               **Argv,
  IN CHAR16               *OptionsWithArgs,
  IN OUT GET_OPT_CONTEXT  *Context
  )
{
  UINTN  Index;
  UINTN  SkipCount;

  if ((Context->OptIndex >= Argc) ||
      (*Argv[Context->OptIndex] != L'-'))
  {
    return EFI_END_OF_MEDIA;
  }

  if (*(Argv[Context->OptIndex] + 1) == L'\0') {
    /*
     * A lone dash is used to signify end of options list.
     *
     * Like above, but we want to skip the dash.
     */
    Context->OptIndex++;
    return EFI_END_OF_MEDIA;
  }

  SkipCount    = 1;
  Context->Opt = *(Argv[Context->OptIndex] + 1);

  if (OptionsWithArgs != NULL) {
    UINTN  ArgsLen = StrLen (OptionsWithArgs);

    for (Index = 0; Index < ArgsLen; Index++) {
      if (OptionsWithArgs[Index] == Context->Opt) {
        if (*(Argv[Context->OptIndex] + 2) != L'\0') {
          /*
           * Argument to the option may immediately follow
           * the option (not separated by space).
           */
          Context->OptArg = Argv[Context->OptIndex] + 2;
        } else if ((Context->OptIndex + 1 < Argc) &&
                   (*(Argv[Context->OptIndex + 1]) != L'-'))
        {
          /*
           * If argument is separated from option by space, it
           * cannot look like an option (i.e. begin with a dash).
           */
          Context->OptArg = Argv[Context->OptIndex + 1];
          SkipCount++;
        } else {
          /*
           * No argument. Maybe it was optional? Up to the caller
           * to decide.
           */
          Context->OptArg = NULL;
        }

        break;
      }
    }
  }

  Context->OptIndex += SkipCount;
  return EFI_SUCCESS;
}

EFI_STATUS
GetShellArgcArgv (
  IN  EFI_HANDLE  ImageHandle,
  OUT UINTN       *Argcp,
  OUT CHAR16      ***Argvp
  )
{
  EFI_STATUS                     Status;
  EFI_SHELL_PARAMETERS_PROTOCOL  *EfiShellParametersProtocol;
  EFI_SHELL_INTERFACE            *EfiShellInterface;

  Status = gBS->OpenProtocol (
                  ImageHandle,
                  &gEfiShellParametersProtocolGuid,
                  (VOID **)&EfiShellParametersProtocol,
                  ImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (!EFI_ERROR (Status)) {
    //
    // Shell 2.0 interface.
    //
    *Argcp = EfiShellParametersProtocol->Argc;
    *Argvp = EfiShellParametersProtocol->Argv;
    return EFI_SUCCESS;
  }

  Status = gBS->OpenProtocol (
                  ImageHandle,
                  &gEfiShellInterfaceGuid,
                  (VOID **)&EfiShellInterface,
                  ImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (!EFI_ERROR (Status)) {
    //
    // 1.0 interface.
    //
    *Argcp = EfiShellInterface->Argc;
    *Argvp = EfiShellInterface->Argv;
    return EFI_SUCCESS;
  }

  if ((Status != EFI_SUCCESS) || (*Argcp < 1)) {
    Print (
      L"This program requires Microsoft Windows. "
      "Just kidding...only the UEFI Shell!\n"
      );
  }

  return EFI_NOT_FOUND;
}

/**
  Converts the Unicode string to ASCII string to a new allocated buffer.

  @param[in]  String  Unicode string to be converted.

  @return  Buffer points to ASCII string, or NULL if error happens.

**/
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

/**
  Looks up a DT I/O Protocol given a string, which encodes a handle
  or DT alias/path.

  Logs error conditions.

  @param[in]   String     Encodes handle or DT alias/path.
  @param[out]  OutDtIo    Where to return the looked up protocol.
  @param[out]  OutHandle  Where to return the looked up handle.

  @return  EFI_STATUS.

**/
EFI_STATUS
FbpAppLookup (
  IN  CONST CHAR16        *String,
  OUT EFI_DT_IO_PROTOCOL  **OutDtIo,
  OUT EFI_HANDLE          *OutHandle OPTIONAL
  )
{
  EFI_STATUS          Status;
  UINTN               ArgValue;
  EFI_HANDLE          Handle;
  EFI_DT_IO_PROTOCOL  *DtIo;
  EFI_DT_IO_PROTOCOL  *RootDtIo;

  ASSERT (String != NULL);
  ASSERT (OutDtIo != NULL);

  RootDtIo = FbpGetDtRoot ();
  if (RootDtIo == NULL) {
    Print (L"No EFI_DT_IO_PROTOCOL devices present!\n");
    return EFI_NOT_FOUND;
  }

  ArgValue = StrHexToUintn (String);
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

    AsciiArg = UnicodeStrDupToAsciiStr (String);
    if (AsciiArg == NULL) {
      Print (
        L"Couldn't convert '%s' to ASCII\n",
        String
        );
      return EFI_OUT_OF_RESOURCES;
    }

    Status = RootDtIo->Lookup (RootDtIo, AsciiArg, TRUE, &Handle);
    if (Status == EFI_NOT_FOUND) {
      //
      // Allow path/alias lookups of test devicetree nodes (the ones
      // used by the FdtBusDxe unit tests on DEBUG builds).
      //
      RootDtIo = FbpGetDtTestRoot ();
      if (RootDtIo != NULL) {
        Status = RootDtIo->Lookup (RootDtIo, AsciiArg, TRUE, &Handle);
      }
    }

    FreePool (AsciiArg);

    if (EFI_ERROR (Status)) {
      Print (L"Bad parameter '%s': %r\n", String, Status);
      return Status;
    }

    Status = gBS->HandleProtocol (
                    Handle,
                    &gEfiDtIoProtocolGuid,
                    (VOID **)&DtIo
                    );
    ASSERT_EFI_ERROR (Status);
  }

  if (OutHandle != NULL) {
    *OutHandle = Handle;
  }

  *OutDtIo = DtIo;
  return EFI_SUCCESS;
}

VOID
PrintDtU128 (
  IN  EFI_DT_U128  Value,
  IN  BOOLEAN      NewLine
  )
{
  UINTN  Pad;

  Pad = 0;
  if ((Value >> 64) > 0) {
    Print (L"%lx", (UINT64)(Value >> 64));
    Pad = 16;
  }

  Print (
    L"%0*lx%a",
    Pad,
    (UINT64)Value,
    NewLine ? "\r\n" : ""
    );
}

STATIC
VOID
PrintMemTypeAttrs (
  IN  EFI_PHYSICAL_ADDRESS  Address,
  IN  BOOLEAN               NewLine
  )
{
  EFI_STATUS                       Status;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  GcdDescriptor;
  CONST CHAR8                      *Str;
  UINTN                            Index;

  Status = gDS->GetMemorySpaceDescriptor (Address, &GcdDescriptor);
  if (EFI_ERROR (Status)) {
    Print (L"invalid");
    return;
  }

  switch (GcdDescriptor.GcdMemoryType) {
    case EfiGcdMemoryTypeNonExistent:
      Str = "NonExistent";
      break;
    case EfiGcdMemoryTypeReserved:
      Str = "Reserved";
      break;
    case EfiGcdMemoryTypeSystemMemory:
      Str = "SystemMemory";
      break;
    case EfiGcdMemoryTypeMemoryMappedIo:
      Str = "MemoryMappedIo";
      break;
    case EfiGcdMemoryTypePersistent:
      Str = "Persistent";
      break;
    case EfiGcdMemoryTypeMoreReliable:
      Str = "MoreReliable";
      break;
    default:
      Str = "?";
  }

  Print (L"%a ", Str);
  for (Index = 0; GcdDescriptor.Attributes != 0;
       GcdDescriptor.Attributes >>= 1, Index++)
  {
    if ((GcdDescriptor.Attributes & 1) == 0) {
      continue;
    }

    switch (1UL << Index) {
      case EFI_MEMORY_UC:
        Str = "UC";
        break;
      case EFI_MEMORY_WC:
        Str = "WC";
        break;
      case EFI_MEMORY_WT:
        Str = "WT";
        break;
      case EFI_MEMORY_WB:
        Str = "WB";
        break;
      case EFI_MEMORY_UCE:
        Str = "UCE";
        break;
      case EFI_MEMORY_WP:
        Str = "WT";
        break;
      case EFI_MEMORY_RP:
        Str = "WT";
        break;
      case EFI_MEMORY_XP:
        Str = "WT";
        break;
      case EFI_MEMORY_NV:
        Str = "NV";
        break;
      case EFI_MEMORY_MORE_RELIABLE:
        Str = "MR";
        break;
      case EFI_MEMORY_RO:
        Str = "RO";
        break;
      case EFI_MEMORY_SP:
        Str = "SP";
        break;
      case EFI_MEMORY_CPU_CRYPTO:
        Str = "CC";
        break;
      case EFI_MEMORY_RUNTIME:
        Str = "RT";
        break;
      default:
        Str = "??";
    }

    Print (L"%a ", Str);
  }

  if (NewLine) {
    Print (L"\r\n");
  }
}

VOID
PrintDtReg (
  IN  EFI_DT_REG  *Reg,
  IN  BOOLEAN     NewLine
  )
{
  PrintDtU128 (Reg->TranslatedBase, FALSE);
  Print (L"(");
  PrintDtU128 (Reg->Length, FALSE);
  Print (L") ");

  if (Reg->BusDtIo != NULL) {
    Print (L"via %s", Reg->BusDtIo->ComponentName);
  } else {
    PrintMemTypeAttrs (Reg->TranslatedBase, FALSE);
  }

  if (NewLine) {
    Print (L"\r\n");
  }
}

VOID
PrintDtRange (
  IN  EFI_DT_RANGE  *Range,
  IN  BOOLEAN       NewLine
  )
{
  Print (L"0x");
  PrintDtU128 (Range->ChildBase, FALSE);
  Print (L"(");
  PrintDtU128 (Range->Length, FALSE);
  Print (L")->0x");
  PrintDtU128 (Range->TranslatedParentBase, FALSE);

  Print (L" ");
  if (Range->BusDtIo != NULL) {
    Print (L"via %s", Range->BusDtIo->ComponentName);
  } else {
    PrintMemTypeAttrs (Range->TranslatedParentBase, FALSE);
  }

  if (NewLine) {
    Print (L"\r\n");
  }
}

STATIC
BOOLEAN
StrIsHex (
  IN CONST CHAR16  *String
  )
{
  if (*String == L'-') {
    String++;
  }

  //
  // A number that begins with 0x or 0X is hex.
  //
  if (*String == L'0') {
    String++;

    if ((*String == L'x') || (*String == L'X')) {
      String++;

      if (*String != L'\0') {
        return TRUE;
      }
    }
  }

  return FALSE;
}

UINTN
StrHexOrDecToUintn (
  IN CONST CHAR16  *String
  )
{
  if (StrIsHex (String)) {
    return StrHexToUintn (String);
  } else {
    return StrDecimalToUintn (String);
  }
}
