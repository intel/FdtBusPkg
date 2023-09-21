/** @file

    Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "FdtBusDxe.h"

#ifndef MDEPKG_NDEBUG

#define TEST_DT_SIZE  4096

//
// Encode the pointer as a 32-bit offset to not have to worry
// when we go 128-bit.
//

#define NODE_TEST(Buffer, Test, ...)  do {                              \
    if (Test != NULL) {                                                 \
      fdt_property_u32 (Buffer, "uefi,FdtBusDxe-Test", (UINTN) Test - (UINTN) TestsInit); \
    }                                                                   \
  } while (0)

#define BEGIN_NODE(Buffer, Name)  do {          \
    INTN  FdtErr;                               \
                                                \
    FdtErr = fdt_begin_node (Buffer, #Name);    \
    if (FdtErr < 0) {                           \
      DEBUG ((                                  \
               DEBUG_ERROR,                     \
               "%a: fdt_begin_node(%a): %a\n",  \
               __func__,                        \
               #Name,                           \
               fdt_strerror (FdtErr)            \
               ));                              \
      return FdtErr;                            \
    }                                           \
  } while (0)

#define END_NODE(Buffer, Name)  do {            \
    INTN  FdtErr;                               \
                                                \
    FdtErr =  fdt_end_node (Buffer);            \
    if (FdtErr < 0) {                           \
      DEBUG ((                                  \
               DEBUG_ERROR,                     \
               "%a: fdt_end_node(%a): %a\n",    \
               __func__,                        \
               #Name,                           \
               fdt_strerror (FdtErr)            \
               ));                              \
      return FdtErr;                            \
    }                                           \
  } while (0)

#define NEW_NODE(Buffer, Name, Test, ...)  do {                         \
    BEGIN_NODE (Buffer, Name);                                          \
    NODE_TEST (Buffer, Test);                                           \
    __VA_ARGS__;                                                        \
    END_NODE (Buffer, Name);                                            \
  } while (0)

VOID  *gTestTreeBase;

UINT32 Dt_DeviceRegs_TestTemplate00 [] = {
  //0x86,0x80,0xAA,0x8F,0x00,0x01,0x02,0x00, 0x00,0x01,0x06,0x00,0x80,0x1A,0x06,0x00,    /* 00000000    "........" */
  //0x00,0x00,0x0A,0x10,0x53,0x42,0x2E,0x50, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,    /* 00000010    "........" */
  //0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x3C,0x10,0x43,0x58,    /* 00000020    "........" */
  //0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,    /* 00000030    "........" */
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
STATIC
BOOLEAN
EFIAPI
RootTestFn (
  IN  DT_DEVICE  *DtDevice
  )
{
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  //
  // Check default values as per 2.3.5 of DT spec.
  //
  ASSERT (DtIo->AddressCells == 2);
  ASSERT (DtIo->SizeCells == 1);

  return TRUE;
}

