/** @file
    NOR Flash Driver for the cfi-flash DT node.

    Copyright (c) 2011 - 2021, ARM Ltd. All rights reserved.<BR>
    Copyright (c) 2020, Linaro, Ltd. All rights reserved.<BR>
    Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "VirtNorFlash.h"

/**
  Fixup internal data so that EFI can be call in virtual mode.

  @param[in]    Event   The Event that is being processed
  @param[in]    Context Event Context
**/
STATIC
VOID
EFIAPI
VirtualNotifyEvent (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  NOR_FLASH_INSTANCE  *Instance;

  Instance = Context;
  EfiConvertPointer (0x0, (VOID **)&Instance->StorageVariableBase);
  EfiConvertPointer (0x0, (VOID **)&Instance->DeviceBaseAddress);
  EfiConvertPointer (0x0, (VOID **)&Instance->RegionBaseAddress);

  EfiConvertPointer (0x0, (VOID **)&Instance->FvbProtocol.EraseBlocks);
  EfiConvertPointer (0x0, (VOID **)&Instance->FvbProtocol.GetAttributes);
  EfiConvertPointer (0x0, (VOID **)&Instance->FvbProtocol.GetBlockSize);
  EfiConvertPointer (0x0, (VOID **)&Instance->FvbProtocol.GetPhysicalAddress);
  EfiConvertPointer (0x0, (VOID **)&Instance->FvbProtocol.Read);
  EfiConvertPointer (0x0, (VOID **)&Instance->FvbProtocol.SetAttributes);
  EfiConvertPointer (0x0, (VOID **)&Instance->FvbProtocol.Write);

  EfiConvertPointer (0x0, (VOID **)&Instance->ShadowBuffer);
  return;
}

