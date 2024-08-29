/** @file

    Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __FBP_INTERRUPT_UTILS_LIB_H__
#define __FBP_INTERRUPT_UTILS_LIB_H__

#include <Uefi.h>
#include <Protocol/DtIo.h>

//
// See Power ISA Open PIC Interrupt Controller in the Devicetree Specification.
//
#define EFI_DT_INTERRUPT_EDGE_HIGH   0
#define EFI_DT_INTERRUPT_LEVEL_LOW   1
#define EFI_DT_INTERRUPT_LEVEL_HIGH  2
#define EFI_DT_INTERRUPT_EDGE_LOW    3

EFI_STATUS
FbpInterruptGet (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  UINTN               Index,
  OUT EFI_HANDLE          *InterruptParent,
  OUT EFI_DT_PROPERTY     *Interrupt
  );

#endif /* __FBP_INTERRUPT_UTILS_LIB_H__ */