STATIC
BOOLEAN
EFIAPI
TestG0Fn (
  IN  DT_DEVICE  *DtDevice
  )
{
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
  ASSERT (AsciiStrCmp (DtIo->Model, "") == 0);
  ASSERT (AsciiStrCmp (DtIo->DeviceType, "") == 0);
  ASSERT (DtIo->DeviceStatus == EFI_DT_STATUS_OKAY);
  ASSERT (!DtIo->IsDmaCoherent);

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

STATIC
BOOLEAN
EFIAPI
TestG1Fn (
  IN  DT_DEVICE  *DtDevice
  )
{
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (DtIo->IsDmaCoherent);
  ASSERT (AsciiStrCmp (DtIo->Model, "foo") == 0);
  ASSERT (AsciiStrCmp (DtIo->DeviceType, "bar") == 0);
  return TRUE;
}

STATIC
BOOLEAN
EFIAPI
TestG2Fn (
  IN  DT_DEVICE  *DtDevice
  )
{
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  //
  // #address-cells and #size-cells apply to children,
  // not the node itself.
  //
  ASSERT (DtIo->AddressCells == 2);
  ASSERT (DtIo->SizeCells == 1);
  return TRUE;
}

STATIC
BOOLEAN
EFIAPI
TestG2P0Fn (
  IN  DT_DEVICE  *DtDevice
  )
{
  UINT8               Buffer;
  EFI_DT_REG          Reg;
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  //
  // These should be inherited from g2.
  //
  ASSERT (DtIo->AddressCells == 4);
  ASSERT (DtIo->SizeCells == 3);

  //
  // Correctly parse 'reg' based on address cells == 4 and
  // size cells == 3.
  //
  ASSERT (!EFI_ERROR (DtIo->GetReg (DtIo, 0, &Reg)));
  ASSERT (Reg.BusDtIo == &(DtDevice->Parent->DtIo));
  ASSERT (
    (Reg.Base & (UINTN)-1UL) ==
    0x0000000300000004
    );
  ASSERT (
    ((Reg.Base >> 64) & (UINTN)-1UL) ==
    0x0000000100000002
    );
  ASSERT (
    (Reg.Length & (UINTN)-1UL) ==
    0x0000000600000007
    );
  ASSERT (
    ((Reg.Length >> 64) & (UINTN)-1UL) ==
    0x0000000000000005
    );

  //
  // EFI_UNSUPPORTED because Reg.BusDtIo != NULL and G2 didn't
  // override the ReadReg function in its DtIo protocol.
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

  ASSERT (DtIo->GetReg (DtIo, 1, &Reg) == EFI_NOT_FOUND);

  return TRUE;
}

STATIC
BOOLEAN
EFIAPI
TestG2P0C1Fn  (
  IN  DT_DEVICE  *DtDevice
  )
{
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  //
  // These should be the defaults, as Parent1 doesn't
  // have own #address-cells and #size-cells and there is
  // no inheritance.
  //
  ASSERT (DtIo->AddressCells == 2);
  ASSERT (DtIo->SizeCells == 1);
  return TRUE;
}

STATIC
BOOLEAN
EFIAPI
TestG2P1Fn (
  IN  DT_DEVICE  *DtDevice
  )
{
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (DtIo->DeviceStatus == EFI_DT_STATUS_BROKEN);
  return TRUE;
}

STATIC
BOOLEAN
EFIAPI
TestG2P2Fn (
  IN  DT_DEVICE  *DtDevice
  )
{
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (DtIo->DeviceStatus == EFI_DT_STATUS_BROKEN);
  return TRUE;
}

STATIC
BOOLEAN
EFIAPI
TestG3P0Fn (
  IN  DT_DEVICE  *DtDevice
  )
{
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (DtIo->DeviceStatus == EFI_DT_STATUS_DISABLED);
  return TRUE;
}

STATIC
BOOLEAN
EFIAPI
TestG3P1Fn (
  IN  DT_DEVICE  *DtDevice
  )
{
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (DtIo->DeviceStatus == EFI_DT_STATUS_RESERVED);
  return TRUE;
}

STATIC
BOOLEAN
EFIAPI
TestG3P2Fn (
  IN  DT_DEVICE  *DtDevice
  )
{
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (DtIo->DeviceStatus == EFI_DT_STATUS_FAIL);
  return TRUE;
}

STATIC
BOOLEAN
EFIAPI
TestG3P3Fn (
  IN  DT_DEVICE  *DtDevice
  )
{
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (DtIo->DeviceStatus == EFI_DT_STATUS_FAIL_WITH_CONDITION);
  return TRUE;
}

STATIC
BOOLEAN
EFIAPI
TestG3P4Fn (
  IN  DT_DEVICE  *DtDevice
  )
{
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (DtIo->DeviceStatus == EFI_DT_STATUS_OKAY);
  return TRUE;
}

STATIC
BOOLEAN
EFIAPI
TestG3P5Fn (
  IN  DT_DEVICE  *DtDevice
  )
{
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);

  ASSERT (DtIo->DeviceStatus == EFI_DT_STATUS_BROKEN);
  return TRUE;
}

