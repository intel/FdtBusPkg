/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "FdtBusDxe.h"

/**
  Given an FdtNode, return the device_type property or the empty string.

  @param[in]    TreeBase         Devicetree blob base
  @param[in]    FdtNode          INTN

  @retval CHAR8 *                Device type or empty string.

**/
CONST CHAR8 *
FdtGetDeviceType (
  IN  VOID  *TreeBase,
  IN  INTN  FdtNode
  )
{
  CONST CHAR8  *Buf;

  Buf = fdt_getprop (TreeBase, FdtNode, "device_type", NULL);
  if (Buf == NULL) {
    return "";
  }

  return Buf;
}

/**
  Given an FdtNode, return the device status.

  @param[in]    TreeBase         Devicetree blob base
  @param[in]    FdtNode          INTN

  @retval EFI_DT_STATUS          Enum.

**/
EFI_DT_STATUS
FdtGetStatus (
  IN  VOID  *TreeBase,
  IN  INTN  FdtNode
  )
{
  CONST CHAR8  *Buf;

  Buf = fdt_getprop (TreeBase, FdtNode, "status", NULL);
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

  @param[in]    TreeBase         Devicetree blob base
  @param[in]    FdtNode          INTN
  @param[out]   Cells            UINTN

  @retval EFI_SUCCESS            *Out is populated.
  @retval EFI_NOT_FOUND          No #size-cells property.
  @retval Others                 Errors.

**/
EFI_STATUS
FdtGetSizeCells (
  IN  VOID   *TreeBase,
  IN  INTN   FdtNode,
  OUT UINT8  *Cells
  )
{
  INT32              Len;
  UINT8              Value;
  CONST EFI_DT_CELL  *Buf;

  Buf = fdt_getprop (TreeBase, FdtNode, "#size-cells", &Len);
  if (!Buf) {
    return EFI_NOT_FOUND;
  }

  if (Len != sizeof (EFI_DT_CELL)) {
    return EFI_DEVICE_ERROR;
  }

  Value = fdt32_to_cpu (*Buf);
  if (Value > FDT_MAX_NCELLS) {
    return EFI_DEVICE_ERROR;
  }

  *Cells = Value;
  return EFI_SUCCESS;
}

/**
  Given an FdtNode, return the address cells in *Cells.

  @param[in]    TreeBase         Devicetree blob base
  @param[in]    FdtNode          INTN
  @param[out]   Cells            UINTN

  @retval EFI_SUCCESS            *Out is populated.
  @retval EFI_NOT_FOUND          No #address-cells property.
  @retval Others                 Errors.

**/
EFI_STATUS
FdtGetAddressCells (
  IN  VOID   *TreeBase,
  IN  INTN   FdtNode,
  OUT UINT8  *Cells
  )
{
  INT32              Len;
  UINT8              Value;
  CONST EFI_DT_CELL  *Buf;

  Buf = fdt_getprop (TreeBase, FdtNode, "#address-cells", &Len);
  if (!Buf) {
    return EFI_NOT_FOUND;
  }

  if (Len != sizeof (EFI_DT_CELL)) {
    return EFI_DEVICE_ERROR;
  }

  Value = fdt32_to_cpu (*Buf);
  if (Value > FDT_MAX_NCELLS) {
    return EFI_DEVICE_ERROR;
  }

  *Cells = Value;
  return EFI_SUCCESS;
}

/**
  Given an FdtNode, return whether DMA is coherent.

  @param[in]    TreeBase         Devicetree blob base
  @param[in]    FdtNode          INTN

  @retval TRUE                   DMA is coherent.
  @retval FALSE                  DMA is not coherent.

**/
BOOLEAN
FdtGetDmaCoherency (
  IN  VOID  *TreeBase,
  IN  INTN  FdtNode
  )
{
  return fdt_getprop (TreeBase, FdtNode, "dma-coherent", NULL) != NULL;
}

/**
  Given an FdtNode, return whether this device is critical to platform
  operation (e.g. it must be connected before or during EndOfDxe event).

  @param[in]    TreeBase         Devicetree blob base
  @param[in]    FdtNode          INTN

  @retval TRUE                   Device is critical.
  @retval FALSE                  Device is not critical.

**/
BOOLEAN
FdtIsDeviceCritical (
  IN  VOID  *TreeBase,
  IN  INTN  FdtNode
  )
{
  return fdt_getprop (TreeBase, FdtNode, "uefi,critical", NULL) != NULL;
}

#ifndef MDEPKG_NDEBUG

/**
  Given an FdtNode, return whether this device is a unit test device.

  @param[in]    TreeBase         Devicetree blob base
  @param[in]    FdtNode          INTN

  @retval TRUE                   Device is critical.
  @retval FALSE                  Device is not critical.

**/
BOOLEAN
FdtIsUnitTestDevice (
  IN  VOID  *TreeBase,
  IN  INTN  FdtNode
  )
{
  return fdt_getprop (TreeBase, FdtNode, "uefi,unit-test-device", NULL) != NULL;
}

#endif /* MDEPKG_NDEBUG */
