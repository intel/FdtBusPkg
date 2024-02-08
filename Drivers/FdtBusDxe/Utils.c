/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "FdtBusDxe.h"

/**
  Given DeviceFlags, return the right Devicetree base.

  In practice, on non-debug builds, this always returns gDeviceTreeBase,
  because DT_DEVICE_TEST is defined as 0.

  @param[in]    DeviceFlags    DT_DEVICE DeviceFlags.

  @retval VOID *               Fdt base.

**/
VOID *
GetTreeBaseFromDeviceFlags (
  IN UINTN  DeviceFlags
  )
{
  VOID  *TreeBase;

  TreeBase = (DeviceFlags & DT_DEVICE_TEST) != 0 ?
             gTestTreeBase : gDeviceTreeBase;

  ASSERT (TreeBase != NULL);
  return TreeBase;
}

/**
  Given DeviceFlags, return the DT root node name.

  @param[in]    DeviceFlags    DT_DEVICE DeviceFlags.

  @retval CHAR8 *              Name.

**/
CONST CHAR8 *
GetDtRootNameFromDeviceFlags (
  IN UINTN  DeviceFlags
  )
{
  CONST CHAR8  *Name;

  Name = (DeviceFlags & DT_DEVICE_TEST) != 0 ?
         "DtTestRoot" : "DtRoot";

  ASSERT (Name != NULL);
  return Name;
}

/**
  Given DeviceFlags, return the matching root DT_DEVICE.

  @param[in]    DeviceFlags    DT_DEVICE DeviceFlags.

  @retval DT_DEVICE *          Root DT_DEVICE.

**/
CONST DT_DEVICE *
GetDtRootFromDeviceFlags (
  IN UINTN  DeviceFlags
  )
{
  CONST DT_DEVICE  *Device;

  Device = (DeviceFlags & DT_DEVICE_TEST) != 0 ?
           gTestRootDtDevice : gRootDtDevice;

  ASSERT (Device != NULL);
  return Device;
}