STATIC
BOOLEAN
EFIAPI
TestG5Fn (
  IN  DT_DEVICE  *DtDevice
  )
{
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);
  //
  // #address-cells and #size-cells apply to children,
  // not the node itself.
  //
  ASSERT (DtIo->AddressCells == 2);
  ASSERT (DtIo->SizeCells == 1);
  return TRUE;
}

STATIC
BOOLEAN
EFIAPI
TestG5P0Fn (
  IN  DT_DEVICE  *DtDevice
  )
{
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);
  UINTN               TestRegionSize;
  UINT8               *TempMemBuffer;
  EFI_DT_REG          Reg00;
  EFI_DT_REG          *Reg11;
  UINT8               Array1[16];
  UINT8               Array2[16];

  Reg11 = NULL;
  TestRegionSize = sizeof (Dt_DeviceRegs_TestTemplate00);
  TempMemBuffer  = AllocateZeroPool (TestRegionSize);
  if (TempMemBuffer == NULL) {
    DEBUG ((DEBUG_ERROR,"Warning: Run out memory. \n"));
    goto ErrorExit;
  }

  CopyMem (TempMemBuffer, Dt_DeviceRegs_TestTemplate00, TestRegionSize);
  CopyMem (Array2, Dt_DeviceRegs_TestTemplate00, 16);

  ZeroMem (&Reg00, sizeof (EFI_DT_REG));
  Reg00.Base   = (EFI_PHYSICAL_ADDRESS)TempMemBuffer;
  Reg00.Length = TestRegionSize;
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

  DEBUG ((DEBUG_INFO,"%a: the test for ReadReg is passed. \n", __func__));
  FreePool (TempMemBuffer);
  return TRUE;

ErrorExit:
  DEBUG ((DEBUG_ERROR,"%a: does not accomplish the test.  \n", __func__));
  if (TempMemBuffer != NULL) {
    FreePool (TempMemBuffer);
  }

  return 0;
}

STATIC
BOOLEAN
EFIAPI
TestG5P2Fn (
  IN  DT_DEVICE  *DtDevice
  )
{
  EFI_DT_IO_PROTOCOL  *DtIo = &(DtDevice->DtIo);
  UINTN               TestRegionSize;
  UINT8               *TempMemBuffer;
  EFI_DT_REG          Reg00;
  UINT64              Indicator;

  TestRegionSize = sizeof (Dt_DeviceRegs_TestTemplate00);
  TempMemBuffer  = AllocateZeroPool (TestRegionSize);
  if (TempMemBuffer == NULL) {
    DEBUG ((DEBUG_ERROR,"Warning: Run out memory. \n"));
    goto ErrorExit;
  }

  CopyMem (TempMemBuffer, Dt_DeviceRegs_TestTemplate00, TestRegionSize);
  ZeroMem (&Reg00, sizeof (EFI_DT_REG));
  Reg00.Base   = (EFI_PHYSICAL_ADDRESS)TempMemBuffer;
  Reg00.Length = TestRegionSize;

  //
  // offset 0 = 0x86 now.
  //
  ASSERT (DtIo->PollReg (DtIo, EfiDtIoWidthUint8, &Reg00, 0, 0xFF, 0x17, 1000000, &Indicator) == EFI_TIMEOUT);
  ASSERT (DtIo->PollReg (DtIo, EfiDtIoWidthUint8, &Reg00, 0, 0xFF, 0x86, 1000000, &Indicator) == EFI_SUCCESS);

  DEBUG ((DEBUG_INFO,"%a: the test for PollReg is passed. \n", __func__));
  FreePool (TempMemBuffer);
  return TRUE;

ErrorExit:
  DEBUG ((DEBUG_ERROR,"%a: does not accomplish the test.  \n", __func__));
  if (TempMemBuffer != NULL) {
    FreePool (TempMemBuffer);
  }

  return 0;
}

