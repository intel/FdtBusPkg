/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "FdtBusDxe.h"

#ifndef MDEPKG_NDEBUG
  #include "TestDt.dtbi"

#define TEST_DECL(x)  { #x, Test ## x ## Fn }

#define TEST_DEF(x)                             \
  STATIC                                        \
  BOOLEAN                                       \
  EFIAPI                                        \
  Test ## x ## Fn (                             \
    IN  DT_DEVICE  *DtDevice                    \
  )                                             \

typedef struct {
  const char    *Name;
  BOOLEAN EFIAPI (*Fn)(IN  DT_DEVICE *DtDevice);
} TestDesc;

VOID  *gTestTreeBase;

STATIC UINT32  Dt_DeviceRegs_TestTemplate00[] = {
  // 0x86,0x80,0xAA,0x8F,0x00,0x01,0x02,0x00, 0x00,0x01,0x06,0x00,0x80,0x1A,0x06,0x00,    /* 00000000    "........" */
  // 0x00,0x00,0x0A,0x10,0x53,0x42,0x2E,0x50, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,    /* 00000010    "........" */
  // 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x3C,0x10,0x43,0x58,    /* 00000020    "........" */
  // 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,    /* 00000030    "........" */
  0x8FAA8086, 0x00020100, 0x00060100, 0x00061A80,
  // 00000010
  0x100A0000, 0x502E4253, 0x00000000, 0x00000000,
  // 00000020
  0x00000000, 0x00000000, 0x00000000, 0x5843103C,
  // 00000030
  0x00000000, 0x00000000, 0x00000000, 0x00000000
};

/**
  Node tests.

  @param[in] DtDevice       Device to test.

  @retval TRUE              Success.
  @retval FALSE             Failure.

**/
TEST_DEF (DtTestRoot) {
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  //
  // Check default values as per 2.3.5 of DT spec.
  //
  ASSERT (DtIo->AddressCells == 2);
  ASSERT (DtIo->SizeCells == 1);

  return TRUE;
}

TEST_DEF (G0) {
  EFI_DT_REG          Reg;
  EFI_DT_PROPERTY     Property;
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (
    DtIo->IsCompatible (NULL, "test1_compatible") ==
    EFI_INVALID_PARAMETER
    );
  ASSERT (DtIo->IsCompatible (DtIo, NULL) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->IsCompatible (DtIo, "test1_compatible") == EFI_SUCCESS);
  ASSERT (DtIo->IsCompatible (DtIo, "asldflkasjf") == EFI_NOT_FOUND);
  ASSERT (AsciiStrCmp (DtIo->DeviceType, "") == 0);
  ASSERT (DtIo->DeviceStatus == EFI_DT_STATUS_OKAY);

  ASSERT (
    DtIo->GetProp (NULL, "compatible", &Property) ==
    EFI_INVALID_PARAMETER
    );
  ASSERT (
    DtIo->GetProp (DtIo, NULL, &Property) ==
    EFI_INVALID_PARAMETER
    );
  ASSERT (
    DtIo->GetProp (DtIo, "compatible", NULL) ==
    EFI_INVALID_PARAMETER
    );
  ASSERT (
    DtIo->GetProp (DtIo, "alskdflksmdf", &Property) ==
    EFI_NOT_FOUND
    );
  ASSERT (
    DtIo->GetProp (DtIo, "compatible", &Property) ==
    EFI_SUCCESS
    );
  ASSERT (
    AsciiStrnCmp (
      Property.Begin,
      "test1_compatible",
      Property.End - Property.Begin
      ) == 0
    );

  ASSERT (DtIo->GetReg (NULL, 0, &Reg) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->GetReg (DtIo, 0, NULL) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->GetReg (DtIo, 0, &Reg) == EFI_NOT_FOUND);

  return TRUE;
}

TEST_DEF (G1) {
  EFI_HANDLE          FoundHandle;
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (AsciiStrCmp (DtIo->DeviceType, "bar") == 0);

  ASSERT (DtIo->Lookup (NULL, "/unit-test-devices/G0", FALSE, &FoundHandle) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->Lookup (DtIo, NULL, FALSE, &FoundHandle) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->Lookup (DtIo, "/unit-test-devices/G0", FALSE, NULL) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->Lookup (DtIo, "/unit-test-devices/G0", FALSE, &FoundHandle) == EFI_SUCCESS);
  ASSERT (DtIo->Lookup (DtIo, "/unit-test-devices/somethinginvalid", FALSE, &FoundHandle) == EFI_NOT_FOUND);
  //
  // Should return NOT_FOUND as it's not connected yet.
  //
  ASSERT (DtIo->Lookup (DtIo, "/unit-test-devices/G2/G2P1", FALSE, &FoundHandle) == EFI_NOT_FOUND);

  return TRUE;
}

TEST_DEF (G2) {
  EFI_HANDLE          FoundHandle;
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  //
  // #address-cells and #size-cells apply to children,
  // not the node itself.
  //
  ASSERT (DtIo->AddressCells == 2);
  ASSERT (DtIo->SizeCells == 1);

  ASSERT (DtIo->Lookup (DtIo, "somethingrelativeinvalid", FALSE, &FoundHandle) == EFI_NOT_FOUND);
  ASSERT (DtIo->Lookup (DtIo, "G2P0", FALSE, &FoundHandle) == EFI_SUCCESS);
  ASSERT (DtIo->Lookup (DtIo, "alias-G2P0", FALSE, &FoundHandle) == EFI_SUCCESS);

  return TRUE;
}

