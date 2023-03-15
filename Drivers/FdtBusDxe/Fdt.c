/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

**/

#include "FdtBusDxe.h"

/**
  Given an FdtNode, return the model name property or the empty string.

  @param[in]    FdtNode          INTN

  @retval CHAR8 *                Model name or empty string.

**/
CONST CHAR8 *
FdtGetModel (
  IN INTN  FdtNode
  )
{
  CONST CHAR8  *Buf;

  Buf = fdt_getprop (gDeviceTreeBase, FdtNode, "model", NULL);
  if (Buf == NULL) {
    return "";
  }

  return Buf;
}

/**
  Given an FdtNode, return the device status.

  @param[in]    FdtNode          INTN

  @retval EFI_DT_STATUS          Enum.

**/
EFI_DT_STATUS
FdtGetStatus (
  IN INTN  FdtNode
  )
{
  CONST CHAR8  *Buf;

  Buf = fdt_getprop (gDeviceTreeBase, FdtNode, "status", NULL);
  if (Buf == NULL) {
    return EFI_DT_STATUS_OKAY;
  }

  if (AsciiStrCmp (Buf, "okay") == 0) {
    return EFI_DT_STATUS_OKAY;
  } else if (AsciiStrCmp (Buf, "disabled") == 0) {
    return EFI_DT_STATUS_DISABLED;
  } else if (AsciiStrCmp (Buf, "reserved") == 0) {
    return EFI_DT_STATUS_RESERVED;
  } else if (AsciiStrCmp (Buf, "fail") == 0) {
    return EFI_DT_STATUS_FAIL;
  } else if (AsciiStrnCmp (Buf, "fail-", 5) == 0) {
    return EFI_DT_STATUS_FAIL_WITH_CONDITION;
  }

  return EFI_DT_STATUS_BROKEN;
}

/**
  Given an FdtNode, return the size cells in *Cells.

  @param[in]    FdtNode          INTN
  @param[out]   Cells            UINTN

  @retval EFI_SUCCESS            *Out is populated.
  @retval Others                 Errors.

**/
EFI_STATUS
FdtGetSizeCells (
  IN  INTN   FdtNode,
  OUT UINT8  *Cells
  )
{
  INTN  Result;

  Result = fdt_size_cells (gDeviceTreeBase, FdtNode);
  if (Result >= 0) {
    *Cells = (UINT8)Result;
    return EFI_SUCCESS;
  }

  return EFI_DEVICE_ERROR;
}

/**
  Given an FdtNode, return the address cells in *Cells.

  @param[in]    FdtNode          INTN
  @param[out]   Cells            UINTN

  @retval EFI_SUCCESS            *Out is populated.
  @retval Others                 Errors.

**/
EFI_STATUS
FdtGetAddressCells (
  IN  INTN   FdtNode,
  OUT UINT8  *Cells
  )
{
  INTN  Result;

  Result = fdt_address_cells (gDeviceTreeBase, FdtNode);
  if (Result >= 0) {
    *Cells = (UINT8)Result;
    return EFI_SUCCESS;
  }

  return EFI_DEVICE_ERROR;
}
