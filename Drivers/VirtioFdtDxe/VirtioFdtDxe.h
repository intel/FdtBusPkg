/** @file
*  Virtio FDT client protocol driver for virtio,mmio DT node
*
*  Copyright (c) 2014 - 2016, Linaro Ltd. All rights reserved.<BR>
*  Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef __VIRTIO_FDT_DXE_H__
#define __VIRTIO_FDT_DXE_H__

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/VirtioMmioDeviceLib.h>

#include <Guid/VirtioMmioTransport.h>

#include <Protocol/DtIo.h>
#include <Protocol/FdtClient.h>

extern EFI_COMPONENT_NAME_PROTOCOL   gComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL  gComponentName2;
extern EFI_DRIVER_BINDING_PROTOCOL   gDriverBinding;

#pragma pack (1)
typedef struct {
  VENDOR_DEVICE_PATH    Vendor;
  UINT64                PhysBase;
} VIRTIO_TRANSPORT_DEVICE_PATH_NODE;
#pragma pack ()

#endif /* __VIRTIO_FDT_DXE_H__ */