STATIC
EFI_STATUS
EFIAPI
TestG2P0ReadChildReg (
  IN     EFI_DT_IO_PROTOCOL        *This,
  IN     EFI_DT_IO_PROTOCOL_WIDTH  Width,
  IN     EFI_DT_REG                *Reg,
  IN     EFI_DT_SIZE               Offset,
  IN     UINTN                     Count,
  IN OUT VOID                      *Buffer
  )
{
  ASSERT (Offset == 0 && Count == 1 && Width == EfiDtIoWidthUint32);

  *(UINT32 *)Buffer = 0xc0ff33c0;
  return EFI_SUCCESS;
}

EFI_DT_IO_PROTOCOL_CB  TestG2P0Callbacks = {
  .ReadChildReg = TestG2P0ReadChildReg,
};

TEST_DEF (G2P0) {
  UINT8               Buffer;
  EFI_DT_REG          Reg;
  EFI_DT_PROPERTY     Property;
  UINT32              U32;
  UINT64              U64;
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ZeroMem (&Property, sizeof (EFI_DT_PROPERTY));
  //
  // These should be inherited from g2.
  //
  ASSERT (DtIo->AddressCells == 4);
  ASSERT (DtIo->SizeCells == 3);

  ASSERT (DtIo->GetProp (DtIo, "reg", &Property) == EFI_SUCCESS);
  ASSERT (DtIo->ParseProp (DtIo, &Property, EFI_DT_VALUE_U32, 0, &U32) == EFI_SUCCESS);
  ASSERT (U32 == 0x1);
  ASSERT (DtIo->ParseProp (DtIo, &Property, EFI_DT_VALUE_U32, 1, &U32) == EFI_SUCCESS);
  ASSERT (U32 == 0x3);
  ASSERT (DtIo->ParseProp (DtIo, &Property, EFI_DT_VALUE_U32, 2, &U32) == EFI_SUCCESS);
  ASSERT (U32 == 0x6);
  Property.Iter = Property.Begin;
  ASSERT (DtIo->ParseProp (DtIo, &Property, EFI_DT_VALUE_U32, 13, &U32) == EFI_SUCCESS);
  ASSERT (U32 == 0x7);
  ASSERT (DtIo->ParseProp (DtIo, &Property, EFI_DT_VALUE_U32, 0, &U32) == EFI_NOT_FOUND);
  Property.Iter = Property.Begin;
  ASSERT (DtIo->ParseProp (DtIo, &Property, EFI_DT_VALUE_U64, 0, &U64) == EFI_SUCCESS);
  ASSERT (U64 == 0x0000000100000002);

  ASSERT (DtIo->GetU32 (DtIo, "reg", 2, &U32) == EFI_SUCCESS);
  ASSERT (U32 == 0x3);
  ASSERT (DtIo->GetU32 (NULL, "reg", 2, &U32) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->GetU32 (DtIo, NULL, 2, &U32) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->GetU32 (DtIo, "reg", 2, NULL) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->GetU32 (DtIo, "reg", 14, &U32) == EFI_NOT_FOUND);
  ASSERT (DtIo->GetU64 (DtIo, "reg", 1, &U64) == EFI_SUCCESS);
  ASSERT (U64 == 0x0000000300000004);
  ASSERT (DtIo->GetU64 (NULL, "reg", 1, &U64) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->GetU64 (DtIo, NULL, 1, &U64) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->GetU64 (DtIo, "reg", 1, NULL) == EFI_INVALID_PARAMETER);

  //
  // Correctly parse 'reg' based on address cells == 4 and
  // size cells == 3.
  //
  ASSERT (!EFI_ERROR (DtIo->GetReg (DtIo, 0, &Reg)));
  ASSERT (Reg.BusDtIo == &(DtDevice->Parent->DtIo));
  ASSERT (
    (Reg.BusBase & (UINT64)-1UL) ==
    0x0000000300000004
    );
  ASSERT (
    ((Reg.BusBase >> 64) & (UINT64)-1UL) ==
    0x0000000100000002
    );
  ASSERT (
    (Reg.Length & (UINT64)-1UL) ==
    0x0000000600000007
    );
  ASSERT (
    ((Reg.Length >> 64) & (UINT64)-1UL) ==
    0x0000000000000005
    );

  ASSERT (!EFI_ERROR (DtIo->GetReg (DtIo, 1, &Reg)));
  ASSERT (Reg.BusDtIo == &(DtDevice->Parent->DtIo));
  ASSERT (
    (Reg.BusBase & (UINT64)-1UL) ==
    0x000000030000000b
    );
  ASSERT (
    ((Reg.BusBase >> 64) & (UINT64)-1UL) ==
    0x000000010000000a
    );
  ASSERT (
    (Reg.Length & (UINT64)-1UL) ==
    0x0000000c00000007
    );
  ASSERT (
    ((Reg.Length >> 64) & (UINT64)-1UL) ==
    0x0000000000000005
    );

  //
  // EFI_UNSUPPORTED because Reg.BusDtIo != NULL and G2 didn't
  // set up the ReadChildReg function in its DtIo protocol.
  //
  ASSERT (
    DtIo->ReadReg (DtIo, EfiDtIoWidthUint8, &Reg, 0, 1, &Buffer) ==
    EFI_UNSUPPORTED
    );
  ASSERT (
    DtIo->ReadReg (DtIo, EfiDtIoWidthMaximum, &Reg, 0, 1, &Buffer) ==
    EFI_INVALID_PARAMETER
    );
  ASSERT (
    DtIo->ReadReg (DtIo, EfiDtIoWidthUint8, NULL, 0, 1, &Buffer) ==
    EFI_INVALID_PARAMETER
    );
  ASSERT (
    DtIo->ReadReg (DtIo, EfiDtIoWidthUint8, &Reg, Reg.Length, 1, &Buffer) ==
    EFI_INVALID_PARAMETER
    );
  ASSERT (
    DtIo->ReadReg (DtIo, EfiDtIoWidthUint8, &Reg, Reg.Length - 1, 2, &Buffer) ==
    EFI_INVALID_PARAMETER
    );
  ASSERT (
    DtIo->ReadReg (DtIo, EfiDtIoWidthUint8, &Reg, 0, 1, NULL) ==
    EFI_INVALID_PARAMETER
    );

  ASSERT (DtIo->GetReg (DtIo, 2, &Reg) == EFI_NOT_FOUND);

  // For TestG2P0C1Fn.
  ASSERT (DtIo->SetCallbacks (DtIo, gDriverBinding.DriverBindingHandle, &TestG2P0Callbacks) == EFI_SUCCESS);

  return TRUE;
}