/**
  Populate test DT nodes.

  @param[in] Buffer         Test DT to populate.

  @retval 0                 Success.
  @retval other             Some libfdt error occured.

**/
STATIC
INTN
TestsPopulate (
  VOID  *Buffer
  )
{
  UINT32  G2P0RegProp[] = {
    cpu_to_fdt32 (0x1),
    cpu_to_fdt32 (0x2),
    cpu_to_fdt32 (0x3),
    cpu_to_fdt32 (0x4),
    cpu_to_fdt32 (0x5),
    cpu_to_fdt32 (0x6),
    cpu_to_fdt32 (0x7)
  };

  //
  // Test handling for default #address-cells and #size-cells.
  //

  NODE_TEST (Buffer, RootTestFn);

  //
  // Test compatible, model, status, and dma-coherent properties,
  // GetProp, IsCompatible, GetReg.
  //

  NEW_NODE (
    Buffer,
    g0,
    TestG0Fn,
    fdt_property_string (Buffer, "compatible", "test1_compatible")
    );

  //
  // Test model, dma-coherent property.
  //

  NEW_NODE (
    Buffer,
    g1,
    TestG1Fn,
    fdt_property (Buffer, "dma-coherent", NULL, 0),
    fdt_property_string (Buffer, "model", "foo"),
    fdt_property_string (Buffer, "device_type", "bar")
    );

  //
  // More tests for #address-cells and #size-cells, reg/GetReg.
  //

  BEGIN_NODE (Buffer, g2);
  NODE_TEST (Buffer, TestG2Fn);
  fdt_property_u32 (Buffer, "#address-cells", 4);
  fdt_property_u32 (Buffer, "#size-cells", 3);
  BEGIN_NODE (Buffer, g2p0);
  NODE_TEST (Buffer, TestG2P0Fn);
  fdt_property (Buffer, "reg", G2P0RegProp, sizeof (G2P0RegProp));
  BEGIN_NODE (Buffer, g2p0c1);
  NODE_TEST (Buffer, TestG2P0C1Fn);
  END_NODE (Buffer, g2p0c1);
  END_NODE (Buffer, g2p0);
  NEW_NODE (
    Buffer,
    g2p1,
    TestG2P1Fn,
    fdt_property_u32 (Buffer, "#address-cells", 5),
    fdt_property_u32 (Buffer, "#size-cells", 2)
    );
  NEW_NODE (
    Buffer,
    g2p2,
    TestG2P2Fn,
    fdt_property_u32 (Buffer, "#address-cells", 2),
    fdt_property_u32 (Buffer, "#size-cells", 5)
    );
  END_NODE (Buffer, g2);

  //
  // More tests for device_status.
  //
  BEGIN_NODE (Buffer, g3);
  NEW_NODE (
    Buffer,
    g3p0,
    TestG3P0Fn,
    fdt_property_string (Buffer, "status", "disabled")
    );
  NEW_NODE (
    Buffer,
    g3p1,
    TestG3P1Fn,
    fdt_property_string (Buffer, "status", "reserved")
    );
  NEW_NODE (
    Buffer,
    g3p2,
    TestG3P2Fn,
    fdt_property_string (Buffer, "status", "fail")
    );
  NEW_NODE (
    Buffer,
    g3p3,
    TestG3P3Fn,
    fdt_property_string (Buffer, "status", "fail-foo")
    );
  NEW_NODE (
    Buffer,
    g3p4,
    TestG3P4Fn,
    fdt_property_string (Buffer, "status", "okay")
    );
  NEW_NODE (
    Buffer,
    g3p5,
    TestG3P5Fn,
    fdt_property_string (Buffer, "status", "lkalksjdlkajsd")
    );
  END_NODE (Buffer, g3);

  BEGIN_NODE (Buffer, g5);
  NODE_TEST (Buffer, TestG5Fn);
  fdt_property_u32 (Buffer, "#address-cells", 3);
  fdt_property_u32 (Buffer, "#size-cells", 2);
  NEW_NODE (
    Buffer,
    g5p0,
    TestG5P0Fn
    );
  NEW_NODE (
    Buffer,
    g5p2,
    TestG5P2Fn
    );
  END_NODE (Buffer, g5);

  return 0;
}

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
  CONST VOID      *Buf;
  INT32           Len;
  BOOLEAN EFIAPI  (*Fn)(
    DT_DEVICE  *DtDevice
    );

  if ((DtDevice->Flags & DT_DEVICE_TEST_RAN) != 0) {
    //
    // Only run tests once.
    //
    return;
  }

  Buf = fdt_getprop (gTestTreeBase, DtDevice->FdtNode, "uefi,FdtBusDxe-Test", &Len);
  if (Buf == NULL) {
    ASSERT (Len == -FDT_ERR_NOTFOUND);
    return;
  }

  ASSERT (Len == sizeof (UINT32));
  Fn = (VOID *)((INT32)fdt32_to_cpu (*((UINT32 *)Buf)) + (UINTN)TestsInit);
  DEBUG ((DEBUG_ERROR, "%a: running unit test\n", DtDevice->DtIo.Name));
  ASSERT (Fn (DtDevice));

  DtDevice->Flags |= DT_DEVICE_TEST_RAN;
}

