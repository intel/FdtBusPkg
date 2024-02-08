/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __FDT_BUS_DXE_H__
#define __FDT_BUS_DXE_H__

#include <Uefi.h>
#include <Protocol/CpuIo2.h>
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
#include <Library/TimerLib.h>

#define DT_DEV_SIGNATURE  SIGNATURE_32 ('d', 't', 'i', 'o')
#define DT_DEV_FROM_THIS(a)  CR(a, DT_DEVICE, DtIo, DT_DEV_SIGNATURE)
#define DT_DEV_FROM_LINK(a)  CR(a, DT_DEVICE, Link, DT_DEV_SIGNATURE)

typedef struct _DT_DEVICE DT_DEVICE;

extern EFI_CPU_IO2_PROTOCOL  *gCpuIo2;
extern VOID                  *gDeviceTreeBase;
extern DT_DEVICE             *gRootDtDevice;
extern DT_DEVICE             *gTestRootDtDevice;
#ifndef MDEPKG_NDEBUG
extern VOID  *gTestTreeBase;
#else
#define gTestTreeBase  NULL
#endif /* MDEPKG_NDEBUG */
extern EFI_COMPONENT_NAME_PROTOCOL   gComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL  gComponentName2;
extern EFI_DRIVER_BINDING_PROTOCOL   gDriverBinding;
extern LIST_ENTRY                    gCriticalDevices;

#define DT_DEVICE_CRITICAL           (1UL << 0)
#define DT_DEVICE_HAS_SIZE_CELLS     (1UL << 1)
#define DT_DEVICE_HAS_ADDRESS_CELLS  (1UL << 2)
#ifndef MDEPKG_NDEBUG
#define DT_DEVICE_TEST      (1UL << 3)
#define DT_DEVICE_TEST_RAN  (1UL << 4)
#else
#define DT_DEVICE_TEST  0
#endif /* MDEPKG_NDEBUG */
#define DT_DEVICE_INHERITED  DT_DEVICE_TEST

struct _DT_DEVICE {
  UINTN                      Signature;
  EFI_HANDLE                 Handle;
  INTN                       FdtNode;
  EFI_DT_DEVICE_PATH_NODE    *DevicePath;
  struct _DT_DEVICE          *Parent;
  EFI_DT_IO_PROTOCOL         DtIo;
  UINTN                      Flags;
  //
  // To insert into gCriticalDevices when DT_DEVICE_CRITICAL
  // is set.
  //
  LIST_ENTRY                 Link;
  //
  // Set via DtIoSetCallbacks.
  //
  EFI_DT_IO_PROTOCOL_CB      *Callbacks;
};

VOID *
GetTreeBaseFromDeviceFlags (
  IN UINTN  DeviceFlags
  );

CONST CHAR8 *
GetDtRootNameFromDeviceFlags (
  IN UINTN  DeviceFlags
  );

CONST DT_DEVICE *
GetDtRootFromDeviceFlags (
  IN UINTN  DeviceFlags
  );

BOOLEAN
HandleHasBoundDriver (
  IN  EFI_HANDLE                           Handle,
  IN  UINT32                               ExtraAttributeChecks,
  OUT EFI_OPEN_PROTOCOL_INFORMATION_ENTRY  *MatchingEntry OPTIONAL
  );

CHAR16 *
FormatComponentName (
  IN  CONST CHAR8  *AsciiStr
  );

EFI_DT_DEVICE_PATH_NODE *
DtPathNodeCreate (
  IN  CONST CHAR8  *Name
  );

EFI_STATUS
DtPathToHandle (
  IN  EFI_DEVICE_PATH_PROTOCOL  *Path,
  IN  BOOLEAN                   Connect,
  OUT EFI_HANDLE                *FoundHandle OPTIONAL
  );

EFI_STATUS
DtDeviceCreate (
  IN  INTN         FdtNode,
  IN  CONST CHAR8  *Name,
  IN  DT_DEVICE    *Parent,
  IN  UINTN        DeviceFlags,
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
  IN  EFI_HANDLE  ControllerHandle OPTIONAL,
  IN  EFI_HANDLE  DriverBindingHandle OPTIONAL
  );

EFI_STATUS
DtDeviceUnregister (
  IN  DT_DEVICE   *DtDevice,
  IN  EFI_HANDLE  ControllerHandle OPTIONAL,
  IN  EFI_HANDLE  DriverBindingHandle OPTIONAL
  );

EFI_STATUS
DtDeviceRemove (
  IN EFI_HANDLE  DeviceHandle,
  IN EFI_HANDLE  ParentHandle,
  IN EFI_HANDLE  DriverBindingHandle
  );

EFI_STATUS
DtDeviceTranslateRangeToCpu (
  IN  DT_DEVICE                 *DtDevice,
  IN  CONST EFI_DT_BUS_ADDRESS  *In,
  IN  CONST EFI_DT_SIZE         *Length,
  OUT EFI_DT_BUS_ADDRESS        *Out,
  OUT DT_DEVICE                 **OutDevice
  );

CONST CHAR8 *
FdtGetDeviceType (
  IN VOID  *TreeBase,
  IN INTN  FdtNode
  );

EFI_DT_STATUS
FdtGetStatus (
  IN VOID  *TreeBase,
  IN INTN  FdtNode
  );