TEST_DEF (G2P0C1) {
  UINT32              Buffer;
  EFI_DT_REG          Reg;
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  //
  // These should be the defaults, as Parent1 doesn't
  // have own #address-cells and #size-cells and there is
  // no inheritance.
  //
  ASSERT (DtIo->AddressCells == 2);
  ASSERT (DtIo->SizeCells == 1);

  ASSERT (!EFI_ERROR (DtIo->GetReg (DtIo, 0, &Reg)));
  ASSERT (Reg.BusDtIo == &(DtDevice->Parent->DtIo));

  //
  // Invokes TestG2P0ReadChildReg.
  //
  ASSERT (
    DtIo->ReadReg (DtIo, EfiDtIoWidthUint32, &Reg, 0, 1, &Buffer) ==
    EFI_SUCCESS
    );

  ASSERT (Buffer == 0xc0ff33c0);

  return TRUE;
}

TEST_DEF (G2P1) {
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (DtIo->DeviceStatus == EFI_DT_STATUS_BROKEN);
  return TRUE;
}

TEST_DEF (G2P2) {
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (DtIo->DeviceStatus == EFI_DT_STATUS_BROKEN);
  return TRUE;
}

TEST_DEF (G3P0) {
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (DtIo->DeviceStatus == EFI_DT_STATUS_DISABLED);
  return TRUE;
}

TEST_DEF (G3P1) {
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (DtIo->DeviceStatus == EFI_DT_STATUS_RESERVED);
  return TRUE;
}

TEST_DEF (G3P2) {
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (DtIo->DeviceStatus == EFI_DT_STATUS_FAIL);
  return TRUE;
}

TEST_DEF (G3P3) {
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (DtIo->DeviceStatus == EFI_DT_STATUS_FAIL_WITH_CONDITION);
  return TRUE;
}

TEST_DEF (G3P4) {
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (DtIo->DeviceStatus == EFI_DT_STATUS_OKAY);
  return TRUE;
}

TEST_DEF (G3P5) {
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (DtIo->DeviceStatus == EFI_DT_STATUS_BROKEN);
  return TRUE;
}

//
// Tests GetRange.
//
TEST_DEF (G4) {
  EFI_DT_RANGE        Range;
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (DtIo->ChildAddressCells == 3);
  ASSERT (DtIo->AddressCells == 2);
  ASSERT (DtIo->ChildSizeCells == 2);

  ASSERT (DtIo->GetRange (NULL, "ranges", 0, &Range) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->GetRange (DtIo, NULL, 0, &Range) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->GetRange (DtIo, "ranges", 0, NULL) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->GetRange (DtIo, "ranges", 0, &Range) == EFI_SUCCESS);
  ASSERT ((Range.ChildBase & (UINT64)-1UL) == 0x200000003);
  ASSERT (((Range.ChildBase >> 32) & (UINT64)-1UL) == 0x100000002);
  ASSERT (Range.ParentBase == 0x500000006);
  ASSERT (Range.ParentBase == Range.TranslatedParentBase);
  ASSERT (Range.Length == 0x700000008);

  ASSERT (DtIo->GetRange (DtIo, "ranges", 1, &Range) == EFI_SUCCESS);
  ASSERT ((Range.ChildBase & (UINT64)-1UL) == 0xb0000000c);
  ASSERT (((Range.ChildBase >> 32) & (UINT64)-1UL) == 0xa0000000b);
  ASSERT (Range.ParentBase == 0xd0000000e);
  ASSERT (Range.ParentBase == Range.TranslatedParentBase);
  ASSERT (Range.Length == 0xf00000001);

  return TRUE;
}

TEST_DEF (G5) {
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  //
  // #address-cells and #size-cells apply to children,
  // not the node itself.
  //
  ASSERT (DtIo->AddressCells == 2);
  ASSERT (DtIo->SizeCells == 1);
  return TRUE;
}

