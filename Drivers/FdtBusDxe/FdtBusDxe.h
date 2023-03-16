/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

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

BOOLEAN
FdtGetDmaCoherency (
  IN INTN  FdtNode
  );

EFI_STATUS
EFIAPI
DtIoLookup (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *PathOrAlias,
  OUT EFI_DT_IO_PROTOCOL  **Device
  );

EFI_STATUS
EFIAPI
DtIoGetProp (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *Name,
  OUT EFI_DT_PROPERTY     *Property
  );

EFI_STATUS
EFIAPI
DtIoScanChildren (
  IN  EFI_DT_IO_PROTOCOL        *This,
  IN  EFI_DEVICE_PATH_PROTOCOL  *RemainingDevicePath OPTIONAL
  );

EFI_STATUS
EFIAPI
DtIoRemoveChildren (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  UINTN               NumberOfChildren,
  IN  EFI_HANDLE          *ChildHandleBuffer OPTIONAL
  );

EFI_STATUS
EFIAPI
DtIoGetReg (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  UINTN               Index,
  OUT EFI_DT_REG          *Reg
  );

EFI_STATUS
EFIAPI
DtIoIsCompatible (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *CompatibleString
  );

EFI_STATUS
EFIAPI
DtIoParseProp (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  EFI_DT_PROPERTY     *Prop,
  IN  EFI_DT_VALUE_TYPE   Type,
  IN  UINTN               Index,
  OUT VOID                *Buffer
  );

EFI_STATUS
EFIAPI
DtIoPollReg (
  IN  EFI_DT_IO_PROTOCOL        *This,
  IN  EFI_DT_IO_PROTOCOL_WIDTH  Width,
  IN  EFI_DT_REG                *Reg,
  IN  UINT64                    Offset,
  IN  UINT64                    Mask,
  IN  UINT64                    Value,
  IN  UINT64                    Delay,
  OUT UINT64                    *Result
  );

EFI_STATUS
EFIAPI
DtIoWriteReg (
  IN     EFI_DT_IO_PROTOCOL        *This,
  IN     EFI_DT_IO_PROTOCOL_WIDTH  Width,
  IN     EFI_DT_REG                *Reg,
  IN     UINT64                    Offset,
  IN     UINTN                     Count,
  IN OUT VOID                      *Buffer
  );

EFI_STATUS
EFIAPI
DtIoReadReg (
  IN     EFI_DT_IO_PROTOCOL        *This,
  IN     EFI_DT_IO_PROTOCOL_WIDTH  Width,
  IN     EFI_DT_REG                *Reg,
  IN     UINT64                    Offset,
  IN     UINTN                     Count,
  IN OUT VOID                      *Buffer
  );

EFI_STATUS
EFIAPI
DtIoCopyReg (
  IN  EFI_DT_IO_PROTOCOL        *This,
  IN  EFI_DT_IO_PROTOCOL_WIDTH  Width,
  IN  EFI_DT_REG                *DestReg,
  IN  UINT64                    DestOffset,
  IN  EFI_DT_REG                *SrcReg,
  IN  UINT64                    SrcOffset,
  IN  UINTN                     Count
  );

EFI_STATUS
EFIAPI
DtIoMap (
  IN      EFI_DT_IO_PROTOCOL                *This,
  IN      EFI_DT_IO_PROTOCOL_DMA_OPERATION  Operation,
  IN      VOID                              *HostAddress,
  IN  OUT UINTN                             *NumberOfBytes,
  OUT     EFI_PHYSICAL_ADDRESS              *DeviceAddress,
  OUT     VOID                              **Mapping
  );

EFI_STATUS
EFIAPI
DtIoUnmap (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  VOID                *Mapping
  );

EFI_STATUS
EFIAPI
DtIoAllocateBuffer (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  EFI_MEMORY_TYPE     MemoryType,
  IN  UINTN               Pages,
  OUT VOID                **HostAddress
  );

EFI_STATUS
EFIAPI
DtIoFreeBuffer (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  UINTN               Pages,
  IN  VOID                *HostAddress
  );

#endif /* __FDT_BUS_DXE_H__ */
