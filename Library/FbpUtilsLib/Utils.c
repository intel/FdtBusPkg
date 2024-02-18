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

  @param[in]  Name             FBP_DT_ROOT_NAME or FBP_DT_TEST_ROOT_NAME.

  @retval NULL                 Failed to find controller or allocate memory.
  @retval Other                DT I/O Protocol.

**/
STATIC
EFI_DT_IO_PROTOCOL *
FbpGetRootByName (
  IN  CONST CHAR8  *Name
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

/**
  Given a EFI_HANDLE, return if the handle has a driver started
  on it, which means the handle was opened on behalf of itself
  BY_DRIVER.

  @param[in]    Handle                EFI_HANDLE.
  @param[in]    ExtraAttributeChecks  Extra attributes to validate against
                                      the Attribute field in a
                                      EFI_OPEN_PROTOCOL_INFORMATION_ENTRY.
  @oaram[out]   MatchingEntry         EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *

  @retval TRUE                        Has driver bound/started.
  @retval FALSE                       Does not have a driver bound/started.

**/
BOOLEAN
FbpHandleHasBoundDriver (
  IN  EFI_HANDLE                           Handle,
  IN  UINT32                               ExtraAttributeChecks,
  OUT EFI_OPEN_PROTOCOL_INFORMATION_ENTRY  *MatchingEntry OPTIONAL
  )
{
  EFI_STATUS                           Status;
  UINTN                                EntryCount;
  UINTN                                Index;
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY  *OpenInfoBuffer;

  //
  // Detect if the child handle has a driver bound to it, by
  // retrieving the list of agents that are consuming
  // gEfiDtIoProtocolGuid on the handle.
  //
  Status = gBS->OpenProtocolInformation (
                  Handle,
                  &gEfiDtIoProtocolGuid,
                  &OpenInfoBuffer,
                  &EntryCount
                  );
  if (EFI_ERROR (Status)) {
    return FALSE;
  }

  for (Index = 0; Index < EntryCount; Index++) {
    EFI_OPEN_PROTOCOL_INFORMATION_ENTRY  *Entry = &OpenInfoBuffer[Index];

    if (((Entry->Attributes & (EFI_OPEN_PROTOCOL_BY_DRIVER | ExtraAttributeChecks)) ==
         (EFI_OPEN_PROTOCOL_BY_DRIVER | ExtraAttributeChecks)) &&
        (Entry->ControllerHandle == Handle))
    {
      if (MatchingEntry != NULL) {
        *MatchingEntry = *Entry;
      }

      break;
    }
  }

  FreePool (OpenInfoBuffer);
  if (Index != EntryCount) {
    //
    // The child handle was opened by a bound driver.
    //
    return TRUE;
  }

  return FALSE;
}

/**
  Retrieves a Unicode string that is the user readable name of the controller
  that is being managed by a driver. This is a special helper for use by
  bus drivers that produce EFI_DT_IO_PROTOCOL children. This function only
  succeeds if the controller specified by ChildHandle does not have a
  driver bound to it. This allows the driver managing ChildHandle to provide
  its own readable name, yet fall back to the bus driver if the device is
  unbound.

  @param[in]  ControllerHandle  Controller managed by the bus driver using this
                                helper.

  @param[in]  ChildHandle       Child controller created the bus driver.

  @param[in]  Language          A pointer to a Null-terminated ASCII string
                                array indicating the language.  This is the
                                language of the driver name that the caller is
                                requesting, and it must match one of the
                                languages specified in SupportedLanguages. The
                                number of languages supported by a driver is up
                                to the driver writer. Language is specified in
                                RFC 4646 or ISO 639-2 language code format.

  @param[out]  ControllerName   A pointer to the Unicode string to return.
                                This Unicode string is the name of the
                                controller specified by ControllerHandle and
                                ChildHandle in the language specified by
                                Language from the point of view of the driver
                                specified by This.

  @retval EFI_SUCCESS           Success.

  @retval EFI_UNSUPPORTED       The bus driver is not currently managing
                                the controller specified by ControllerHandle
                                and ChildHandle.

  @retval EFI_UNSUPPORTED       The bus driver does not support the Language
                                the language specified by Language.

  @retval EFI_UNSUPPORTED       ChildHandle has a bound driver.

  @retval EFI_UNSUPPORTED       ChildHandle not a DT device.

**/
EFI_STATUS
FbpBusComponentName (
  IN  EFI_HANDLE  ControllerHandle,
  IN  EFI_HANDLE  ChildHandle,
  IN  CHAR8       *Language,
  OUT CHAR16      **ControllerName
  )
{
  EFI_STATUS                           Status;
  EFI_DT_IO_PROTOCOL                   *DtIoProtocol;
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY  InfoEntry;

  ASSERT (ChildHandle != NULL);

  Status = EfiTestChildHandle (
             ControllerHandle,
             ChildHandle,
             &gEfiDtIoProtocolGuid
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if (FbpHandleHasBoundDriver (ChildHandle, 0, &InfoEntry)) {
    VOID  *Scratch;

    ASSERT (InfoEntry.AgentHandle != NULL);
    if (gBS->HandleProtocol (
               InfoEntry.AgentHandle,
               &gEfiDriverBindingProtocolGuid,
               &Scratch
               ) == EFI_SUCCESS)
    {
      //
      // There is a UEFI Driver Model-compliant driver bound,
      // punt to it to display its own component name info.
      //
      return EFI_UNSUPPORTED;
    }

    *ControllerName = L"Legacy-Managed Device";
    return EFI_SUCCESS;
  }

  Status = gBS->HandleProtocol (
                  ChildHandle,
                  &gEfiDtIoProtocolGuid,
                  (VOID **)&DtIoProtocol
                  );
  if (EFI_ERROR (Status)) {
    return EFI_UNSUPPORTED;
  }

  *ControllerName = DtIoProtocol->ComponentName;
  return EFI_SUCCESS;
}