/**
  Initialize the unit tests (which use their own device tree).

  @retval EFI_SUCCESS       Success.
  @retval other             Some error occured.

**/
EFI_STATUS
TestsInit (
  VOID
  )
{
  VOID  *Buffer;
  INTN  FdtErr;

  Buffer = AllocateZeroPool (TEST_DT_SIZE);
  if (Buffer == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: AllocateZeroPool\n", __func__));
    return EFI_OUT_OF_RESOURCES;
  }

  FdtErr = fdt_create (Buffer, TEST_DT_SIZE);
  if (FdtErr < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: fdt_create: %a\n",
      __func__,
      fdt_strerror (FdtErr)
      ));
    goto done;
  }

  FdtErr = fdt_finish_reservemap (Buffer);
  if (FdtErr < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: fdt_finish_reservemap: %a\n",
      __func__,
      fdt_strerror (FdtErr)
      ));
    goto done;
  }

  FdtErr = fdt_begin_node (Buffer, "");
  if (FdtErr < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: fdt_begin_node: %a\n",
      __func__,
      fdt_strerror (FdtErr)
      ));
    goto done;
  }

  FdtErr = TestsPopulate (Buffer);
  if (FdtErr < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: TestsPopulate: %a\n",
      __func__,
      fdt_strerror (FdtErr)
      ));
    goto done;
  }

  FdtErr =  fdt_end_node (Buffer);
  if (FdtErr < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: fdt_end_node: %a\n",
      __func__,
      fdt_strerror (FdtErr)
      ));
    goto done;
  }

  FdtErr = fdt_finish (Buffer);
  if (FdtErr < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: fdt_finish: %a\n",
      __func__,
      fdt_strerror (FdtErr)
      ));
    goto done;
  }

  FdtErr = fdt_open_into (Buffer, Buffer, TEST_DT_SIZE);
  if (FdtErr < 0) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: fdt_open_into: %a\n",
      __func__,
      fdt_strerror (FdtErr)
      ));
    goto done;
  }

  gTestTreeBase = Buffer;
done:
  if (FdtErr < 0) {
    FreePool (Buffer);
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  Cleanup the unit tests (which use their own device tree).

  @retval None.

**/
VOID
TestsCleanup (
  VOID
  )
{
  FreePool (gTestTreeBase);
  gTestTreeBase = NULL;
}

#endif /* MDEPKG_NDEBUG */
