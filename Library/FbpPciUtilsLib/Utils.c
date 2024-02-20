/** @file

    Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/FbpPciUtilsLib.h>

/**
  Get the range attribute portion of the child base address.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  ChildBase             The ChildBase portion of 'ranges' element.

  @return EFI_DT_CELL           phys.hi from 2.2.1 IEEE Std 1275-1994.
**/
EFI_DT_CELL
FbpPciGetRangeAttribute (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  EFI_DT_BUS_ADDRESS  ChildBase
  )
{
  if (This->ChildAddressCells < 2) {
    return 0;
  }

  return (EFI_DT_CELL)(ChildBase >> ((This->ChildAddressCells - 1) * sizeof (EFI_DT_CELL) * 8));
}
