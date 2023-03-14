/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

**/

#include "FdtBusDxe.h"

/**
  Given an ASCII name, format as a Unicode component name.

  E.g. foo -> DT(foo).

  @param[in]    AsciiStr       ASCII string.

  @retval NULL                 Failed to allocate memory.
  @retval Others               Pointer to Unicode string.

**/
CHAR16 *
FormatComponentName (
  IN  CONST CHAR8  *AsciiStr
  )
{
  UINTN   Size;
  CHAR16  *UniStr;

  ASSERT (AsciiStr != NULL);

  Size   = AsciiStrSize (AsciiStr) + 4; /* DT() */
  UniStr = AllocateZeroPool (Size * sizeof (CHAR16));
  if (UniStr == NULL) {
    return NULL;
  }

  AsciiStrToUnicodeStrS ("DT(", UniStr, Size);
  AsciiStrToUnicodeStrS (AsciiStr, UniStr + 3, Size - 3);
  UniStr[Size - 2] = L')';

  return UniStr;
}

/**
  Given an ASCII name, allocate/fill a EFI_DT_DEVICE_PATH_NODE.

  @param[in]    Name           ASCII string.

  @retval NULL                 Failed to allocate memory.
  @retval Others               EFI_DT_DEVICE_PATH_NODE.

**/
EFI_DT_DEVICE_PATH_NODE *
DtDevicePathNodeCreate (
  IN  CONST CHAR8  *Name
  )
{
  UINTN                    Size;
  EFI_STATUS               Status;
  EFI_DT_DEVICE_PATH_NODE  *Node;

  Size = AsciiStrSize (Name);
  Node = AllocateZeroPool (sizeof (EFI_DT_DEVICE_PATH_NODE) + Size);
  if (Node == NULL) {
    return NULL;
  }

  Node->VendorDevicePath.Header.Type      = HARDWARE_DEVICE_PATH;
  Node->VendorDevicePath.Header.SubType   = HW_VENDOR_DP;
  Node->VendorDevicePath.Header.Length[0] =
    (UINT8)(sizeof (EFI_DT_DEVICE_PATH_NODE) + Size);
  Node->VendorDevicePath.Header.Length[1] =
    (UINT8)((sizeof (EFI_DT_DEVICE_PATH_NODE) + Size) >> 8);
  CopyGuid (&Node->VendorDevicePath.Guid, &gEfiDtIoProtocolGuid);

  Status = AsciiStrCpyS (Node->Name, Size, Name);
  if (EFI_ERROR (Status)) {
    FreePool (Node);
    DEBUG ((DEBUG_ERROR, "%a: AsciiStrCpyS: %r\n", __func__, Status));
    return NULL;
  }

  return Node;
}
