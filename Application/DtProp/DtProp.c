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
VOID
DumpHex (
  IN  UINTN       Indent,
  IN  UINTN       Offset,
  IN  UINTN       DataSize,
  IN  CONST VOID  *UserData
  )
{
  CONST UINT8  *Data;
  CHAR8        Val[50];
  CHAR8        Str[20];
  UINT8        TempByte;
  UINTN        Size;
  UINTN        Index;

  STATIC CONST CHAR8  Hex[] = {
    '0', '1', '2', '3',
    '4', '5', '6', '7',
    '8', '9', 'A', 'B',
    'C', 'D', 'E', 'F'
  };

  Data = UserData;
  while (DataSize != 0) {
    Size = 16;
    if (Size > DataSize) {
      Size = DataSize;
    }

    for (Index = 0; Index < Size; Index += 1) {
      TempByte           = Data[Index];
      Val[Index * 3 + 0] = Hex[TempByte >> 4];
      Val[Index * 3 + 1] = Hex[TempByte & 0xF];
      Val[Index * 3 + 2] = (CHAR8)((Index == 7) ? '-' : ' ');
      Str[Index]         = (CHAR8)((TempByte < ' ' || TempByte > '~') ? '.' : TempByte);
    }

    Val[Index * 3] = 0;
    Str[Index]     = 0;
    Print (L"%*a%08X: %-48a *%a*\r\n", Indent, "", Offset, Val, Str);

    Data     += Size;
    Offset   += Size;
    DataSize -= Size;
  }
}

STATIC
EFI_STATUS
Usage (
  IN  CHAR16  *Name
  )
{
  Print (L"Usage: %s handle|handle index|path property [parse string]\n", Name);
  return EFI_INVALID_PARAMETER;
}

