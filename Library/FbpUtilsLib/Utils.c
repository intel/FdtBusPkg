/** @file

    Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/FbpUtilsLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/FbpUtilsLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>

/**
  Return the DT I/O protocol corresponding to the root DT controller
  with the passed name.

  @retval NULL                 Failed to find controller or allocate memory.
  @retval Other                DT I/O Protocol.

**/
STATIC
EFI_DT_IO_PROTOCOL *
FbpGetRootByName (
  CONST CHAR8  *Name
  )
{
  EFI_DT_DEVICE_PATH_NODE   *DevicePathNode;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  EFI_DEVICE_PATH_PROTOCOL  *RemPath;
  EFI_DT_IO_PROTOCOL        *DtIo;
  EFI_STATUS                Status;
  EFI_HANDLE                Handle;

  DevicePathNode = FbpPathNodeCreate (Name);
  if (DevicePathNode == NULL) {
    return NULL;
  }

  DevicePath = AppendDevicePathNode (NULL, (VOID *)DevicePathNode);
  FreePool (DevicePathNode);

  RemPath = DevicePath;
  Status  = gBS->LocateDevicePath (&gEfiDtIoProtocolGuid, &RemPath, &Handle);
  FreePool (DevicePath);

  if (EFI_ERROR (Status)) {
    return NULL;
  }

  Status = gBS->HandleProtocol (Handle, &gEfiDtIoProtocolGuid, (VOID **)&DtIo);
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  return DtIo;
}

/**
  Return the DT I/O protocol corresponding to the root DT controller,
  which corresponds to the '/' node in the Devicetree.

  @retval NULL                 Failed to find controller or allocate memory.
  @retval Other                DT I/O Protocol.

**/
EFI_DT_IO_PROTOCOL *
FbpGetDtRoot (
  VOID
  )
{
  return FbpGetRootByName (FBP_DT_ROOT_NAME);
}

/**
  Return the DT I/O protocol corresponding to the test root DT controller,
  which corresponds to the '/' node in the testing Devicetree.

  This will only succeed with DEBUG FdtBusDxe builds, which include the
  unit tests.

  @retval NULL                 Failed to find controller or allocate memory.
  @retval Other                DT I/O Protocol.

**/
EFI_DT_IO_PROTOCOL *
FbpGetDtTestRoot (
  VOID
  )
{
  return FbpGetRootByName (FBP_DT_TEST_ROOT_NAME);
}

/**
  Given an ASCII name, allocate/fill a EFI_DT_DEVICE_PATH_NODE.

  @param[in]    Name           ASCII string.

  @retval NULL                 Failed to allocate memory.
  @retval Others               EFI_DT_DEVICE_PATH_NODE.

**/
EFI_DT_DEVICE_PATH_NODE *
FbpPathNodeCreate (
  IN  CONST CHAR8  *Name
  )
{
  UINTN                    Size;
  EFI_STATUS               Status;
  EFI_DT_DEVICE_PATH_NODE  *Node;

  Size = AsciiStrSize (Name);
  Node = AllocateZeroPool (sizeof (EFI_DT_DEVICE_PATH_NODE) + Size);
  if (Node == NULL) {
    return NULL;
  }

  Node->VendorDevicePath.Header.Type      = HARDWARE_DEVICE_PATH;
  Node->VendorDevicePath.Header.SubType   = HW_VENDOR_DP;
  Node->VendorDevicePath.Header.Length[0] =
    (UINT8)(sizeof (EFI_DT_DEVICE_PATH_NODE) + Size);
  Node->VendorDevicePath.Header.Length[1] =
    (UINT8)((sizeof (EFI_DT_DEVICE_PATH_NODE) + Size) >> 8);
  CopyGuid (&Node->VendorDevicePath.Guid, &gEfiDtDevicePathGuid);

  Status = AsciiStrCpyS (Node->Name, Size, Name);
  if (EFI_ERROR (Status)) {
    FreePool (Node);
    DEBUG ((DEBUG_ERROR, "%a: AsciiStrCpyS: %r\n", __func__, Status));
    return NULL;
  }

  return Node;
}
