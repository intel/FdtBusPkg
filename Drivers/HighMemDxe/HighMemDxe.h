/** @file
    High memory node enumeration DXE driver for ARM and RISC-V.

    Copyright (c) 2015-2016, Linaro Ltd. All rights reserved.
    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __HIGH_MEM_DXE_H__
#define __HIGH_MEM_DXE_H__

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/DxeServicesTableLib.h>

#include <Protocol/Cpu.h>
#include <Protocol/DtIo.h>

extern EFI_COMPONENT_NAME_PROTOCOL   gComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL  gComponentName2;
extern EFI_DRIVER_BINDING_PROTOCOL   gDriverBinding;

EFI_STATUS
DeviceIsSupported (
  IN  EFI_DT_IO_PROTOCOL  *DtIo
  );

EFI_STATUS
ProcessMemoryRanges (
  IN  EFI_DT_IO_PROTOCOL  *DtIo
  );

#endif /* __HIGH_MEM_DXE_H__ */