//
// ReadReg Test.
//
TEST_DEF (G5P0) {
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);
  UINTN               TestRegionSize;
  UINT8               *TempMemBuffer;
  EFI_DT_REG          Reg00;
  EFI_DT_REG          *Reg11;
  UINT8               Array1[16];
  UINT8               Array2[16];

  Reg11          = NULL;
  TestRegionSize = sizeof (Dt_DeviceRegs_TestTemplate00);
  TempMemBuffer  = AllocateZeroPool (TestRegionSize);
  ASSERT (TempMemBuffer != NULL);

  CopyMem (TempMemBuffer, Dt_DeviceRegs_TestTemplate00, TestRegionSize);
  CopyMem (Array2, Dt_DeviceRegs_TestTemplate00, 16);

  ZeroMem (&Reg00, sizeof (EFI_DT_REG));
  Reg00.TranslatedBase = (EFI_PHYSICAL_ADDRESS)TempMemBuffer;
  Reg00.Length         = TestRegionSize;
  ASSERT (DtIo->ReadReg (DtIo, EfiDtIoWidthUint8, &Reg00, 0, 16, Array1) == EFI_SUCCESS);
  ASSERT (CompareMem (Array1, Array2, 16) == 0);
  ASSERT (DtIo->ReadReg (DtIo, EfiDtIoWidthUint16, &Reg00, 0, 8, Array1) == EFI_SUCCESS);
  ASSERT (CompareMem (Array1, Array2, 16) == 0);
  ASSERT (DtIo->ReadReg (DtIo, EfiDtIoWidthUint32, &Reg00, 0, 4, Array1) == EFI_SUCCESS);
  ASSERT (CompareMem (Array1, Array2, 16) == 0);
  ASSERT (DtIo->ReadReg (DtIo, EfiDtIoWidthUint64, &Reg00, 0, 2, Array1) == EFI_SUCCESS);
  ASSERT (CompareMem (Array1, Array2, 16) == 0);

  ASSERT (DtIo->ReadReg (DtIo, EfiDtIoWidthUint8, &Reg00, 0, 256, Array1) == EFI_INVALID_PARAMETER);
  //
  // count==0 will be regarded as a reasonable read request
  //
  ASSERT (DtIo->ReadReg (DtIo, EfiDtIoWidthUint16, &Reg00, 0, 0, Array1) == EFI_SUCCESS);
  ASSERT (DtIo->ReadReg (DtIo, EfiDtIoWidthUint32, &Reg00, 0, 1024, Array1) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->ReadReg (DtIo, EfiDtIoWidthUint64, Reg11, 0, 2, Array1) == EFI_INVALID_PARAMETER);

  FreePool (TempMemBuffer);
  return TRUE;
}

//
// WriteReg Test.
//
TEST_DEF (G5P1) {
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);
  UINTN               TestRegionSize;
  UINT8               *TempMemBuffer;
  EFI_DT_REG          Reg00;
  UINT8               Array1[32];
  UINT8               Array2[32];
  UINTN               Index;

  TestRegionSize = sizeof (Dt_DeviceRegs_TestTemplate00);
  TempMemBuffer  = AllocateZeroPool (TestRegionSize);
  ASSERT (TempMemBuffer != NULL);

  for (Index = 0; Index < 32; Index++) {
    Array1[Index] = Index;
  }

  CopyMem (TempMemBuffer, Dt_DeviceRegs_TestTemplate00, TestRegionSize);
  CopyMem (Array2, Dt_DeviceRegs_TestTemplate00, 16);

  ZeroMem (&Reg00, sizeof (EFI_DT_REG));
  Reg00.TranslatedBase = (EFI_PHYSICAL_ADDRESS)TempMemBuffer;
  Reg00.Length         = TestRegionSize;
  ASSERT (DtIo->WriteReg (DtIo, EfiDtIoWidthUint8, &Reg00, 0x10, 16, Array1) == EFI_SUCCESS);
  ASSERT (DtIo->ReadReg (DtIo, EfiDtIoWidthUint8, &Reg00, 0x10, 16, Array2) == EFI_SUCCESS);
  ASSERT (CompareMem (Array1, Array2, 16) == 0);
  ASSERT (DtIo->WriteReg (DtIo, EfiDtIoWidthUint32, &Reg00, 0x20, 8, Array1) == EFI_SUCCESS);
  ASSERT (DtIo->ReadReg (DtIo, EfiDtIoWidthUint32, &Reg00, 0x20, 8, Array2) == EFI_SUCCESS);
  ASSERT (CompareMem (Array1, Array2, 16) == 0);
  ASSERT (DtIo->WriteReg (DtIo, EfiDtIoWidthUint32, &Reg00, 0x30, 8, Array1) == EFI_INVALID_PARAMETER);

  FreePool (TempMemBuffer);
  return TRUE;
}

//
// PollReg Test.
//
TEST_DEF (G5P2) {
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);
  UINTN               TestRegionSize;
  UINT8               *TempMemBuffer;
  EFI_DT_REG          Reg00;
  UINT64              Indicator;

  TestRegionSize = sizeof (Dt_DeviceRegs_TestTemplate00);
  TempMemBuffer  = AllocateZeroPool (TestRegionSize);
  ASSERT (TempMemBuffer != NULL);

  CopyMem (TempMemBuffer, Dt_DeviceRegs_TestTemplate00, TestRegionSize);
  ZeroMem (&Reg00, sizeof (EFI_DT_REG));
  Reg00.TranslatedBase = (EFI_PHYSICAL_ADDRESS)TempMemBuffer;
  Reg00.Length         = TestRegionSize;

  //
  // offset 0 = 0x86 now.
  //
  ASSERT (DtIo->PollReg (DtIo, EfiDtIoWidthUint8, &Reg00, 0, 0xFF, 0x17, 1000000, &Indicator) == EFI_TIMEOUT);
  ASSERT (DtIo->PollReg (DtIo, EfiDtIoWidthUint8, &Reg00, 0, 0xFF, 0x86, 1000000, &Indicator) == EFI_SUCCESS);

  FreePool (TempMemBuffer);
  return TRUE;
}

