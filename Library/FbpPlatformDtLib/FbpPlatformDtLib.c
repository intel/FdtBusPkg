/** @file

    Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>
#include <PiDxe.h>
#include <Library/HobLib.h>

/**
  Return the platform-specific device tree pointer, to be used for
  FdtBusDxe initialization.

  @retval NULL                 Not found.
  @retval Other                Pointer to dtb/fdb blob.

**/
VOID *
FbpPlatformGetDt (
  VOID
  )
{
  VOID  *Hob;

  Hob = GetFirstGuidHob (&gFdtHobGuid);
  if ((Hob == NULL) || (GET_GUID_HOB_DATA_SIZE (Hob) != sizeof (UINT64))) {
    return NULL;
  }

  return (VOID *)(UINTN)*(UINT64 *)GET_GUID_HOB_DATA (Hob);
}