EFI_STATUS
EFIAPI
EntryPoint (
  IN  EFI_HANDLE        ImageHandle,
  IN  EFI_SYSTEM_TABLE  *SystemTable
  )
{
  UINTN               Argc;
  CHAR16              **Argv;
  EFI_STATUS          Status;
  GET_OPT_CONTEXT     GetOptContext;
  EFI_DT_IO_PROTOCOL  *DtIo;
  CHAR8               *PropName;
  EFI_DT_PROPERTY     Prop;

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

  if (Argc - GetOptContext.OptIndex < 2) {
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

  PropName = UnicodeStrDupToAsciiStr (Argv[GetOptContext.OptIndex + 1]);
  if (PropName == NULL) {
    Print (
      L"Couldn't convert '%s' to ASCII\n",
      Argv[GetOptContext.OptIndex + 1]
      );
    return EFI_OUT_OF_RESOURCES;
  }

  Status = DtIo->GetProp (DtIo, PropName, &Prop);
  if (EFI_ERROR (Status)) {
    Print (L"Couldn't get property '%a': %r\n", PropName, Status);
    goto out;
  }

  if (Argc - GetOptContext.OptIndex < 3) {
    if ((Prop.End - Prop.Begin) == 0) {
      Print (L"Property '%a' exists but is EMPTY\n");
    } else {
      Print (L"Dumping %u bytes of '%a':\n", Prop.End - Prop.Begin, PropName);
      DumpHex (2, 0, Prop.End - Prop.Begin, Prop.Begin);
    }
  } else {
    UINTN  Index;

    Print (
      L"Parsing '%a' with command string '%s':\n",
      PropName,
      Argv[GetOptContext.OptIndex + 2]
      );
    for (Index = 0; Argv[GetOptContext.OptIndex + 2][Index] != L'\0'; Index++) {
      CHAR16  Command;

      Print (L"  0x%04x | ", Prop.Iter - Prop.Begin);
      Command = Argv[GetOptContext.OptIndex + 2][Index];
      switch (Command) {
        case L'1':
        {
          UINT32  Value;

          Status = DtIo->ParseProp (DtIo, &Prop, EFI_DT_VALUE_U32, 0, &Value);
          if (EFI_ERROR (Status)) {
            Print (
              L"\nError parsing EFI_DT_VALUE_U32 at offset 0x%x: %r\n",
              Prop.Iter - Prop.Begin,
              Status
              );
            goto out;
          }

          Print (L"0x%x\n", Value);
          break;
        }
        case L'2':
        {
          UINT64  Value;

          Status = DtIo->ParseProp (DtIo, &Prop, EFI_DT_VALUE_U64, 0, &Value);
          if (EFI_ERROR (Status)) {
            Print (
              L"\nError parsing EFI_DT_VALUE_U64 at offset 0x%x: %r\n",
              Prop.Iter - Prop.Begin,
              Status
              );
            goto out;
          }

          Print (L"0x%lx\n", Value);
          break;
        }
        case L'4':
        {
          EFI_DT_U128  Value;

          Status = DtIo->ParseProp (DtIo, &Prop, EFI_DT_VALUE_U128, 0, &Value);
          if (EFI_ERROR (Status)) {
            Print (
              L"\nError parsing EFI_DT_VALUE_U128 at offset 0x%x: %r\n",
              Prop.Iter - Prop.Begin,
              Status
              );
            goto out;
          }

          PrintDtU128 (Value, TRUE);
          break;
        }
        case L'b':
        {
          EFI_DT_BUS_ADDRESS  Value;

          Status = DtIo->ParseProp (DtIo, &Prop, EFI_DT_VALUE_BUS_ADDRESS, 0, &Value);
          if (EFI_ERROR (Status)) {
            Print (
              L"\nError parsing EFI_DT_VALUE_BUS_ADDRESS at offset 0x%x: %r\n",
              Prop.Iter - Prop.Begin,
              Status
              );
            goto out;
          }

          PrintDtU128 (Value, TRUE);
          break;
        }
        case L'B':
        {
          EFI_DT_BUS_ADDRESS  Value;

          Status = DtIo->ParseProp (DtIo, &Prop, EFI_DT_VALUE_CHILD_BUS_ADDRESS, 0, &Value);
          if (EFI_ERROR (Status)) {
            Print (
              L"\nError parsing EFI_DT_VALUE_CHILD_BUS_ADDRESS at offset 0x%x: %r\n",
              Prop.Iter - Prop.Begin,
              Status
              );
            goto out;
          }

          PrintDtU128 (Value, TRUE);
          break;
        }
        case L'z':
        {
          EFI_DT_SIZE  Value;

          Status = DtIo->ParseProp (DtIo, &Prop, EFI_DT_VALUE_SIZE, 0, &Value);
          if (EFI_ERROR (Status)) {
            Print (
              L"\nError parsing EFI_DT_VALUE_SIZE at offset 0x%x: %r\n",
              Prop.Iter - Prop.Begin,
              Status
              );
            goto out;
          }

          PrintDtU128 (Value, TRUE);
          break;
        }
        case L'Z':
        {
          EFI_DT_SIZE  Value;

          Status = DtIo->ParseProp (DtIo, &Prop, EFI_DT_VALUE_CHILD_SIZE, 0, &Value);
          if (EFI_ERROR (Status)) {
            Print (
              L"\nError parsing EFI_DT_VALUE_CHILD_SIZE at offset 0x%x: %r\n",
              Prop.Iter - Prop.Begin,
              Status
              );
            goto out;
          }

          PrintDtU128 (Value, TRUE);
          break;
        }
        case L'r':
        {
          EFI_DT_REG  Value;

          Status = DtIo->ParseProp (DtIo, &Prop, EFI_DT_VALUE_REG, 0, &Value);
          if (EFI_ERROR (Status)) {
            Print (
              L"\nError parsing EFI_DT_VALUE_REG at offset 0x%x: %r\n",
              Prop.Iter - Prop.Begin,
              Status
              );
            goto out;
          }

          PrintDtReg (&Value);
          break;
        }
        case L'R':
        {
          EFI_DT_RANGE  Value;

          Status = DtIo->ParseProp (DtIo, &Prop, EFI_DT_VALUE_RANGE, 0, &Value);
          if (EFI_ERROR (Status)) {
            Print (
              L"\nError parsing EFI_DT_VALUE_RANGE at offset 0x%x: %r\n",
              Prop.Iter - Prop.Begin,
              Status
              );
            goto out;
          }

          PrintDtU128 (Value.ChildBase, FALSE);
          Print (L"->");
          PrintDtU128 (Value.ParentBase, FALSE);
          Print (L"(");
          PrintDtU128 (Value.Size, FALSE);
          Print (L")\n");
          break;
        }
        case L's':
        {
          CHAR8  *String;

          Status = DtIo->ParseProp (DtIo, &Prop, EFI_DT_VALUE_STRING, 0, &String);
          if (EFI_ERROR (Status)) {
            Print (
              L"\nError parsing EFI_DT_VALUE_STRING at offset 0x%x: %r\n",
              Prop.Iter - Prop.Begin,
              Status
              );
            goto out;
          }

          Print (L"%a\n", String);
          break;
        }
        case L'd':
        {
          EFI_HANDLE  Handle;

          Status = DtIo->ParseProp (DtIo, &Prop, EFI_DT_VALUE_DEVICE, 0, &Handle);
          if (EFI_ERROR (Status)) {
            Print (
              L"\nError parsing EFI_DT_VALUE_STRING at offset 0x%x: %r\n",
              Prop.Iter - Prop.Begin,
              Status
              );
            goto out;
          }

          Print (L"%lx\n", Handle);
          break;
        }
        default:
          Print (L"Unknown parsing commmand '%c'\n", Command);
          goto out;
      }
    }
  }

out:
  FreePool (PropName);
  return Status;
}