/**
  Given a EFI_HANDLE, return if the handle has a driver started
  on it, which means the handle was opened on behalf of itself
  BY_DRIVER.

  @param[in]    Handle                EFI_HANDLE.
  @param[in]    ExtraAttributeChecks  Extra attributes to validate against
                                      the Attribute field in a
                                      EFI_OPEN_PROTOCOL_INFORMATION_ENTRY.
  @oaram[out]   MatchingEntry         EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *

  @retval TRUE                        Has driver bound/started.
  @retval FALSE                       Does not have a driver bound/started.

**/
BOOLEAN
HandleHasBoundDriver (
  IN  EFI_HANDLE                           Handle,
  IN  UINT32                               ExtraAttributeChecks,
  OUT EFI_OPEN_PROTOCOL_INFORMATION_ENTRY  *MatchingEntry OPTIONAL
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
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY  *Entry = &OpenInfoBuffer[Index];

    if (((Entry->Attributes & (EFI_OPEN_PROTOCOL_BY_DRIVER | ExtraAttributeChecks)) ==
         (EFI_OPEN_PROTOCOL_BY_DRIVER | ExtraAttributeChecks)) &&
        (Entry->ControllerHandle == Handle))
    {
      if (MatchingEntry != NULL) {
        *MatchingEntry = *Entry;
      }

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
  @param[in]    Connect        TRUE if connect should be called on
                               missing components.
  @param[out]   FoundHandle    Optional pointer for storing found handle.

  @retval EFI_SUCCESS          Found.
  @retval EFI_NOT_FOUND        Not found.
  @retval Other.               EFI_STATUS.
**/
EFI_STATUS
DtPathToHandle (
  IN  EFI_DEVICE_PATH_PROTOCOL  *Path,
  IN  BOOLEAN                   Connect,
  OUT EFI_HANDLE                *FoundHandle OPTIONAL
  )
{
  EFI_STATUS                Status;
  EFI_HANDLE                Handle;
  EFI_HANDLE                PreviousHandle;
  EFI_DEVICE_PATH_PROTOCOL  *RemainingDevicePath;

  ASSERT (Path != NULL);

  PreviousHandle = NULL;
  do {
    RemainingDevicePath = Path;
    Status              = gBS->LocateDevicePath (&gEfiDtIoProtocolGuid, &RemainingDevicePath, &Handle);
    ASSERT (Status != EFI_INVALID_PARAMETER);

    if (EFI_ERROR (Status)) {
      break;
    }

    if (!Connect && !IsDevicePathEnd (RemainingDevicePath)) {
      Status = EFI_NOT_FOUND;
      break;
    }

    if (Connect) {
      if (PreviousHandle == Handle) {
        //
        // Second attempt after a connect.
        //
        Status = EFI_NOT_FOUND;
        break;
      }

      PreviousHandle = Handle;
      Status         = gBS->ConnectController (Handle, NULL, RemainingDevicePath, FALSE);
    }
  } while (!EFI_ERROR (Status) && !IsDevicePathEnd (RemainingDevicePath));

  if (!EFI_ERROR (Status)) {
    ASSERT (IsDevicePathEnd (RemainingDevicePath));
    if (FoundHandle != NULL) {
      *FoundHandle = Handle;
    }
  }

  return Status;
}

/**
  Return the result of (Multiplicand * Multiplier / Divisor).

  @param Multiplicand A 64-bit unsigned value.
  @param Multiplier   A 64-bit unsigned value.
  @param Divisor      A 32-bit unsigned value.
  @param Remainder    A pointer to a 32-bit unsigned value. This parameter is
                      optional and may be NULL.

  @return Multiplicand * Multiplier / Divisor.
**/
UINT64
MultThenDivU64x64x32 (
  IN  UINT64  Multiplicand,
  IN  UINT64  Multiplier,
  IN  UINT32  Divisor,
  OUT UINT32  *Remainder OPTIONAL
  )
{
  UINT64  Uint64;
  UINT32  LocalRemainder;
  UINT32  Uint32;

  if (Multiplicand > DivU64x64Remainder (MAX_UINT64, Multiplier, NULL)) {
    //
    // Make sure Multiplicand is the bigger one.
    //
    if (Multiplicand < Multiplier) {
      Uint64       = Multiplicand;
      Multiplicand = Multiplier;
      Multiplier   = Uint64;
    }

    //
    // Because Multiplicand * Multiplier overflows,
    //   Multiplicand * Multiplier / Divisor
    // = (2 * Multiplicand' + 1) * Multiplier / Divisor
    // = 2 * (Multiplicand' * Multiplier / Divisor) + Multiplier / Divisor
    //
    Uint64 = MultThenDivU64x64x32 (RShiftU64 (Multiplicand, 1), Multiplier, Divisor, &LocalRemainder);
    Uint64 = LShiftU64 (Uint64, 1);
    Uint32 = 0;
    if ((Multiplicand & 0x1) == 1) {
      Uint64 += DivU64x32Remainder (Multiplier, Divisor, &Uint32);
    }

    return Uint64 + DivU64x32Remainder (Uint32 + LShiftU64 (LocalRemainder, 1), Divisor, Remainder);
  } else {
    return DivU64x32Remainder (MultU64x64 (Multiplicand, Multiplier), Divisor, Remainder);
  }
}

/**
  Return the elapsed tick count from CurrentTick.

  @param  CurrentTick  On input, the previous tick count.
                       On output, the current tick count.
  @param  StartTick    The value the performance counter starts with when it
                       rolls over.
  @param  EndTick      The value that the performance counter ends with before
                       it rolls over.

  @return  The elapsed tick count from CurrentTick.
**/
UINT64
GetElapsedTick (
  IN  UINT64  *CurrentTick,
  IN  UINT64  StartTick,
  IN  UINT64  EndTick
  )
{
  UINT64  PreviousTick;

  PreviousTick = *CurrentTick;
  *CurrentTick = GetPerformanceCounter ();
  if (StartTick < EndTick) {
    return *CurrentTick - PreviousTick;
  } else {
    return PreviousTick - *CurrentTick;
  }
}

/**
  Return the pointer to the end of a string, which is the byte following
  th NUL terminator, or NULL if the NUL terminator is not found.

  @param   Start  Beginning of buffer to consider.
  @param   End    End of buffer (i.e. pointer to first byte beyond).

  @return  Pointer between [Start, End) or NULL.
**/
CONST CHAR8 *
AsciiStrFindEnd (
  IN  CONST CHAR8  *Start,
  IN  CONST CHAR8  *End
  )
{
  CONST CHAR8  *Iter;

  for (Iter = Start; Iter < End; Iter++) {
    if (*Iter == '\0') {
      return Iter + 1;
    }
  }

  return NULL;
}

/**
  Locate the first occurance of a character in a string.

  @param  Str Pointer to NULL terminated ASCII string.
  @param  Chr Character to locate.

  @return  NULL or first occurance of Chr in Str.
**/
CHAR8 *
AsciiStrChr (
  IN  CHAR8  *Str,
  IN  CHAR8  Chr
  )
{
  if (Str == NULL) {
    return Str;
  }

  while (*Str != '\0' && *Str != Chr) {
    ++Str;
  }

  return (*Str == Chr) ? Str : NULL;
}
