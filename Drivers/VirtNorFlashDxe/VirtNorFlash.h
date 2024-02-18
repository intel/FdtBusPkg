/** @file
    NOR Flash Driver for the cfi-flash DT node.

    Copyright (c) 2011 - 2021, ARM Ltd. All rights reserved.<BR>
    Copyright (c) 2020, Linaro, Ltd. All rights reserved.<BR>
    Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __VIRT_NOR_FLASH__
#define __VIRT_NOR_FLASH__

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeLib.h>
#include <Library/VirtNorFlashPlatformLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/DevicePathLib.h>
#include <Library/FbpUtilsLib.h>
#include <Guid/EventGroup.h>
#include <Guid/NvVarStoreFormatted.h>
#include <Guid/SystemNvDataGuid.h>
#include <Guid/VariableFormat.h>
#include <Protocol/DtIo.h>
#include <Protocol/FirmwareVolumeBlock.h>

#define QEMU_NOR_BLOCK_SIZE    SIZE_256KB
#define NOR_FLASH_ERASE_RETRY  10

// Device access macros
// These are necessary because we use 2 x 16bit parts to make up 32bit data

#define HIGH_16_BITS  0xFFFF0000
#define LOW_16_BITS   0x0000FFFF
#define LOW_8_BITS    0x000000FF

#define FOLD_32BIT_INTO_16BIT(value)  ( ( value >> 16 ) | ( value & LOW_16_BITS ) )

#define GET_LOW_BYTE(value)   ( value & LOW_8_BITS )
#define GET_HIGH_BYTE(value)  ( GET_LOW_BYTE( value >> 16 ) )

// Each command must be sent simultaneously to both chips,
// i.e. at the lower 16 bits AND at the higher 16 bits
#define CREATE_NOR_ADDRESS(BaseAddr, OffsetAddr)       ((BaseAddr) + ((OffsetAddr) << 2))
#define CREATE_DUAL_CMD(Cmd)                           ( ( Cmd << 16) | ( Cmd & LOW_16_BITS) )
#define SEND_NOR_COMMAND(BaseAddr, Offset, Cmd)        MmioWrite32 (CREATE_NOR_ADDRESS(BaseAddr,Offset), CREATE_DUAL_CMD(Cmd))
#define GET_NOR_BLOCK_ADDRESS(BaseAddr, Lba, LbaSize)  ( BaseAddr + (UINTN)((Lba) * LbaSize) )

// Status Register Bits
#define P30_SR_BIT_WRITE            (BIT7 << 16 | BIT7)
#define P30_SR_BIT_ERASE_SUSPEND    (BIT6 << 16 | BIT6)
#define P30_SR_BIT_ERASE            (BIT5 << 16 | BIT5)
#define P30_SR_BIT_PROGRAM          (BIT4 << 16 | BIT4)
#define P30_SR_BIT_VPP              (BIT3 << 16 | BIT3)
#define P30_SR_BIT_PROGRAM_SUSPEND  (BIT2 << 16 | BIT2)
#define P30_SR_BIT_BLOCK_LOCKED     (BIT1 << 16 | BIT1)
#define P30_SR_BIT_BEFP             (BIT0 << 16 | BIT0)

// Device Commands for Intel StrataFlash(R) Embedded Memory (P30) Family

// On chip buffer size for buffered programming operations
// There are 2 chips, each chip can buffer up to 32 (16-bit)words, and each word is 2 bytes.
// Therefore the total size of the buffer is 2 x 32 x 2 = 128 bytes
#define P30_MAX_BUFFER_SIZE_IN_BYTES  ((UINTN)128)
#define P30_MAX_BUFFER_SIZE_IN_WORDS  (P30_MAX_BUFFER_SIZE_IN_BYTES/((UINTN)4))
#define MAX_BUFFERED_PROG_ITERATIONS  10000000
#define BOUNDARY_OF_32_WORDS          0x7F

// READ Commands
#define P30_CMD_READ_DEVICE_ID         0x0090
#define P30_CMD_READ_STATUS_REGISTER   0x0070
#define P30_CMD_CLEAR_STATUS_REGISTER  0x0050
#define P30_CMD_READ_ARRAY             0x00FF

// WRITE Commands
#define P30_CMD_WORD_PROGRAM_SETUP        0x0040
#define P30_CMD_BUFFERED_PROGRAM_SETUP    0x00E8
#define P30_CMD_BUFFERED_PROGRAM_CONFIRM  0x00D0

// ERASE Commands
#define P30_CMD_BLOCK_ERASE_SETUP    0x0020
#define P30_CMD_BLOCK_ERASE_CONFIRM  0x00D0

// BLOCK LOCKING / UNLOCKING Commands
#define P30_CMD_LOCK_BLOCK_SETUP  0x0060
#define P30_CMD_LOCK_BLOCK        0x0001
#define P30_CMD_UNLOCK_BLOCK      0x00D0
#define P30_CMD_LOCK_DOWN_BLOCK   0x002F

#define NOR_FLASH_SIGNATURE  SIGNATURE_32('n', 'o', 'r', '0')
#define INSTANCE_FROM_FVB_THIS(a)  CR(a, NOR_FLASH_INSTANCE, FvbProtocol, NOR_FLASH_SIGNATURE)

typedef struct _NOR_FLASH_INSTANCE NOR_FLASH_INSTANCE;

extern EFI_COMPONENT_NAME_PROTOCOL   gComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL  gComponentName2;
extern EFI_DRIVER_BINDING_PROTOCOL   gDriverBinding;

#pragma pack (1)
typedef struct {
  VENDOR_DEVICE_PATH      Vendor;
  EFI_PHYSICAL_ADDRESS    DeviceBaseAddress;
  UINT8                   Index;
} NOR_FLASH_DEVICE_PATH;
#pragma pack ()

struct _NOR_FLASH_INSTANCE {
  UINT32                                 Signature;
  EFI_HANDLE                             Handle;

  UINTN                                  DeviceBaseAddress;
  UINTN                                  RegionBaseAddress;
  UINTN                                  Size;
  EFI_LBA                                StartLba;
  EFI_LBA                                LastBlock;
  UINT32                                 BlockSize;

  EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL    FvbProtocol;
  VOID                                   *ShadowBuffer;

  UINTN                                  StorageVariableBase;
  EFI_EVENT                              VirtualAddrChangeEvent;
};

//
// VirtNorFlashDxe.c
//

EFI_STATUS
ChildCreate (
  IN  UINTN                     Index,
  IN  UINTN                     NorFlashDeviceBase,
  IN  UINTN                     NorFlashRegionBase,
  IN  UINTN                     NorFlashSize,
  IN  UINT32                    BlockSize,
  IN  EFI_HANDLE                ControllerHandle,
  IN  EFI_HANDLE                DriverBindingHandle,
  IN  EFI_DEVICE_PATH_PROTOCOL  *ControllerPath
  );

//
// VirtNorFlashFvb.c
//

EFI_STATUS
EFIAPI
FvbGetAttributes (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL  *This,
  OUT       EFI_FVB_ATTRIBUTES_2                 *Attributes
  );

EFI_STATUS
EFIAPI
FvbSetAttributes (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL  *This,
  IN OUT    EFI_FVB_ATTRIBUTES_2                 *Attributes
  );

EFI_STATUS
EFIAPI
FvbGetPhysicalAddress (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL  *This,
  OUT       EFI_PHYSICAL_ADDRESS                 *Address
  );

EFI_STATUS
EFIAPI
FvbGetBlockSize (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL  *This,
  IN        EFI_LBA                              Lba,
  OUT       UINTN                                *BlockSize,
  OUT       UINTN                                *NumberOfBlocks
  );

EFI_STATUS
EFIAPI
FvbRead (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL  *This,
  IN        EFI_LBA                              Lba,
  IN        UINTN                                Offset,
  IN OUT    UINTN                                *NumBytes,
  IN OUT    UINT8                                *Buffer
  );

EFI_STATUS
EFIAPI
FvbWrite (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL  *This,
  IN        EFI_LBA                              Lba,
  IN        UINTN                                Offset,
  IN OUT    UINTN                                *NumBytes,
  IN        UINT8                                *Buffer
  );

EFI_STATUS
EFIAPI
FvbEraseBlocks (
  IN CONST  EFI_FIRMWARE_VOLUME_BLOCK2_PROTOCOL  *This,
  ...
  );

EFI_STATUS
ValidateFvHeader (
  IN  NOR_FLASH_INSTANCE  *Instance
  );

EFI_STATUS
InitializeFvAndVariableStoreHeaders (
  IN NOR_FLASH_INSTANCE  *Instance
  );

//
// VirtNorFlash.c
//

EFI_STATUS
NorFlashWriteFullBlock (
  IN NOR_FLASH_INSTANCE  *Instance,
  IN EFI_LBA             Lba,
  IN UINT32              *DataBuffer,
  IN UINT32              BlockSizeInWords
  );

EFI_STATUS
NorFlashUnlockAndEraseSingleBlock (
  IN NOR_FLASH_INSTANCE  *Instance,
  IN UINTN               BlockAddress
  );

EFI_STATUS
NorFlashWriteSingleBlock (
  IN        NOR_FLASH_INSTANCE  *Instance,
  IN        EFI_LBA             Lba,
  IN        UINTN               Offset,
  IN OUT    UINTN               *NumBytes,
  IN        UINT8               *Buffer
  );

EFI_STATUS
NorFlashReadBlocks (
  IN NOR_FLASH_INSTANCE  *Instance,
  IN EFI_LBA             Lba,
  IN UINTN               BufferSizeInBytes,
  OUT VOID               *Buffer
  );

EFI_STATUS
NorFlashRead (
  IN NOR_FLASH_INSTANCE  *Instance,
  IN EFI_LBA             Lba,
  IN UINTN               Offset,
  IN UINTN               BufferSizeInBytes,
  OUT VOID               *Buffer
  );

EFI_STATUS
NorFlashWrite (
  IN        NOR_FLASH_INSTANCE  *Instance,
  IN        EFI_LBA             Lba,
  IN        UINTN               Offset,
  IN OUT    UINTN               *NumBytes,
  IN        UINT8               *Buffer
  );

#endif /* __VIRT_NOR_FLASH__ */
