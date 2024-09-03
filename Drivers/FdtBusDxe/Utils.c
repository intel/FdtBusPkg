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
         FBP_DT_TEST_ROOT_NAME : FBP_DT_ROOT_NAME;

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

  if (IsDevicePathEnd (RemainingDevicePath)) {
    if (FoundHandle != NULL) {
      *FoundHandle = Handle;
    }

    //
    // Ignore ConnectController failures on the last component.
    //
    Status = EFI_SUCCESS;
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

/**
  Set particular EFI_GCD_MEMORY_TYPE and memory region attributes for a
  physical memory address range.

  @param  Address               Range base.
  @param  Length                Range length.
  @param  Type                  EFI_GCD_MEMORY_TYPE.
  @param  Attributes            Memory region attributes.
  @param  OutType               Where to store last seen EFI_GCD_MEMORY_TYPE.
  @param  OutAttributes         Where to store last seen attributes.
  @param  OnConflictDoNothing   TRUE if a conflicting region/attributes
                                should be left alone. FALSE if it should
                                be corrected.
  @retval EFI_SUCCESS           Success.
  @retval Other                 Error, conflict, etc.

**/
EFI_STATUS
ApplyGcdTypeAndAttrs (
  IN  EFI_PHYSICAL_ADDRESS  Address,
  IN  UINTN                 Length,
  IN  EFI_GCD_MEMORY_TYPE   Type,
  IN  UINT64                Attributes,
  OUT EFI_GCD_MEMORY_TYPE   *OutType OPTIONAL,
  OUT UINT64                *OutAttributes OPTIONAL,
  IN  BOOLEAN               OnConflictDoNothing
  )
{
  EFI_STATUS                       Status;
  EFI_GCD_MEMORY_SPACE_DESCRIPTOR  GcdDescriptor;
  EFI_PHYSICAL_ADDRESS             AlignedAddress;
  UINTN                            AlignedLength;
  EFI_PHYSICAL_ADDRESS             AlignedEnd;
  EFI_PHYSICAL_ADDRESS             Next;

  ASSERT (Length != 0);

  //
  // Attributes work on page-aligned mappings, so make sure
  // to widen any requests that are smaller.
  //
  AlignedAddress = ROUND_DOWN (Address, EFI_PAGE_SIZE);
  AlignedLength  = ROUND_UP (Address + Length, EFI_PAGE_SIZE) -
                   AlignedAddress;
  AlignedEnd = AlignedAddress + AlignedLength;

  DEBUG ((
    DEBUG_VERBOSE,
    "%a: widening 0x%lx 0x%lx -> 0x%lx 0x%lx\n",
    __func__,
    Address,
    Length,
    AlignedAddress,
    AlignedLength
    ));

  for (Next = AlignedAddress; Next < AlignedEnd;) {
    BOOLEAN               HaveConflict;
    EFI_PHYSICAL_ADDRESS  OverlapRegionEnd;

    Status = gDS->GetMemorySpaceDescriptor (
                    Next,
                    &GcdDescriptor
                    );
    if (EFI_ERROR (Status)) {
      //
      // This can happen if AlignedAddress exceeds the Gcd limits as set
      // by the Cpu HOB. You'll get an EFI_NOT_FOUND and it's really
      // a programmer error since the AlignedAddress is clearly invalid.
      //
      DEBUG ((
        DEBUG_ERROR,
        "%a: GetMemorySpaceDescriptor(0x%lx): %r\n",
        __func__,
        Next,
        Status
        ));
      if (Status == EFI_NOT_FOUND) {
        Status = EFI_INVALID_PARAMETER;
      }

      return Status;
    }

    if (GcdDescriptor.BaseAddress + GcdDescriptor.Length > AlignedEnd) {
      OverlapRegionEnd = AlignedEnd;
    } else {
      OverlapRegionEnd = GcdDescriptor.BaseAddress + GcdDescriptor.Length;
    }

    HaveConflict = FALSE;
    if (GcdDescriptor.GcdMemoryType != EfiGcdMemoryTypeNonExistent) {
      if ((GcdDescriptor.GcdMemoryType == Type) &&
          ((GcdDescriptor.Attributes == 0) ||
           (GcdDescriptor.Attributes == Attributes)))
      {
        DEBUG ((
          DEBUG_VERBOSE,
          "%a: consuming compatible existing range 0x%lx-0x%lx\n",
          __func__,
          Next,
          OverlapRegionEnd - 1
          ));
      } else {
        DEBUG ((
          OnConflictDoNothing ? DEBUG_INFO : DEBUG_ERROR,
          "%a: %a incompatible existing range 0x%lx-0x%lx type %u attributes 0x%x\n",
          __func__,
          OnConflictDoNothing ? "saw" : "overriding",
          Next,
          OverlapRegionEnd - 1,
          GcdDescriptor.GcdMemoryType,
          GcdDescriptor.Attributes
          ));
        HaveConflict = TRUE;
      }
    }

    if (HaveConflict && !OnConflictDoNothing) {
      Status = gDS->RemoveMemorySpace (Next, OverlapRegionEnd - Next);
      if (EFI_ERROR (Status)) {
        //
        // This means the region is in use and there are allocations made out of it.
        //
        DEBUG ((
          DEBUG_ERROR,
          "%a: couldn't remove conflicting [0x%lx, 0x%lx): %r\n",
          __func__,
          Next,
          OverlapRegionEnd,
          Status
          ));
        return Status;
      }
    }

    if ((GcdDescriptor.GcdMemoryType == EfiGcdMemoryTypeNonExistent) ||
        (HaveConflict && !OnConflictDoNothing))
    {
      Status = gDS->AddMemorySpace (Type, Next, OverlapRegionEnd - Next, Attributes);
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: failed to add [0x%lx, 0x%lx) to GCD: %r\n",
          __func__,
          Next,
          OverlapRegionEnd,
          Status
          ));
        return Status;
      }
    }

    //
    // Setting attributes is only safe when:
    // - adding new region.
    // - safely resolved conflict (could re-add region)
    // - no conflict, but the current attributes is 0:
    //
    // It looks like on some (RISC-V?) implementations the GCD
    // entries set up via HOBS have attributes 0 with the MMU code
    // setting real page protections (but not updating
    // the GCD attributes). So, BaseRiscVMmuLib as of 02/2024 violates
    // the PI spec, which states:
    //
    // "... the DXE driver that produces the EFI_CPU_ARCH_PROTOCOL must
    // seed the GCD memory space map with the initial state of the
    // attributes for all the memory regions visible to the processor."
    //
    // So if the attributes are 0 we don't know for sure - just assume
    // it wasn't really mapped, which is resilient to this kind of
    // violation.
    //
    if ((GcdDescriptor.GcdMemoryType == EfiGcdMemoryTypeNonExistent) ||
        (!HaveConflict && (GcdDescriptor.Attributes == 0)) ||
        (HaveConflict && !OnConflictDoNothing))
    {
      Status = gDS->SetMemorySpaceAttributes (
                      Next,
                      OverlapRegionEnd - Next,
                      Attributes
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: failed to set attributes for [0x%lx, 0x%lx): %r\n",
          __func__,
          Next,
          OverlapRegionEnd - Next,
          Status
          ));
        return Status;
      }
    }

    Next = OverlapRegionEnd;
  }

  if (OutType != NULL) {
    *OutType = GcdDescriptor.GcdMemoryType;
  }

  if (OutAttributes != NULL) {
    *OutAttributes = GcdDescriptor.Attributes;
  }

  return EFI_SUCCESS;
}

