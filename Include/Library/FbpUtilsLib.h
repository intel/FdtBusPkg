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
  Convert an EFI_DT_RANGE to an EFI_DT_REG.

  If the EFI_DT_REG is going to be use with
  ChildBase address offsets, pass ForChildSide
  as TRUE.

  Useful in a bus driver to do I/O on behalf of a child.

  @param[in]  Range            EFI_DT_RANGE *.
  @param[in]  ForChildSide     BOOLEAN.
  @param[out] Reg              EFI_DT_REG *.

**/
STATIC
inline
VOID
FbpRangeToReg (
  IN  CONST EFI_DT_RANGE  *Range,
  IN  BOOLEAN             ForChildSide,
  OUT EFI_DT_REG          *Reg
  )
{
  EFI_DT_SIZE  Offset;

  if (ForChildSide) {
    Offset = Range->ChildBase;
  } else {
    Offset = 0;
  }

  Reg->BusBase        = Range->ParentBase - Offset;
  Reg->TranslatedBase = Range->TranslatedParentBase - Offset;
  Reg->Length         = Range->Length + Offset;
  Reg->BusDtIo        = Range->BusDtIo;
}

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
  OUT EFI_PHYSICAL_ADDRESS  *Address OPTIONAL
  )
{
  if (Reg->BusDtIo != NULL) {
    return EFI_UNSUPPORTED;
  }

  if (Address != NULL) {
    *Address = Reg->TranslatedBase;
  }

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
  OUT EFI_PHYSICAL_ADDRESS  *Address OPTIONAL
  )
{
  if (Range->BusDtIo != NULL) {
    return EFI_UNSUPPORTED;
  }

  if (Address != NULL) {
    *Address = Range->TranslatedParentBase;
  }

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