//
// CopyReg Test.
//
TEST_DEF (G5P3) {
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);
  UINTN               TestRegionSize;
  UINT8               *TempMemBuffer;
  EFI_DT_REG          Reg00;
  EFI_DT_REG          Reg11;
  UINT8               Array2[32];

  TestRegionSize = sizeof (Dt_DeviceRegs_TestTemplate00);
  TempMemBuffer  = AllocateZeroPool (TestRegionSize);
  ASSERT (TempMemBuffer != NULL);

  CopyMem (TempMemBuffer, Dt_DeviceRegs_TestTemplate00, TestRegionSize);
  ZeroMem (&Reg00, sizeof (EFI_DT_REG));
  Reg00.TranslatedBase = (EFI_PHYSICAL_ADDRESS)TempMemBuffer;
  Reg00.Length         = TestRegionSize;

  ZeroMem (&Reg11, sizeof (EFI_DT_REG));
  Reg11.TranslatedBase = (EFI_DT_BUS_ADDRESS)(UINTN)Array2;
  Reg11.Length         = 32;
  ASSERT (DtIo->CopyReg (DtIo, EfiDtIoWidthUint32, &Reg11, 0, &Reg00, 0, 8) == EFI_SUCCESS);
  ASSERT (CompareMem ((UINT8 *)(UINTN)Reg00.TranslatedBase, (UINT8 *)(UINTN)Reg11.TranslatedBase, 32) == 0);

  FreePool (TempMemBuffer);
  return TRUE;
}

//
// String properties.
//
TEST_DEF (G6) {
  EFI_DT_PROPERTY     Property;
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);
  CONST CHAR8         *String;
  CONST CHAR8         *String2;
  UINTN               Index;

  ZeroMem (&Property, sizeof (EFI_DT_PROPERTY));
  ASSERT (DtIo->GetProp (DtIo, "string", &Property) == EFI_SUCCESS);
  ASSERT (DtIo->ParseProp (DtIo, &Property, EFI_DT_VALUE_STRING, 0, &String) == EFI_SUCCESS);
  ASSERT (AsciiStrCmp (String, "a string") == 0);
  ASSERT (DtIo->ParseProp (DtIo, &Property, EFI_DT_VALUE_STRING, 0, &String) == EFI_NOT_FOUND);
  Index = 1;
  ASSERT (DtIo->GetStringIndex (DtIo, "string", "a string", &Index) == EFI_SUCCESS);
  ASSERT (Index == 0);
  ASSERT (DtIo->GetStringIndex (DtIo, "string", "ya nah", &Index) == EFI_NOT_FOUND);
  ASSERT (DtIo->GetStringIndex (NULL, "string", "ya nah", &Index) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->GetStringIndex (DtIo, NULL, "ya nah", &Index) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->GetStringIndex (DtIo, "string", NULL, &Index) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->GetStringIndex (DtIo, "string", "ya nah", NULL) == EFI_INVALID_PARAMETER);

  ASSERT (DtIo->GetProp (DtIo, "svals1", &Property) == EFI_SUCCESS);
  ASSERT (DtIo->ParseProp (DtIo, &Property, EFI_DT_VALUE_STRING, 0, &String) == EFI_SUCCESS);
  ASSERT (AsciiStrCmp (String, "string1") == 0);
  ASSERT (DtIo->ParseProp (DtIo, &Property, EFI_DT_VALUE_STRING, 0, &String) == EFI_SUCCESS);
  ASSERT (AsciiStrCmp (String, "string2") == 0);
  ASSERT (DtIo->ParseProp (DtIo, &Property, EFI_DT_VALUE_STRING, 0, &String) == EFI_NOT_FOUND);
  Property.Iter = Property.Begin;
  ASSERT (DtIo->ParseProp (DtIo, &Property, EFI_DT_VALUE_STRING, 1, &String) == EFI_SUCCESS);
  ASSERT (AsciiStrCmp (String, "string2") == 0);
  Property.Iter = Property.Begin;
  ASSERT (DtIo->ParseProp (DtIo, &Property, EFI_DT_VALUE_STRING, 2, &String) == EFI_NOT_FOUND);
  ASSERT (DtIo->GetStringIndex (DtIo, "svals1", "string3", &Index) == EFI_NOT_FOUND);
  ASSERT (DtIo->GetStringIndex (DtIo, "svals1", "string2", &Index) == EFI_SUCCESS);
  ASSERT (Index == 1);
  ASSERT (DtIo->GetStringIndex (DtIo, "svals1", "string1", &Index) == EFI_SUCCESS);
  ASSERT (Index == 0);

  ASSERT (DtIo->GetString (DtIo, "svals1", 0, &String) == EFI_SUCCESS);
  ASSERT (AsciiStrCmp (String, "string1") == 0);
  ASSERT (DtIo->GetString (DtIo, "svals1", 100, &String) == EFI_NOT_FOUND);
  ASSERT (DtIo->GetString (NULL, "svals1", 0, &String) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->GetString (DtIo, NULL, 0, &String) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->GetString (DtIo, "svals1", 0, NULL) == EFI_INVALID_PARAMETER);

  ASSERT (DtIo->GetProp (DtIo, "empty", &Property) == EFI_SUCCESS);
  ASSERT (DtIo->ParseProp (DtIo, &Property, EFI_DT_VALUE_STRING, 0, &String) == EFI_NOT_FOUND);
  ASSERT (DtIo->ParseProp (DtIo, &Property, EFI_DT_VALUE_STRING, 0, &String) == EFI_NOT_FOUND);
  ASSERT (DtIo->ParseProp (DtIo, &Property, EFI_DT_VALUE_STRING, 100, &String) == EFI_NOT_FOUND);
  ASSERT (DtIo->GetStringIndex (DtIo, "empty", "", &Index) == EFI_NOT_FOUND);
  ASSERT (DtIo->GetStringIndex (DtIo, "empty", "something else", &Index) == EFI_NOT_FOUND);

  ASSERT (DtIo->GetProp (DtIo, "svals2", &Property) == EFI_SUCCESS);
  ASSERT (DtIo->ParseProp (DtIo, &Property, EFI_DT_VALUE_STRING, 0, &String) == EFI_SUCCESS);
  ASSERT (DtIo->ParseProp (DtIo, &Property, EFI_DT_VALUE_STRING, 0, &String2) == EFI_SUCCESS);
  ASSERT (AsciiStrCmp (String, "") == 0);
  ASSERT (AsciiStrCmp (String2, "") == 0);
  ASSERT (String2 > String);
  ASSERT (DtIo->ParseProp (DtIo, &Property, EFI_DT_VALUE_STRING, 0, &String2) == EFI_SUCCESS);
  ASSERT (AsciiStrCmp (String2, "1") == 0);
  ASSERT (DtIo->ParseProp (DtIo, &Property, EFI_DT_VALUE_STRING, 0, &String2) == EFI_SUCCESS);
  ASSERT (AsciiStrCmp (String2, "") == 0);
  ASSERT (String2 > String);
  ASSERT (DtIo->ParseProp (DtIo, &Property, EFI_DT_VALUE_STRING, 0, &String2) == EFI_NOT_FOUND);
  ASSERT (DtIo->GetStringIndex (DtIo, "svals2", "", &Index) == EFI_SUCCESS);
  ASSERT (Index == 0);
  ASSERT (DtIo->GetStringIndex (DtIo, "svals2", "1", &Index) == EFI_SUCCESS);
  ASSERT (Index == 2);

  return TRUE;
}