STATIC
EFI_STATUS
NorFlashFvbInitialize (
  IN NOR_FLASH_INSTANCE  *Instance
  )
{
  EFI_STATUS     Status;
  UINT32         FvbNumLba;
  EFI_BOOT_MODE  BootMode;

  DEBUG ((DEBUG_BLKIO, "NorFlashFvbInitialize\n"));
  ASSERT ((Instance != NULL));

  Instance->StorageVariableBase = (PcdGet64 (PcdFlashNvStorageVariableBase64) != 0) ?
                                  PcdGet64 (PcdFlashNvStorageVariableBase64) : PcdGet32 (PcdFlashNvStorageVariableBase);

  // Set the index of the first LBA for the FVB
  Instance->StartLba = (Instance->StorageVariableBase - Instance->RegionBaseAddress) / Instance->BlockSize;

  BootMode = GetBootModeHob ();
  if (BootMode == BOOT_WITH_DEFAULT_SETTINGS) {
    Status = EFI_INVALID_PARAMETER;
  } else {
    // Determine if there is a valid header at the beginning of the NorFlash
    Status = ValidateFvHeader (Instance);
  }

  // Install the Default FVB header if required
  if (EFI_ERROR (Status)) {
    // There is no valid header, so time to install one.
    DEBUG ((DEBUG_INFO, "%a: The FVB Header is not valid.\n", __func__));
    DEBUG ((
      DEBUG_INFO,
      "%a: Installing a correct one for this volume.\n",
      __func__
      ));

    // Erase all the NorFlash that is reserved for variable storage
    FvbNumLba = (PcdGet32 (PcdFlashNvStorageVariableSize) + PcdGet32 (PcdFlashNvStorageFtwWorkingSize) + PcdGet32 (PcdFlashNvStorageFtwSpareSize)) / Instance->BlockSize;

    Status = FvbEraseBlocks (&Instance->FvbProtocol, (EFI_LBA)0, FvbNumLba, EFI_LBA_LIST_TERMINATOR);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    // Install all appropriate headers
    Status = InitializeFvAndVariableStoreHeaders (Instance);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  //
  // The driver implementing the variable read service can now be dispatched;
  // the varstore headers are in place.
  //
  Status = gBS->InstallProtocolInterface (
                  &gImageHandle,
                  &gEdkiiNvVarStoreFormattedGuid,
                  EFI_NATIVE_INTERFACE,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  return Status;
}

/**
  Register a flash child handle with the FVB protocol installed.

  @param[in]    Index                Index of the flash within device.
  @param[in]    NorFlashDeviceBase   Device base.
  @param[in]    NorFlashRegionBase   Region base.
  @param[in]    NorFlashSize         Flash size.
  @param[in]    BlockSize            Block size.
  @param[in]    ControllerHandle     Controller (parent) handle.
  @param[in]    ControllerPath       Parent path.

  @retval EFI_SUCCESS                Success.
  @retval Others                     Errors.

**/
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
  )
{
  EFI_HANDLE                Handle;
  EFI_STATUS                Status;
  VOID                      *OpenProtoData;
  EFI_DEVICE_PATH_PROTOCOL  *HandlePath;
  NOR_FLASH_DEVICE_PATH     *PathNode;
  NOR_FLASH_INSTANCE        *Instance;

  Handle     = NULL;
  HandlePath = NULL;

  PathNode = (NOR_FLASH_DEVICE_PATH *)
             CreateDeviceNode (
               HARDWARE_DEVICE_PATH,
               HW_VENDOR_DP,
               sizeof (NOR_FLASH_DEVICE_PATH)
               );
  if (PathNode == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: CreateDeviceNode: %r\n", __func__, Status));
    goto out;
  }

  CopyGuid (&PathNode->Vendor.Guid, &gEfiCallerIdGuid);
  PathNode->DeviceBaseAddress = NorFlashDeviceBase;
  PathNode->Index             = Index;

  HandlePath = AppendDevicePathNode (
                 ControllerPath,
                 (VOID *)PathNode
                 );
  FreePool (PathNode);
  if (HandlePath == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((
      DEBUG_ERROR,
      "%a: AppendDevicePathNode: %r\n",
      __func__,
      Status
      ));
    goto out;
  }

  Instance = AllocateRuntimeZeroPool (sizeof (NOR_FLASH_INSTANCE));
  if (Instance == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: AllocateRuntimeZeroPool 0x%x: %r\n", __func__, sizeof (NOR_FLASH_INSTANCE), Status));
    goto out;
  }

  Instance->Signature                      = NOR_FLASH_SIGNATURE;
  Instance->DeviceBaseAddress              = NorFlashDeviceBase;
  Instance->RegionBaseAddress              = NorFlashRegionBase;
  Instance->Size                           = NorFlashSize;
  Instance->LastBlock                      = (NorFlashSize / BlockSize) - 1;
  Instance->BlockSize                      = BlockSize;
  Instance->FvbProtocol.GetAttributes      = FvbGetAttributes;
  Instance->FvbProtocol.SetAttributes      = FvbSetAttributes;
  Instance->FvbProtocol.GetPhysicalAddress = FvbGetPhysicalAddress;
  Instance->FvbProtocol.GetBlockSize       = FvbGetBlockSize;
  Instance->FvbProtocol.Read               = FvbRead;
  Instance->FvbProtocol.Write              = FvbWrite;
  Instance->FvbProtocol.EraseBlocks        = FvbEraseBlocks;

  Instance->ShadowBuffer = AllocateRuntimeZeroPool (BlockSize);
  if (Instance->ShadowBuffer == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((DEBUG_ERROR, "%a: AllocateRuntimeZeroPool 0x%x: %r\n", __func__, BlockSize, Status));
    goto out;
  }

  NorFlashFvbInitialize (Instance);
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gEfiDevicePathProtocolGuid,
                  HandlePath,
                  &gEfiFirmwareVolumeBlockProtocolGuid,
                  &Instance->FvbProtocol,
                  NULL
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: InstallMultipleProtocolInterfaces: %r\n", __func__, Status));
    goto out;
  }

  Instance->Handle = Handle;

  //
  // Register for the virtual address change event, so we can
  // update our pointers.
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_NOTIFY,
                  VirtualNotifyEvent,
                  Instance,
                  &gEfiEventVirtualAddressChangeGuid,
                  &Instance->VirtualAddrChangeEvent
                  );
  ASSERT_EFI_ERROR (Status);

  //
  // TODO: Use a TBD DtIo interface to post an update to the DT
  // to mark the managed DT controller as disabled (so it gets
  // ignored by the OS, if the DT is exposed to the OS).
  //

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  &gEfiDtIoProtocolGuid,
                  &OpenProtoData,
                  DriverBindingHandle,
                  Handle,
                  EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER: %r\n", __func__, Status));
    goto out;
  }

out:
  if (EFI_ERROR (Status)) {
    if (Instance != NULL) {
      if (Instance->ShadowBuffer != NULL) {
        FreePool (Instance->ShadowBuffer);
      }

      FreePool (Instance);
    }

    if (Handle != NULL) {
      gBS->UninstallMultipleProtocolInterfaces (
             Handle,
             &gEfiDevicePathProtocolGuid,
             HandlePath,
             NULL
             );
    }

    if (HandlePath != NULL) {
      FreePool (HandlePath);
    }
  }

  return Status;
}

/**
  The Entry Point for VirtNorFlashDxe driver.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
NorFlashInitialise (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  return EfiLibInstallDriverBindingComponentName2 (
           ImageHandle,
           SystemTable,
           &gDriverBinding,
           ImageHandle,
           &gComponentName,
           &gComponentName2
           );
}
