/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

**/

#ifndef __FDT_BUS_DXE_H__
#define __FDT_BUS_DXE_H__

#include <Uefi.h>
#include <Protocol/DtIo.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/HobLib.h>
#include <Library/DevicePathLib.h>
#include <Guid/FdtHob.h>
#include <libfdt.h>

#define DT_DEV_SIGNATURE  SIGNATURE_32 ('d', 't', 'i', 'o')
#define DT_DEV_FROM_THIS(a)  CR(a, DT_DEVICE, DtIo, DT_DEV_SIGNATURE)

extern VOID                          *gDeviceTreeBase;
extern EFI_COMPONENT_NAME_PROTOCOL   gComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL  gComponentName2;
extern EFI_DRIVER_BINDING_PROTOCOL   gDriverBinding;

typedef struct {
  UINTN                      Signature;
  EFI_HANDLE                 Handle;
  INTN                       FdtNode;
  EFI_DT_DEVICE_PATH_NODE    *DevicePath;
  CHAR16                     *ComponentName;
  EFI_DT_IO_PROTOCOL         DtIo;
} DT_DEVICE;

CHAR16 *
FormatComponentName (
  IN  CONST CHAR8  *AsciiStr
  );

EFI_DT_DEVICE_PATH_NODE *
DtPathNodeCreate (
  IN  CONST CHAR8  *Name
  );

BOOLEAN
DtPathMatchesHandle (
  IN  EFI_DEVICE_PATH_PROTOCOL  *Path
  );

EFI_STATUS
DtDeviceCreate (
  IN  INTN         FdtNode,
  IN  CONST CHAR8  *Name,
  IN  DT_DEVICE    *Parent,
  OUT DT_DEVICE    **Out
  );

VOID
DtDeviceCleanup (
  IN  DT_DEVICE  *DtDevice
  );

EFI_STATUS
DtDeviceScan (
  IN  DT_DEVICE                *DtDevice,
  IN  EFI_DT_DEVICE_PATH_NODE  *RemainingDevicePath,
  IN  EFI_HANDLE               DriverBindingHandle
  );

EFI_STATUS
DtDeviceRegister (
  IN  DT_DEVICE   *DtDevice,
  IN  EFI_HANDLE  ControllerHandle,
  IN  EFI_HANDLE  DriverBindingHandle
  );

EFI_STATUS
DtDeviceUnregister (
  IN  DT_DEVICE   *DtDevice,
  IN  EFI_HANDLE  ControllerHandle,
  IN  EFI_HANDLE  DriverBindingHandle
  );

CONST CHAR8 *
FdtGetModel (
  IN INTN  FdtNode
  );

EFI_DT_STATUS
FdtGetStatus (
  IN INTN  FdtNode
  );

EFI_STATUS
FdtGetAddressCells (
  IN  INTN   FdtNode,
  OUT UINT8  *Cells
  );

EFI_STATUS
FdtGetSizeCells (
  IN INTN    FdtNode,
  OUT UINT8  *Cells
  );

#endif /* __FDT_BUS_DXE_H__ */
