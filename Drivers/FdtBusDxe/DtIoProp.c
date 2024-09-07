/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "FdtBusDxe.h"

/**
  Given a string list property name and a value of one of the strings,
  returns the strings index.

  This is useful to look up other properties indexed by name, e.g.
  foo = <value1>, <value2>, <value3>;
  foo-names = "index1", "index2", "index3";

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Name                  Property to examine.
  @param  Value                 String to search for.
  @param  Index                 Pointer for returning found index.

  @retval EFI_SUCCESS           String found.
  @retval EFI_NOT_FOUND         Could not find property or string.
  @retval EFI_DEVICE_ERROR      Devicetree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
EFI_STATUS
EFIAPI
DtIoGetStringIndex (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *Name,
  IN  CONST CHAR8         *Value,
  OUT UINTN               *Index
  )
{
  EFI_STATUS       Status;
  UINTN            CurrentIndex;
  EFI_DT_PROPERTY  Property;
  CONST CHAR8      *EndOfString;

  if ((This == NULL) || (Name == NULL) ||
      (Value == NULL) || (Index == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  Status = DtIoGetProp (This, Name, &Property);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  CurrentIndex = 0;
  while (Property.Iter < Property.End) {
    EndOfString = AsciiStrFindEnd (Property.Iter, Property.End);
    if (EndOfString == NULL) {
      return EFI_NOT_FOUND;
    }

    if (AsciiStrCmp (Property.Iter, Value) == 0) {
      *Index = CurrentIndex;
      return EFI_SUCCESS;
    }

    CurrentIndex++;
    Property.Iter = EndOfString;
  }

  return EFI_NOT_FOUND;
}

/**
  Looks up a UINT32 property value by index.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Name                  Name of the property.
  @param  Index                 Index of the value to return.
  @param  U32                   Pointer to a UINT32.

  @retval EFI_SUCCESS           Lookup successful.
  @retval EFI_NOT_FOUND         Could not find property.
  @retval EFI_DEVICE_ERROR      Devicetree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
EFI_STATUS
EFIAPI
DtIoGetU32 (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *Name,
  IN  UINTN               Index,
  OUT UINT32              *U32
  )
{
  EFI_STATUS       Status;
  EFI_DT_PROPERTY  Property;
  DT_DEVICE        *DtDevice;

  if ((This == NULL) || (Name == NULL) || (U32 == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  DtDevice = DT_DEV_FROM_THIS (This);

  Status = DtIoGetProp (This, Name, &Property);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = DtIoParseProp (
             This,
             &Property,
             EFI_DT_VALUE_U32,
             Index,
             U32
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Looks up a UINT64 property value by index.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Name                  Name of the property.
  @param  Index                 Index of the value to return.
  @param  U64                   Pointer to a UINT64.

  @retval EFI_SUCCESS           Lookup successful.
  @retval EFI_NOT_FOUND         Could not find property.
  @retval EFI_DEVICE_ERROR      Devicetree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
EFI_STATUS
EFIAPI
DtIoGetU64 (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *Name,
  IN  UINTN               Index,
  OUT UINT64              *U64
  )
{
  EFI_STATUS       Status;
  EFI_DT_PROPERTY  Property;
  DT_DEVICE        *DtDevice;

  if ((This == NULL) || (Name == NULL) || (U64 == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  DtDevice = DT_DEV_FROM_THIS (This);

  Status = DtIoGetProp (This, Name, &Property);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = DtIoParseProp (
             This,
             &Property,
             EFI_DT_VALUE_U64,
             Index,
             U64
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Looks up an EFI_DT_U128 property value by index.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Name                  Name of the property.
  @param  Index                 Index of the value to return.
  @param  U128                  Pointer to an EFI_DT_U128.

  @retval EFI_SUCCESS           Lookup successful.
  @retval EFI_NOT_FOUND         Could not find property.
  @retval EFI_DEVICE_ERROR      Devicetree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
EFI_STATUS
EFIAPI
DtIoGetU128 (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *Name,
  IN  UINTN               Index,
  OUT EFI_DT_U128         *U128
  )
{
  EFI_STATUS       Status;
  EFI_DT_PROPERTY  Property;
  DT_DEVICE        *DtDevice;

  if ((This == NULL) || (Name == NULL) || (U128 == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  DtDevice = DT_DEV_FROM_THIS (This);

  Status = DtIoGetProp (This, Name, &Property);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = DtIoParseProp (
             This,
             &Property,
             EFI_DT_VALUE_U128,
             Index,
             U128
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Looks up a reg property value by index for a EFI_DT_IO_PROTOCOL instance.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Index                 Index of the reg value to return.
  @param  Reg                   Pointer to the EFI_DT_REG to fill.

  @retval EFI_SUCCESS           Lookup successful.
  @retval EFI_NOT_FOUND         Could not find property.
  @retval EFI_DEVICE_ERROR      Devicetree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
EFI_STATUS
EFIAPI
DtIoGetReg (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  UINTN               Index,
  OUT EFI_DT_REG          *Reg
  )
{
  EFI_STATUS       Status;
  EFI_DT_PROPERTY  Property;
  DT_DEVICE        *DtDevice;

  if ((This == NULL) || (Reg == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  DtDevice = DT_DEV_FROM_THIS (This);

  Status = DtIoGetProp (This, "reg", &Property);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = DtIoParseProp (
             This,
             &Property,
             EFI_DT_VALUE_REG,
             Index,
             Reg
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Looks up a reg property value by name for a EFI_DT_IO_PROTOCOL instance.

  Note: Lookups by name involve examining the reg-names property.

  Note 2: The returned address is in CPU space, not bus space, if these are
  different.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Name                  Name of the reg value to return.
  @param  Reg                   Pointer to the EFI_DT_REG to fill.

  @retval EFI_SUCCESS           Lookup successful.
  @retval EFI_NOT_FOUND         Could not find property.
  @retval EFI_DEVICE_ERROR      Devicetree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
EFI_STATUS
EFIAPI
DtIoGetRegByName (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CHAR8               *Name,
  OUT EFI_DT_REG          *Reg
  )
{
  EFI_STATUS  Status;
  UINTN       Index;

  if ((This == NULL) || (Name == NULL) || (Reg == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Status = DtIoGetStringIndex (This, "reg-names", Name, &Index);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return DtIoGetReg (This, Index, Reg);
}

/**
  Looks up a ranges property value by index.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Name                  Name of the ranges property to examine.
  @param  Index                 Index of the ranges value to return.
  @param  Range                 Pointer to an EFI_DT_RANGE.

  @retval EFI_SUCCESS           Lookup successful.
  @retval EFI_NOT_FOUND         Could not find property.
  @retval EFI_DEVICE_ERROR      Devicetree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
EFI_STATUS
EFIAPI
DtIoGetRange (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CHAR8               *Name,
  IN  UINTN               Index,
  OUT EFI_DT_RANGE        *Range
  )
{
  EFI_STATUS       Status;
  EFI_DT_PROPERTY  Property;
  DT_DEVICE        *DtDevice;

  if ((This == NULL) || (Name == NULL) || (Range == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  DtDevice = DT_DEV_FROM_THIS (This);

  Status = DtIoGetProp (This, Name, &Property);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = DtIoParseProp (
             This,
             &Property,
             EFI_DT_VALUE_RANGE,
             Index,
             Range
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Looks up a string property value by index.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Name                  Name of the property.
  @param  Index                 Index of the string to return.
  @param  String                Pointer to a CHAR8*.

  @retval EFI_SUCCESS           Lookup successful.
  @retval EFI_NOT_FOUND         Could not find property.
  @retval EFI_DEVICE_ERROR      Devicetree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
EFI_STATUS
EFIAPI
DtIoGetString (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *Name,
  IN  UINTN               Index,
  OUT CONST CHAR8         **String
  )
{
  EFI_STATUS       Status;
  EFI_DT_PROPERTY  Property;
  DT_DEVICE        *DtDevice;

  if ((This == NULL) || (Name == NULL) || (String == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  DtDevice = DT_DEV_FROM_THIS (This);

  Status = DtIoGetProp (This, Name, &Property);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = DtIoParseProp (
             This,
             &Property,
             EFI_DT_VALUE_STRING,
             Index,
             String
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Looks up a device EFI_HANDLE from property value by index.

  @param  This                  A pointer to the EFI_DT_IO_PROTOCOL instance.
  @param  Name                  Name of the property.
  @param  Index                 Index of the device to return.
  @param  Handle                Pointer to an EFI_HANDLE.

  @retval EFI_SUCCESS           Lookup successful.
  @retval EFI_NOT_FOUND         Could not find property.
  @retval EFI_DEVICE_ERROR      Devicetree error.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.

**/
EFI_STATUS
EFIAPI
DtIoGetDevice (
  IN  EFI_DT_IO_PROTOCOL  *This,
  IN  CONST CHAR8         *Name,
  IN  UINTN               Index,
  OUT EFI_HANDLE          *Handle
  )
{
  EFI_STATUS       Status;
  EFI_DT_PROPERTY  Property;
  DT_DEVICE        *DtDevice;

  if ((This == NULL) || (Name == NULL) || (Handle == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  DtDevice = DT_DEV_FROM_THIS (This);

  Status = DtIoGetProp (This, Name, &Property);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = DtIoParseProp (
             This,
             &Property,
             EFI_DT_VALUE_DEVICE,
             Index,
             Handle
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return EFI_SUCCESS;
}
