/** @file

    Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __FBP_PCI_UTILS_LIB_H__
#define __FBP_PCI_UTILS_LIB_H__

#include <Uefi.h>
#include <Protocol/DtIo.h>

//
// See 2.2.1. Physical Address Formats in
// IEEE Std 1275-1994.
//
#define EFI_DT_PCI_HOST_RANGE_RELOCATABLE   BIT31
#define EFI_DT_PCI_HOST_RANGE_PREFETCHABLE  BIT30
#define EFI_DT_PCI_HOST_RANGE_ALIASED       BIT29
#define EFI_DT_PCI_HOST_RANGE_SS_MASK       (BIT24|BIT25)
#define EFI_DT_PCI_HOST_RANGE_MMIO64        (BIT24|BIT25)
#define EFI_DT_PCI_HOST_RANGE_MMIO32        BIT25
#define EFI_DT_PCI_HOST_RANGE_IO            BIT24

EFI_DT_CELL
FbpPciGetRangeAttribute (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  EFI_DT_BUS_ADDRESS  ChildBase
  );

#endif /* __FBP_PCI_UTILS_LIB_H__ */