//
// GetRegByName.
//
TEST_DEF (G7P0) {
  EFI_DT_REG          Reg;
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (DtIo->GetRegByName (NULL, "apple", &Reg) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->GetRegByName (DtIo, NULL, &Reg) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->GetRegByName (DtIo, "apple", NULL) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->GetRegByName (DtIo, "apple", &Reg) == EFI_SUCCESS);
  ASSERT (((UINT64)Reg.BusBase == 0x100000002) && ((UINT64)Reg.Length == 0x300000004));

  ASSERT (DtIo->GetRegByName (DtIo, "orange", &Reg) == EFI_SUCCESS);
  ASSERT (((UINT64)Reg.BusBase == 0x90000000A) && ((UINT64)Reg.Length == 0xB0000000C));

  ASSERT (DtIo->GetRegByName (DtIo, "peach", &Reg) == EFI_SUCCESS);
  ASSERT (((UINT64)Reg.BusBase == 0x1200000013) && ((UINT64)Reg.Length == 0x1400000015));

  ASSERT (DtIo->GetRegByName (DtIo, "gsdfsdfds", &Reg) == EFI_NOT_FOUND);
  ASSERT (DtIo->GetRegByName (DtIo, "", &Reg) == EFI_NOT_FOUND);

  return TRUE;
}

//
// DMA-related tests.
//
TEST_DEF (Dma0) {
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (!DtIo->IsDmaCoherent);

  return TRUE;
}

