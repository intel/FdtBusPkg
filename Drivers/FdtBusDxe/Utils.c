/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "FdtBusDxe.h"

/**
  Given a an EFI_HANDLE, return if the handle has a driver started
  on it.

  @param[in]    Handle         EFI_HANDLE.

  @retval TRUE                 Has driver bound/started.
  @retval FALSE                Does not have a driver bound/started.

**/
BOOLEAN
HandleHasBoundDriver (
  IN  EFI_HANDLE  Handle
  )
{
  EFI_STATUS                           Status;
  UINTN                                EntryCount;
  UINTN                                Index;
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY  *OpenInfoBuffer;

  //
  // Detect if the child handle has a driver bound to it, by
  // retrieving the list of agents that are consuming
  // gEfiDtIoProtocolGuid on the handle.
  //
  Status = gBS->OpenProtocolInformation (
                  Handle,
                  &gEfiDtIoProtocolGuid,
                  &OpenInfoBuffer,
                  &EntryCount
                  );
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  for (Index = 0; Index < EntryCount; Index++) {
    if ((OpenInfoBuffer[Index].Attributes & EFI_OPEN_PROTOCOL_BY_DRIVER) != 0) {
      break;
    }
  }

  FreePool (OpenInfoBuffer);
  if (Index != EntryCount) {
    //
    // The child handle was opened by a bound driver.
    //
    return TRUE;
  }

  return FALSE;
}

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
DtPathNodeCreate (
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

/**
  See if a handle with exactly matching device path already exists.

  @param[in]    Path           Device Path.

  @retval TRUE                 Exists.
  @retval FALSE                Does not exist or error.

**/
BOOLEAN
DtPathMatchesHandle (
  IN  EFI_DEVICE_PATH_PROTOCOL  *Path
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  Handle;

  Status = gBS->LocateDevicePath (
                  &gEfiDtIoProtocolGuid,
                  &Path,
                  &Handle
                  );
  ASSERT (Status != EFI_INVALID_PARAMETER);
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  if (IsDevicePathEnd ((VOID *)Path)) {
    return TRUE;
  }

  return FALSE;
}
