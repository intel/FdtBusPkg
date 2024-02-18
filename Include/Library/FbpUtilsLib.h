/** @file

    Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __FBP_UTILS_LIB_H__
#define __FBP_UTILS_LIB_H__

#include <Uefi.h>
#include <Protocol/DtIo.h>

//
// The name associated with the root DT controller, which
// corresponds to '/' in the Devicetree. Because a DEBUG
// build has a second tree used for unit testing, there
// are real names assigned for these.
//
#define FBP_DT_ROOT_NAME       "DtRoot"
#define FBP_DT_TEST_ROOT_NAME  "DtTestRoot"

EFI_DT_IO_PROTOCOL *
FbpGetDtRoot (
  VOID
  );

EFI_DT_IO_PROTOCOL *
FbpGetDtTestRoot (
  VOID
  );

EFI_DT_DEVICE_PATH_NODE *
FbpPathNodeCreate (
  IN  CONST CHAR8  *Name
  );

/**
  Return the EFI_PHYSICAL_ADDRESS corresponding to an EFI_DT_REG,
  if one exists.

  @param[in]  Reg              EFI_DT_REG *.
  @param[out] Address          EFI_PHYSICAL_ADDRESS *.

  @retval EFI_STATUS           EFI_SUCCESS or error.

**/
STATIC
inline
EFI_STATUS
FbpRegToPhysicalAddress (
  IN  CONST EFI_DT_REG      *Reg,
  OUT EFI_PHYSICAL_ADDRESS  *Address
  )
{
  if (Reg->BusDtIo != NULL) {
    return EFI_UNSUPPORTED;
  }

  *Address = Reg->TranslatedBase;
  return EFI_SUCCESS;
}

/**
  Return the EFI_PHYSICAL_ADDRESS corresponding to an EFI_DT_RANGE,
  if one exists.

  @param[in]  Range            EFI_DuT_RANGE *.
  @param[out] Address          EFI_PHYSICAL_ADDRESS *.

  @retval EFI_STATUS           EFI_SUCCESS or error.

**/
STATIC
inline
EFI_STATUS
FbpRangeToPhysicalAddress (
  IN  CONST EFI_DT_RANGE    *Range,
  OUT EFI_PHYSICAL_ADDRESS  *Address
  )
{
  if (Range->BusDtIo != NULL) {
    return EFI_UNSUPPORTED;
  }

  *Address = Range->TranslatedParentBase;
  return EFI_SUCCESS;
}

BOOLEAN
FbpHandleHasBoundDriver (
  IN  EFI_HANDLE                           Handle,
  IN  UINT32                               ExtraAttributeChecks,
  OUT EFI_OPEN_PROTOCOL_INFORMATION_ENTRY  *MatchingEntry OPTIONAL
  );

EFI_STATUS
FbpBusComponentName (
  IN  EFI_HANDLE  ControllerHandle,
  IN  EFI_HANDLE  ChildHandle,
  IN  CHAR8       *Language,
  OUT CHAR16      **ControllerName
  );

#endif /* __FBP_UTILS_LIB_H__ */