TEST_DEF (Dma1) {
  UINTN                         Index;
  VOID                          *TestAddress;
  VOID                          *TestAddress2;
  EFI_DT_BUS_ADDRESS            BusAddress;
  VOID                          *Mapping;
  UINTN                         NumberOfBytes;
  EFI_DT_IO_PROTOCOL_DMA_EXTRA  Constraints;
  EFI_DT_IO_PROTOCOL            *DtIo = &(DtDevice->DtIo);

  ASSERT (DtIo->IsDmaCoherent);

  TestAddress   = (VOID *)(UINTN)0x1337;
  NumberOfBytes = 4;

  //
  // Basic map/unmap.
  //
  ASSERT (DtIo->Unmap (NULL, NULL) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->Unmap (DtIo, NULL) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->Unmap (DtIo, (VOID *)(UINTN)0xabcd) == EFI_INVALID_PARAMETER);

  ASSERT (
    DtIo->Map (
            NULL,
            EfiDtIoDmaOperationMaximum,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL
            ) == EFI_INVALID_PARAMETER
    );

  ASSERT (
    DtIo->Map (
            DtIo,
            EfiDtIoDmaOperationMaximum,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL
            ) == EFI_INVALID_PARAMETER
    );

  ASSERT (
    DtIo->Map (
            DtIo,
            EfiDtIoDmaOperationBusMasterRead,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL
            ) == EFI_INVALID_PARAMETER
    );

  ASSERT (
    DtIo->Map (
            DtIo,
            EfiDtIoDmaOperationBusMasterRead,
            TestAddress,
            NULL,
            NULL,
            NULL,
            NULL
            ) == EFI_INVALID_PARAMETER
    );

  ASSERT (
    DtIo->Map (
            DtIo,
            EfiDtIoDmaOperationBusMasterRead,
            TestAddress,
            NULL,
            &NumberOfBytes,
            NULL,
            NULL
            ) == EFI_INVALID_PARAMETER
    );

  ASSERT (
    DtIo->Map (
            DtIo,
            EfiDtIoDmaOperationBusMasterRead,
            TestAddress,
            NULL,
            &NumberOfBytes,
            &BusAddress,
            NULL
            ) == EFI_INVALID_PARAMETER
    );

  ASSERT (
    DtIo->Map (
            DtIo,
            EfiDtIoDmaOperationBusMasterRead,
            TestAddress,
            NULL,
            &NumberOfBytes,
            &BusAddress,
            &Mapping
            ) == EFI_SUCCESS
    );
  ASSERT ((UINTN)TestAddress == BusAddress);
  ASSERT (DtIo->Unmap (DtIo, Mapping) == EFI_SUCCESS);

  ASSERT (
    DtIo->Map (
            DtIo,
            EfiDtIoDmaOperationBusMasterWrite,
            TestAddress,
            NULL,
            &NumberOfBytes,
            &BusAddress,
            &Mapping
            ) == EFI_SUCCESS
    );
  ASSERT ((UINTN)TestAddress == BusAddress);
  ASSERT (DtIo->Unmap (DtIo, Mapping) == EFI_SUCCESS);

  ASSERT (
    DtIo->Map (
            DtIo,
            EfiDtIoDmaOperationBusMasterCommonBuffer,
            TestAddress,
            NULL,
            &NumberOfBytes,
            &BusAddress,
            &Mapping
            ) == EFI_SUCCESS
    );
  ASSERT ((UINTN)TestAddress == BusAddress);
  ASSERT (DtIo->Unmap (DtIo, Mapping) == EFI_SUCCESS);

  //
  // Map constraints.
  //
  Constraints.Flags = -1;
  ASSERT (
    DtIo->Map (
            DtIo,
            EfiDtIoDmaOperationBusMasterCommonBuffer,
            TestAddress,
            &Constraints,
            &NumberOfBytes,
            &BusAddress,
            &Mapping
            ) == EFI_INVALID_PARAMETER
    );
  Constraints.Flags = EFI_DT_IO_DMA_NON_COHERENT;
  ASSERT (
    DtIo->Map (
            DtIo,
            EfiDtIoDmaOperationBusMasterCommonBuffer,
            TestAddress,
            &Constraints,
            &NumberOfBytes,
            &BusAddress,
            &Mapping
            ) == EFI_UNSUPPORTED
    );
  Constraints.Flags = 0;
  ASSERT (
    DtIo->Map (
            DtIo,
            EfiDtIoDmaOperationBusMasterCommonBuffer,
            TestAddress,
            &Constraints,
            &NumberOfBytes,
            &BusAddress,
            &Mapping
            ) == EFI_SUCCESS
    );
  ASSERT ((UINTN)TestAddress == BusAddress);
  ASSERT (DtIo->Unmap (DtIo, Mapping) == EFI_SUCCESS);

  //
  // Test for bounce buffering.
  //
  TestAddress = AllocatePages (1);
  ASSERT (TestAddress != NULL);
  NumberOfBytes = EFI_PAGE_SIZE;
  SetMem (TestAddress, NumberOfBytes, 0xAA);
  Constraints.Flags      = EFI_DT_IO_DMA_WITH_MAX_ADDRESS;
  Constraints.MaxAddress = (EFI_PHYSICAL_ADDRESS)TestAddress;
  ASSERT (
    DtIo->Map (
            DtIo,
            EfiDtIoDmaOperationBusMasterCommonBuffer,
            TestAddress,
            &Constraints,
            &NumberOfBytes,
            &BusAddress,
            &Mapping
            ) == EFI_INVALID_PARAMETER
    );
  ASSERT (
    DtIo->Map (
            DtIo,
            EfiDtIoDmaOperationBusMasterRead,
            TestAddress,
            &Constraints,
            &NumberOfBytes,
            &BusAddress,
            &Mapping
            ) == EFI_SUCCESS
    );
  ASSERT (Mapping != NO_MAPPING);
  ASSERT (BusAddress < Constraints.MaxAddress);
  ASSERT (CompareMem (TestAddress, (VOID *)(UINTN)BusAddress, NumberOfBytes) == 0);
  ASSERT (DtIo->Unmap (DtIo, Mapping) == EFI_SUCCESS);
  ASSERT (
    DtIo->Map (
            DtIo,
            EfiDtIoDmaOperationBusMasterWrite,
            TestAddress,
            &Constraints,
            &NumberOfBytes,
            &BusAddress,
            &Mapping
            ) == EFI_SUCCESS
    );
  ASSERT (Mapping != NO_MAPPING);
  ASSERT (BusAddress < Constraints.MaxAddress);
  SetMem ((VOID *)(UINTN)BusAddress, NumberOfBytes, 0xBB);
  ASSERT (DtIo->Unmap (DtIo, Mapping) == EFI_SUCCESS);
  for (Index = 0; Index < EFI_PAGE_SIZE; Index++) {
    ASSERT (*((CHAR8 *)TestAddress + Index) == 0xBB);
  }

  FreePages (TestAddress, 1);

  //
  // Test for AllocateBuffer.
  //
  ASSERT (DtIo->AllocateBuffer (NULL, EfiMaxMemoryType, 0, NULL, NULL) == EFI_INVALID_PARAMETER);

  ASSERT (DtIo->AllocateBuffer (DtIo, EfiMaxMemoryType, 0, NULL, NULL) == EFI_INVALID_PARAMETER);

  ASSERT (DtIo->AllocateBuffer (DtIo, EfiBootServicesData, 0, NULL, &TestAddress) == EFI_INVALID_PARAMETER);

  ASSERT (DtIo->AllocateBuffer (DtIo, EfiBootServicesData, 1, NULL, &TestAddress) == EFI_SUCCESS);

  ASSERT (DtIo->FreeBuffer (NULL, 0, NULL) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->FreeBuffer (DtIo, 0, NULL) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->FreeBuffer (DtIo, 0, TestAddress) == EFI_INVALID_PARAMETER);
  ASSERT (DtIo->FreeBuffer (DtIo, 1, NULL) == EFI_NOT_FOUND);
  ASSERT (DtIo->FreeBuffer (DtIo, 1, TestAddress) == EFI_SUCCESS);

  //
  // Allocation constraints.
  //
  Constraints.Flags = -1;
  ASSERT (DtIo->AllocateBuffer (DtIo, EfiRuntimeServicesData, 1, &Constraints, &TestAddress) == EFI_INVALID_PARAMETER);
  Constraints.Flags = EFI_DT_IO_DMA_NON_COHERENT;
  ASSERT (DtIo->AllocateBuffer (DtIo, EfiRuntimeServicesData, 1, &Constraints, &TestAddress) == EFI_UNSUPPORTED);
  Constraints.Flags = 0;
  ASSERT (DtIo->AllocateBuffer (DtIo, EfiRuntimeServicesData, 1, &Constraints, &TestAddress) == EFI_SUCCESS);
  Constraints.Flags      = EFI_DT_IO_DMA_WITH_MAX_ADDRESS;
  Constraints.MaxAddress = (EFI_PHYSICAL_ADDRESS)TestAddress;
  ASSERT (DtIo->AllocateBuffer (DtIo, EfiRuntimeServicesData, 1, &Constraints, &TestAddress2) == EFI_SUCCESS);
  ASSERT (TestAddress2 < TestAddress);
  ASSERT (DtIo->FreeBuffer (DtIo, 1, TestAddress) == EFI_SUCCESS);
  ASSERT (DtIo->FreeBuffer (DtIo, 1, TestAddress2) == EFI_SUCCESS);

  return TRUE;
}