#ifndef MDEPKG_NDEBUG

/**
  Check that a range is present in the memory map, and that it
  is not part of unallocated/free memory or similar.

  @param[in]  Address       Base of range to check.
  @param[in]  Length        Length fo range to check.

  @retval EFI_SUCCESS       Range is not in free memory or similar.
  @retval other             Some error occured.

**/
EFI_STATUS
RangeIsMapped (
  IN  EFI_PHYSICAL_ADDRESS  Address,
  IN  UINT32                Length
  )
{
  UINTN                  Index;
  UINTN                  MapKey;
  UINTN                  MapSize;
  UINTN                  MapPages;
  EFI_STATUS             Status;
  UINTN                  DescriptorSize;
  UINT32                 DescriptorVersion;
  EFI_MEMORY_DESCRIPTOR  *Map;
  EFI_MEMORY_DESCRIPTOR  *Next;
  EFI_PHYSICAL_ADDRESS   AlignedAddress;

  AlignedAddress = ROUND_DOWN (Address, EFI_PAGE_SIZE);
  Length         = ROUND_UP (Address + Length, EFI_PAGE_SIZE) -
                   AlignedAddress;

  Map               = NULL;
  MapKey            = 0;
  MapSize           = 0;
  MapPages          = 0;
  DescriptorSize    = 0;
  DescriptorVersion = 0;
  Status            = gBS->GetMemoryMap (
                             &MapSize,
                             NULL,
                             &MapKey,
                             &DescriptorSize,
                             &DescriptorVersion
                             );

  if (Status != EFI_BUFFER_TOO_SMALL) {
    DEBUG ((DEBUG_ERROR, "%a: GetMemoryMap: %r\n", __func__, Status));
    return Status;
  }

  do {
    MapPages = EFI_SIZE_TO_PAGES (MapSize) + 1;
    Map      = AllocatePages (MapPages);
    if (Map == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      DEBUG ((
        DEBUG_ERROR,
        "%a: AllocatePages %u: %r\n",
        __func__,
        MapPages,
        Status
        ));
      return Status;
    }

    Status = gBS->GetMemoryMap (
                    &MapSize,
                    Map,
                    &MapKey,
                    &DescriptorSize,
                    &DescriptorVersion
                    );
    if (!EFI_ERROR (Status)) {
      break;
    }

    if (EFI_ERROR (Status)) {
      FreePages (Map, MapPages);

      if (Status != EFI_BUFFER_TOO_SMALL) {
        DEBUG ((DEBUG_ERROR, "%a: GetMemoryMap: %r\n", __func__, Status));
        return Status;
      }
    }
  } while (1);

  ASSERT_EFI_ERROR (Status);

  for (Index = 0, Next = Map; Index < (MapSize / DescriptorSize);
       Index++, Next = (VOID *)((UINTN)Next + DescriptorSize))
  {
    EFI_PHYSICAL_ADDRESS  Last;

    Last = Next->PhysicalStart - 1 + (Next->NumberOfPages * EFI_PAGE_SIZE);
    if ((AlignedAddress >= Next->PhysicalStart) &&
        ((AlignedAddress + Length - 1) <= Last))
    {
      break;
    }
  }

  if (Index == (MapSize / DescriptorSize)) {
    Status = EFI_NOT_FOUND;
  } else {
    if ((Next->Type == EfiConventionalMemory) ||
        (Next->Type == EfiUnusableMemory) ||
        (Next->Type == EfiPersistentMemory) ||
        (Next->Type == EfiUnacceptedMemoryType))
    {
      Status = EFI_UNSUPPORTED;
    }
  }

  FreePages (Map, MapPages);
  return Status;
}

#endif /* MDEPKG_NDEBUG */