EFI_STATUS
FdtGetAddressCells (
  IN VOID    *TreeBase,
  IN  INTN   FdtNode,
  OUT UINT8  *Cells
  );

EFI_STATUS
FdtGetSizeCells (
  IN VOID    *TreeBase,
  IN INTN    FdtNode,
  OUT UINT8  *Cells
  );

BOOLEAN
FdtGetDmaCoherency (
  IN VOID  *TreeBase,
  IN INTN  FdtNode
  );

BOOLEAN
FdtIsDeviceCritical (
  IN VOID   *TreeBase,
  IN  INTN  FdtNode
  );

EFI_STATUS
EFIAPI
DtIoLookup (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *PathOrAlias,
  IN  BOOLEAN             Connect,
  OUT EFI_HANDLE          *FoundHandle
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
  IN  EFI_HANDLE                DriverBindingHandle,
  IN  EFI_DEVICE_PATH_PROTOCOL  *RemainingDevicePath OPTIONAL
  );

EFI_STATUS
EFIAPI
DtIoRemoveChild (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  EFI_HANDLE          ChildHandle,
  IN  EFI_HANDLE          DriverBindingHandle
  );

EFI_STATUS
EFIAPI
DtIoGetU32 (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *Name,
  IN  UINTN               Index,
  OUT UINT32              *U32
  );

EFI_STATUS
EFIAPI
DtIoGetU64 (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *Name,
  IN  UINTN               Index,
  OUT UINT64              *U64
  );

EFI_STATUS
EFIAPI
DtIoGetU128 (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *Name,
  IN  UINTN               Index,
  OUT EFI_DT_U128         *U128
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
DtIoGetRegByName (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CHAR8               *Name,
  OUT EFI_DT_REG          *Reg
  );

EFI_STATUS
EFIAPI
DtIoGetRange (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CHAR8               *Name,
  IN  UINTN               Index,
  OUT EFI_DT_RANGE        *Range
  );

EFI_STATUS
EFIAPI
DtIoGetString (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *Name,
  IN  UINTN               Index,
  OUT CONST CHAR8         **String
  );

EFI_STATUS
EFIAPI
DtIoGetDevice (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *Name,
  IN  UINTN               Index,
  OUT EFI_HANDLE          *Handle
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
  IN  EFI_DT_IO_PROTOCOL   *This,
  IN OUT  EFI_DT_PROPERTY  *Prop,
  IN  EFI_DT_VALUE_TYPE    Type,
  IN  UINTN                Index,
  OUT VOID                 *Buffer
  );

EFI_STATUS
EFIAPI
DtIoGetStringIndex (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *Name,
  IN  CONST CHAR8         *Value,
  OUT UINTN               *Index
  );

EFI_STATUS
EFIAPI
DtIoPollReg (
  IN  EFI_DT_IO_PROTOCOL        *This,
  IN  EFI_DT_IO_PROTOCOL_WIDTH  Width,
  IN  EFI_DT_REG                *Reg,
  IN  EFI_DT_SIZE               Offset,
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
  IN     EFI_DT_SIZE               Offset,
  IN     UINTN                     Count,
  IN OUT VOID                      *Buffer
  );

EFI_STATUS
EFIAPI
DtIoReadReg (
  IN     EFI_DT_IO_PROTOCOL        *This,
  IN     EFI_DT_IO_PROTOCOL_WIDTH  Width,
  IN     EFI_DT_REG                *Reg,
  IN     EFI_DT_SIZE               Offset,
  IN     UINTN                     Count,
  IN OUT VOID                      *Buffer
  );

EFI_STATUS
EFIAPI
DtIoCopyReg (
  IN  EFI_DT_IO_PROTOCOL        *This,
  IN  EFI_DT_IO_PROTOCOL_WIDTH  Width,
  IN  EFI_DT_REG                *DestReg,
  IN  EFI_DT_SIZE               DestOffset,
  IN  EFI_DT_REG                *SrcReg,
  IN  EFI_DT_SIZE               SrcOffset,
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

EFI_STATUS
EFIAPI
DtIoSetCallbacks (
  IN  EFI_DT_IO_PROTOCOL     *This,
  IN  EFI_HANDLE             AgentHandle,
  IN  EFI_DT_IO_PROTOCOL_CB  *Callbacks
  );

UINT64
MultThenDivU64x64x32 (
  IN  UINT64  Multiplicand,
  IN  UINT64  Multiplier,
  IN  UINT32  Divisor,
  OUT UINT32  *Remainder OPTIONAL
  );

UINT64
GetElapsedTick (
  IN  UINT64  *CurrentTick,
  IN  UINT64  StartTick,
  IN  UINT64  EndTick
  );

CONST CHAR8 *
AsciiStrFindEnd (
  IN  CONST CHAR8  *Start,
  IN  CONST CHAR8  *End
  );

CHAR8 *
AsciiStrChr (
  IN  CHAR8  *Str,
  IN  CHAR8  Chr
  );

#ifndef MDEPKG_NDEBUG
EFI_STATUS
TestsInit (
  VOID
  );

VOID
TestsCleanup (
  VOID
  );

VOID
TestsInvoke (
  IN DT_DEVICE  *DtDevice
  );

#else
#define TestsInit()  EFI_SUCCESS
#define TestsCleanup()
#define TestsInvoke(x)
#endif /* MDEPKG_NDEBUG */

#endif /* __FDT_BUS_DXE_H__ */