TEST_DEF (Dma2) {
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (!DtIo->IsDmaCoherent);

  return TRUE;
}

STATIC TestDesc  TestDescs[] = {
  TEST_DECL (DtTestRoot),
  TEST_DECL (G0),
  TEST_DECL (G1),
  TEST_DECL (G2),
  TEST_DECL (G2P0),
  TEST_DECL (G2P0C1),
  TEST_DECL (G2P1),
  TEST_DECL (G2P2),
  TEST_DECL (G3P0),
  TEST_DECL (G3P1),
  TEST_DECL (G3P2),
  TEST_DECL (G3P3),
  TEST_DECL (G3P4),
  TEST_DECL (G3P5),
  TEST_DECL (G4),
  TEST_DECL (G5),
  TEST_DECL (G5P0),
  TEST_DECL (G5P1),
  TEST_DECL (G5P2),
  TEST_DECL (G5P3),
  TEST_DECL (G6),
  TEST_DECL (G7P0),
  TEST_DECL (Dma0),
  TEST_DECL (Dma1),
  TEST_DECL (Dma2),
};

/**
  Invoke a DT node test, if present.

  @param[in] DtDevice       Node to test.

  @retval None

**/
VOID
TestsInvoke (
  IN  DT_DEVICE  *DtDevice
  )
{
  UINTN  Index;

  if ((DtDevice->Flags & DT_DEVICE_TEST_UNIT_RAN) != 0) {
    //
    // Only run tests once.
    //
    return;
  }

  for (Index = 0; Index < ARRAY_SIZE (TestDescs); Index++) {
    TestDesc  *Test = &TestDescs[Index];
    if (AsciiStrCmp (DtDevice->DtIo.Name, Test->Name) == 0) {
      DEBUG ((DEBUG_ERROR, "%a: running unit test\n", DtDevice->DtIo.Name));
      ASSERT (Test->Fn (DtDevice));
      DtDevice->Flags |= DT_DEVICE_TEST_UNIT_RAN;
      break;
    }
  }
}

/**
  Initialize the unit tests (which use their own Devicetree).

  @retval EFI_SUCCESS       Success.
  @retval other             Some error occured.

**/
EFI_STATUS
TestsInit (
  VOID
  )
{
  if (fdt_check_header (TestDt_dtb) != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: test DTB @ %p seems corrupted?\n",
      __func__,
      TestDt_dtb
      ));
    return EFI_NOT_FOUND;
  }

  gTestTreeBase = TestDt_dtb;

  return EFI_SUCCESS;
}

/**
  Cleanup the unit tests (which use their own Devicetree).

  @retval None.

**/
VOID
TestsCleanup (
  VOID
  )
{
  gTestTreeBase = NULL;
}

#endif /* MDEPKG_NDEBUG */
