/** @file
    DT-based PCI(e) host bridge driver.

    Copyright (c) 2024, Intel Corporation. All rights reserved.<BR>

    SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "Driver.h"

#define RANGE_VALID(Range)  ((Range).Length != 0)
#define NO_MAPPING  (VOID *) (UINTN) -1

typedef enum {
  IoOperation,
  MemOperation,
  MemOperationNoBuffer,
  PciOperation
} OPERATION_TYPE;

//
// Can pass EfiPciIoWdith* directly to DT I/O Protocol.
//
#define _(x)  (UINTN)(EfiDtIoWidth##x) == (UINTN)(EfiPciIoWidth##x), #x
STATIC_ASSERT (_ (Uint8));
STATIC_ASSERT (_ (Uint16));
STATIC_ASSERT (_ (Uint32));
STATIC_ASSERT (_ (Uint64));
STATIC_ASSERT (_ (FifoUint8));
STATIC_ASSERT (_ (FifoUint16));
STATIC_ASSERT (_ (FifoUint32));
STATIC_ASSERT (_ (FifoUint64));
STATIC_ASSERT (_ (FillUint8));
STATIC_ASSERT (_ (FillUint16));
STATIC_ASSERT (_ (FillUint32));
STATIC_ASSERT (_ (FillUint64));
STATIC_ASSERT (_ (Maximum));
#undef _

/**
  Check parameters for IO,MMIO,PCI read/write services of PCI Root Bridge IO.

  The I/O operations are carried out exactly as requested. The caller is
  responsible for satisfying any alignment and I/O width restrictions that a PI
  System on a platform might require. For example on some platforms, width
  requests of EfiCpuIoWidthUint64 do not work. Misaligned buffers, on the other
  hand, will be handled by the driver.

  @param[in]  This           A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.

  @param[in]  OperationType  I/O operation type: IO/MMIO/PCI.

  @param[in]  Width          Signifies the width of the I/O or Memory operation.

  @param[in]  Address        The base address of the I/O operation.

  @param[in]  Count          The number of I/O operations to perform. The number
                             of bytes moved is Width size * Count, starting at
                             Address.

  @param[in]  Buffer         For read operations, the destination buffer to
                             store the results. For write operations, the source
                             buffer from which to write data.

  @param[out] Reg            EFI_DT_REG to use for DT I/O accesses.

  @retval EFI_SUCCESS            The parameters for this request pass the
                                 checks.

  @retval EFI_INVALID_PARAMETER  Width is invalid for this PI system.

  @retval EFI_INVALID_PARAMETER  Buffer is NULL.

  @retval EFI_INVALID_PARAMETER  Address or Count is invalid.

  @retval EFI_UNSUPPORTED        The Buffer is not aligned for the given Width.

  @retval EFI_UNSUPPORTED        The address range specified by Address, Width,
                                 and Count is not valid for this PI system.
**/
EFI_STATUS
RootBridgeIoCheckParameter (
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL        *This,
  IN  OPERATION_TYPE                         OperationType,
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH  Width,
  IN  UINT64                                 Address,
  IN  UINTN                                  Count,
  IN  VOID                                   *Buffer,
  OUT EFI_DT_REG                             *Reg
  )
{
  PCI_ROOT_BRIDGE_INSTANCE                     *RootBridge;
  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_PCI_ADDRESS  *PciRbAddr;
  UINT64                                       Base;
  UINT64                                       Limit;
  UINT32                                       Size;
  UINT64                                       Length;

  if ((Reg == NULL) || (Width >= EfiPciWidthMaximum)) {
    return EFI_INVALID_PARAMETER;
  }

  if (OperationType != MemOperationNoBuffer) {
    if (Buffer == NULL) {
      return EFI_INVALID_PARAMETER;
    }
  }

  //
  // For FIFO type, the device address won't increase during the access,
  // so treat Count as 1
  //
  if ((Width >= EfiPciWidthFifoUint8) && (Width <= EfiPciWidthFifoUint64)) {
    Count = 1;
  }

  Width = (EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH)(Width & 0x03);
  Size  = 1 << Width;

  //
  // Make sure (Count * Size) doesn't exceed MAX_UINT64
  //
  if (Count > DivU64x32 (MAX_UINT64, Size)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Check to see if Address is aligned
  //
  if ((Address & (Size - 1)) != 0) {
    return EFI_UNSUPPORTED;
  }

  //
  // Make sure (Address + Count * Size) doesn't exceed MAX_UINT64
  //
  Length = MultU64x32 (Count, Size);
  if (Address > MAX_UINT64 - Length) {
    return EFI_INVALID_PARAMETER;
  }

  RootBridge = ROOT_BRIDGE_FROM_THIS (This);

  //
  // Check to see if any address associated with this transfer exceeds the
  // maximum allowed address.  The maximum address implied by the parameters
  // passed in is Address + Size * Count.  If the following condition is met,
  // then the transfer is not supported.
  //
  //    Address + Size * Count > Limit + 1
  //
  // Since Limit can be the maximum integer value supported by the CPU and
  // Count can also be the maximum integer value supported by the CPU, this
  // range check must be adjusted to avoid all oveflow conditions.
  //
  if (OperationType == IoOperation) {
    Base  = RB (RootBridge->IoRange);
    Limit = RL (RootBridge->IoRange);

    FbpRangeToReg (&RootBridge->IoRange, TRUE, Reg);
  } else if ((OperationType == MemOperation) || (OperationType == MemOperationNoBuffer)) {
    EFI_DT_RANGE  *Range;

    //
    // By comparing the Address against Limit we know which range to be used
    // for checking
    //
    if ((Address >= RB (RootBridge->MemRange)) &&
        (Address + Length <= RL (RootBridge->MemRange) + 1))
    {
      Base  = RB (RootBridge->MemRange);
      Limit = RL (RootBridge->MemRange);
      Range = &RootBridge->MemRange;
    } else if ((Address >= RB (RootBridge->PMemRange)) &&
               (Address + Length <= RL (RootBridge->PMemRange) + 1))
    {
      Base  = RB (RootBridge->PMemRange);
      Limit = RL (RootBridge->PMemRange);
      Range = &RootBridge->PMemRange;
    } else if ((Address >= RB (RootBridge->MemAbove4GRange)) &&
               (Address + Length <= RL (RootBridge->MemAbove4GRange) + 1))
    {
      Base  = RB (RootBridge->MemAbove4GRange);
      Limit = RL (RootBridge->MemAbove4GRange);
      Range = &RootBridge->MemAbove4GRange;
    } else {
      Base  = RB (RootBridge->PMemAbove4GRange);
      Limit = RL (RootBridge->PMemAbove4GRange);
      Range = &RootBridge->PMemAbove4GRange;
    }

    FbpRangeToReg (Range, TRUE, Reg);
  } else {
    PciRbAddr = (EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_PCI_ADDRESS *)&Address;
    if ((PciRbAddr->Bus < RB (RootBridge->BusRange)) ||
        (PciRbAddr->Bus > RL (RootBridge->BusRange)))
    {
      return EFI_INVALID_PARAMETER;
    }

    if ((PciRbAddr->Device > PCI_MAX_DEVICE) ||
        (PciRbAddr->Function > PCI_MAX_FUNC))
    {
      return EFI_INVALID_PARAMETER;
    }

    if (PciRbAddr->ExtendedRegister != 0) {
      Address = PciRbAddr->ExtendedRegister;
    } else {
      Address = PciRbAddr->Register;
    }

    Base  = 0;
    Limit = RootBridge->NoExtendedConfigSpace ? 0xFF : 0xFFF;
    *Reg  = RootBridge->ConfigReg;
  }

  if (Address < Base) {
    return EFI_INVALID_PARAMETER;
  }

  if (Address + Length > Limit + 1) {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

/**
  This routine gets translation offset from a root bridge instance by resource type.

  @param RootBridge The Root Bridge Instance for the resources.
  @param ResourceType The Resource Type of the translation offset.

  @retval The Translation Offset of the specified resource.
**/
UINT64
GetTranslationByResourceType (
  IN  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge,
  IN  PCI_RESOURCE_TYPE         ResourceType
  )
{
  switch (ResourceType) {
    case TypeIo:
      return RT (RootBridge->IoRange);
    case TypeMem32:
      return RT (RootBridge->MemRange);
    case TypePMem32:
      return RT (RootBridge->PMemRange);
    case TypeMem64:
      return RT (RootBridge->MemAbove4GRange);
    case TypePMem64:
      return RT (RootBridge->PMemAbove4GRange);
    case TypeBus:
      return RT (RootBridge->BusRange);
    default:
      ASSERT (FALSE);
      return 0;
  }
}

/**
  Add IO space to GCD.

  Only adds the space if it doesn't exist already.

  @param Range         EFI_DT_RANGE.

  @retval EFI_SUCCESS  The IO space was added successfully.
**/
STATIC
EFI_STATUS
AddIoSpace (
  IN  EFI_DT_RANGE  *Range
  )
{
  EFI_STATUS                   Status;
  EFI_PHYSICAL_ADDRESS         HostAddress;
  UINTN                        Length;
  EFI_GCD_IO_SPACE_DESCRIPTOR  GcdDescriptor;

  //
  // Base and Limit in PCI_ROOT_BRIDGE_APERTURE are device address.
  // For GCD resource manipulation, we need to use host address.
  //
  HostAddress = TO_HOST_ADDRESS (RB (*Range), RT (*Range));
  Length      = RS (*Range);

  Status = gDS->GetIoSpaceDescriptor (
                  HostAddress,
                  &GcdDescriptor
                  );
  if (EFI_ERROR (Status)) {
    //
    // This can happen if HostAddress exceeds the Gcd limits as set
    // by the Cpu HOB. You'll get an EFI_NOT_FOUND and it's really
    // a programmer error since the HostAddress is clearly invalid.
    //
    DEBUG ((
      DEBUG_ERROR,
      "%a: GetIoSpaceDescriptor(0x%lx-0x%lx): %r\n",
      __func__,
      HostAddress,
      HostAddress + Length - 1,
      Status
      ));
    if (Status == EFI_NOT_FOUND) {
      Status = EFI_INVALID_PARAMETER;
    }

    return Status;
  }

  if ((HostAddress + Length - 1) >
      (GcdDescriptor.BaseAddress + GcdDescriptor.Length - 1))
  {
    DEBUG ((
      DEBUG_ERROR,
      "%a: [0x%lx, 0x%lx) straddles multiple GCD entries\n",
      __func__,
      HostAddress,
      HostAddress + Length - 1
      ));
    Status = EFI_ACCESS_DENIED;
    return Status;
  }

  if (GcdDescriptor.GcdIoType == EfiGcdIoTypeIo) {
    //
    // Already present.
    //
    return EFI_SUCCESS;
  }

  return gDS->AddIoSpace (EfiGcdIoTypeIo, HostAddress, Length);
}

/**
  Polls an address in memory mapped I/O space until an exit condition is met,
  or a timeout occurs.

  This function provides a standard way to poll a PCI memory location. A PCI
  memory read operation is performed at the PCI memory address specified by
  Address for the width specified by Width. The result of this PCI memory read
  operation is stored in Result. This PCI memory read operation is repeated
  until either a timeout of Delay 100 ns units has expired, or (Result & Mask)
  is equal to Value.

  @param[in]   This      A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param[in]   Width     Signifies the width of the memory operations.
  @param[in]   Address   The base address of the memory operations. The caller
                         is responsible for aligning Address if required.
  @param[in]   Mask      Mask used for the polling criteria. Bytes above Width
                         in Mask are ignored. The bits in the bytes below Width
                         which are zero in Mask are ignored when polling the
                         memory address.
  @param[in]   Value     The comparison value used for the polling exit
                         criteria.
  @param[in]   Delay     The number of 100 ns units to poll. Note that timer
                         available may be of poorer granularity.
  @param[out]  Result    Pointer to the last value read from the memory
                         location.

  @retval EFI_SUCCESS            The last data returned from the access matched
                                 the poll exit criteria.
  @retval EFI_INVALID_PARAMETER  Width is invalid.
  @retval EFI_INVALID_PARAMETER  Result is NULL.
  @retval EFI_TIMEOUT            Delay expired before a match occurred.
  @retval EFI_OUT_OF_RESOURCES   The request could not be completed due to a
                                 lack of resources.
**/
STATIC
EFI_STATUS
EFIAPI
RootBridgeIoPollMem (
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL        *This,
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH  Width,
  IN  UINT64                                 Address,
  IN  UINT64                                 Mask,
  IN  UINT64                                 Value,
  IN  UINT64                                 Delay,
  OUT UINT64                                 *Result
  )
{
  EFI_STATUS                Status;
  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge;
  EFI_DT_REG                Reg;

  Status = RootBridgeIoCheckParameter (
             This,
             MemOperation,
             Width,
             Address,
             1,
             Result,
             &Reg
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  RootBridge = ROOT_BRIDGE_FROM_THIS (This);
  return RootBridge->DtIo->PollReg (
                             RootBridge->DtIo,
                             Width,
                             &Reg,
                             Address,
                             Mask,
                             Value,
                             Delay,
                             Result
                             );
}

/**
  Reads from the I/O space of a PCI Root Bridge. Returns when either the
  polling exit criteria is satisfied or after a defined duration.

  This function provides a standard way to poll a PCI I/O location. A PCI I/O
  read operation is performed at the PCI I/O address specified by Address for
  the width specified by Width.
  The result of this PCI I/O read operation is stored in Result. This PCI I/O
  read operation is repeated until either a timeout of Delay 100 ns units has
  expired, or (Result & Mask) is equal to Value.

  @param[in] This      A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param[in] Width     Signifies the width of the I/O operations.
  @param[in] Address   The base address of the I/O operations. The caller is
                       responsible for aligning Address if required.
  @param[in] Mask      Mask used for the polling criteria. Bytes above Width in
                       Mask are ignored. The bits in the bytes below Width
                       which are zero in Mask are ignored when polling the I/O
                       address.
  @param[in] Value     The comparison value used for the polling exit criteria.
  @param[in] Delay     The number of 100 ns units to poll. Note that timer
                       available may be of poorer granularity.
  @param[out] Result   Pointer to the last value read from the memory location.

  @retval EFI_SUCCESS            The last data returned from the access matched
                                 the poll exit criteria.
  @retval EFI_INVALID_PARAMETER  Width is invalid.
  @retval EFI_INVALID_PARAMETER  Result is NULL.
  @retval EFI_TIMEOUT            Delay expired before a match occurred.
  @retval EFI_OUT_OF_RESOURCES   The request could not be completed due to a
                                 lack of resources.
**/
STATIC
EFI_STATUS
EFIAPI
RootBridgeIoPollIo (
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL        *This,
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH  Width,
  IN  UINT64                                 Address,
  IN  UINT64                                 Mask,
  IN  UINT64                                 Value,
  IN  UINT64                                 Delay,
  OUT UINT64                                 *Result
  )
{
  EFI_STATUS                Status;
  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge;
  EFI_DT_REG                Reg;

  Status = RootBridgeIoCheckParameter (
             This,
             IoOperation,
             Width,
             Address,
             1,
             Result,
             &Reg
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  RootBridge = ROOT_BRIDGE_FROM_THIS (This);
  return RootBridge->DtIo->PollReg (
                             RootBridge->DtIo,
                             Width,
                             &Reg,
                             Address,
                             Mask,
                             Value,
                             Delay,
                             Result
                             );
}

/**
  Enables a PCI driver to access PCI controller registers in the PCI root
  bridge memory space.

  The Mem.Read(), and Mem.Write() functions enable a driver to access PCI
  controller registers in the PCI root bridge memory space.
  The memory operations are carried out exactly as requested. The caller is
  responsible for satisfying any alignment and memory width restrictions that a
  PCI Root Bridge on a platform might require.

  @param[in]   This      A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param[in]   Width     Signifies the width of the memory operation.
  @param[in]   Address   The base address of the memory operation. The caller
                         is responsible for aligning the Address if required.
  @param[in]   Count     The number of memory operations to perform. Bytes
                         moved is Width size * Count, starting at Address.
  @param[out]  Buffer    For read operations, the destination buffer to store
                         the results. For write operations, the source buffer
                         to write data from.

  @retval EFI_SUCCESS            The data was read from or written to the PCI
                                 root bridge.
  @retval EFI_INVALID_PARAMETER  Width is invalid for this PCI root bridge.
  @retval EFI_INVALID_PARAMETER  Buffer is NULL.
  @retval EFI_OUT_OF_RESOURCES   The request could not be completed due to a
                                 lack of resources.
**/
STATIC
EFI_STATUS
EFIAPI
RootBridgeIoMemRead (
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL        *This,
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH  Width,
  IN     UINT64                                 Address,
  IN     UINTN                                  Count,
  OUT    VOID                                   *Buffer
  )
{
  EFI_STATUS                Status;
  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge;
  EFI_DT_REG                Reg;

  Status = RootBridgeIoCheckParameter (
             This,
             MemOperation,
             Width,
             Address,
             Count,
             Buffer,
             &Reg
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  RootBridge = ROOT_BRIDGE_FROM_THIS (This);
  return RootBridge->DtIo->ReadReg (
                             RootBridge->DtIo,
                             Width,
                             &Reg,
                             Address,
                             Count,
                             Buffer
                             );
}

/**
  Enables a PCI driver to access PCI controller registers in the PCI root
  bridge memory space.

  The Mem.Read(), and Mem.Write() functions enable a driver to access PCI
  controller registers in the PCI root bridge memory space.
  The memory operations are carried out exactly as requested. The caller is
  responsible for satisfying any alignment and memory width restrictions that a
  PCI Root Bridge on a platform might require.

  @param[in]   This      A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param[in]   Width     Signifies the width of the memory operation.
  @param[in]   Address   The base address of the memory operation. The caller
                         is responsible for aligning the Address if required.
  @param[in]   Count     The number of memory operations to perform. Bytes
                         moved is Width size * Count, starting at Address.
  @param[in]   Buffer    For read operations, the destination buffer to store
                         the results. For write operations, the source buffer
                         to write data from.

  @retval EFI_SUCCESS            The data was read from or written to the PCI
                                 root bridge.
  @retval EFI_INVALID_PARAMETER  Width is invalid for this PCI root bridge.
  @retval EFI_INVALID_PARAMETER  Buffer is NULL.
  @retval EFI_OUT_OF_RESOURCES   The request could not be completed due to a
                                 lack of resources.
**/
STATIC
EFI_STATUS
EFIAPI
RootBridgeIoMemWrite (
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL        *This,
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH  Width,
  IN     UINT64                                 Address,
  IN     UINTN                                  Count,
  IN     VOID                                   *Buffer
  )
{
  EFI_STATUS                Status;
  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge;
  EFI_DT_REG                Reg;

  Status = RootBridgeIoCheckParameter (
             This,
             MemOperation,
             Width,
             Address,
             Count,
             Buffer,
             &Reg
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  RootBridge = ROOT_BRIDGE_FROM_THIS (This);
  return RootBridge->DtIo->WriteReg (
                             RootBridge->DtIo,
                             Width,
                             &Reg,
                             Address,
                             Count,
                             Buffer
                             );
}

/**
  Enables a PCI driver to access PCI controller registers in the PCI root
  bridge I/O space.

  @param[in]   This        A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param[in]   Width       Signifies the width of the memory operations.
  @param[in]   Address     The base address of the I/O operation. The caller is
                           responsible for aligning the Address if required.
  @param[in]   Count       The number of I/O operations to perform. Bytes moved
                           is Width size * Count, starting at Address.
  @param[out]  Buffer      For read operations, the destination buffer to store
                           the results. For write operations, the source buffer
                           to write data from.

  @retval EFI_SUCCESS              The data was read from or written to the PCI
                                   root bridge.
  @retval EFI_INVALID_PARAMETER    Width is invalid for this PCI root bridge.
  @retval EFI_INVALID_PARAMETER    Buffer is NULL.
  @retval EFI_OUT_OF_RESOURCES     The request could not be completed due to a
                                   lack of resources.
**/
STATIC
EFI_STATUS
EFIAPI
RootBridgeIoIoRead (
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL        *This,
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH  Width,
  IN     UINT64                                 Address,
  IN     UINTN                                  Count,
  OUT    VOID                                   *Buffer
  )
{
  EFI_STATUS                Status;
  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge;
  EFI_DT_REG                Reg;

  Status = RootBridgeIoCheckParameter (
             This,
             IoOperation,
             Width,
             Address,
             Count,
             Buffer,
             &Reg
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  RootBridge = ROOT_BRIDGE_FROM_THIS (This);
  return RootBridge->DtIo->ReadReg (
                             RootBridge->DtIo,
                             Width,
                             &Reg,
                             Address,
                             Count,
                             Buffer
                             );
}

/**
  Enables a PCI driver to access PCI controller registers in the PCI root
  bridge I/O space.

  @param[in]   This        A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param[in]   Width       Signifies the width of the memory operations.
  @param[in]   Address     The base address of the I/O operation. The caller is
                           responsible for aligning the Address if required.
  @param[in]   Count       The number of I/O operations to perform. Bytes moved
                           is Width size * Count, starting at Address.
  @param[in]   Buffer      For read operations, the destination buffer to store
                           the results. For write operations, the source buffer
                           to write data from.

  @retval EFI_SUCCESS              The data was read from or written to the PCI
                                   root bridge.
  @retval EFI_INVALID_PARAMETER    Width is invalid for this PCI root bridge.
  @retval EFI_INVALID_PARAMETER    Buffer is NULL.
  @retval EFI_OUT_OF_RESOURCES     The request could not be completed due to a
                                   lack of resources.
**/
STATIC
EFI_STATUS
EFIAPI
RootBridgeIoIoWrite (
  IN       EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL        *This,
  IN       EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH  Width,
  IN       UINT64                                 Address,
  IN       UINTN                                  Count,
  IN       VOID                                   *Buffer
  )
{
  EFI_STATUS                Status;
  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge;
  EFI_DT_REG                Reg;

  Status = RootBridgeIoCheckParameter (
             This,
             IoOperation,
             Width,
             Address,
             Count,
             Buffer,
             &Reg
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  RootBridge = ROOT_BRIDGE_FROM_THIS (This);
  return RootBridge->DtIo->WriteReg (
                             RootBridge->DtIo,
                             Width,
                             &Reg,
                             Address,
                             Count,
                             Buffer
                             );
}

/**
  Enables a PCI driver to copy one region of PCI root bridge memory space to
  another region of PCI root bridge memory space.

  The CopyMem() function enables a PCI driver to copy one region of PCI root
  bridge memory space to another region of PCI root bridge memory space. This
  is especially useful for video scroll operation on a memory mapped video
  buffer.
  The memory operations are carried out exactly as requested. The caller is
  responsible for satisfying any alignment and memory width restrictions that a
  PCI root bridge on a platform might require.

  @param[in] This        A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL
                         instance.
  @param[in] Width       Signifies the width of the memory operations.
  @param[in] DestAddress The destination address of the memory operation. The
                         caller is responsible for aligning the DestAddress if
                         required.
  @param[in] SrcAddress  The source address of the memory operation. The caller
                         is responsible for aligning the SrcAddress if
                         required.
  @param[in] Count       The number of memory operations to perform. Bytes
                         moved is Width size * Count, starting at DestAddress
                         and SrcAddress.

  @retval  EFI_SUCCESS             The data was copied from one memory region
                                   to another memory region.
  @retval  EFI_INVALID_PARAMETER   Width is invalid for this PCI root bridge.
  @retval  EFI_OUT_OF_RESOURCES    The request could not be completed due to a
                                   lack of resources.
**/
STATIC
EFI_STATUS
EFIAPI
RootBridgeIoCopyMem (
  IN EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL        *This,
  IN EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH  Width,
  IN UINT64                                 DestAddress,
  IN UINT64                                 SrcAddress,
  IN UINTN                                  Count
  )
{
  EFI_STATUS                Status;
  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge;
  EFI_DT_REG                SrcReg;
  EFI_DT_REG                DestReg;

  Status = RootBridgeIoCheckParameter (
             This,
             MemOperationNoBuffer,
             Width,
             SrcAddress,
             Count,
             NULL,
             &SrcReg
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = RootBridgeIoCheckParameter (
             This,
             MemOperationNoBuffer,
             Width,
             DestAddress,
             Count,
             NULL,
             &DestReg
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  RootBridge = ROOT_BRIDGE_FROM_THIS (This);
  return RootBridge->DtIo->CopyReg (
                             RootBridge->DtIo,
                             Width,
                             &DestReg,
                             DestAddress,
                             &SrcReg,
                             SrcAddress,
                             Count
                             );
}

/**
  PCI configuration space access.

  @param This     A pointer to EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL
  @param Read     TRUE indicating it's a read operation.
  @param Width    Signifies the width of the memory operation.
  @param Address  The address within the PCI configuration space
                  for the PCI controller.
  @param Count    The number of PCI configuration operations
                  to perform.
  @param Buffer   The destination buffer to store the results.

  @retval EFI_SUCCESS            The data was read/written from/to the PCI root bridge.
  @retval EFI_INVALID_PARAMETER  Invalid parameters found.
**/
STATIC
EFI_STATUS
EFIAPI
RootBridgeIoPciAccess (
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL        *This,
  IN     BOOLEAN                                Read,
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH  Width,
  IN     UINT64                                 Address,
  IN     UINTN                                  Count,
  IN OUT VOID                                   *Buffer
  )
{
  EFI_STATUS                                   Status;
  PCI_ROOT_BRIDGE_INSTANCE                     *RootBridge;
  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_PCI_ADDRESS  PciAddress;
  EFI_DT_REG                                   Reg;

  Status = RootBridgeIoCheckParameter (
             This,
             PciOperation,
             Width,
             Address,
             Count,
             Buffer,
             &Reg
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  RootBridge = ROOT_BRIDGE_FROM_THIS (This);
  CopyMem (&PciAddress, &Address, sizeof (PciAddress));

  if (PciAddress.ExtendedRegister == 0) {
    PciAddress.ExtendedRegister = PciAddress.Register;
  }

  if (Read) {
    return RootBridge->DtIo->ReadReg (
                               RootBridge->DtIo,
                               Width,
                               &Reg,
                               PCI_ECAM_ADDRESS (
                                 PciAddress.Bus,
                                 PciAddress.Device,
                                 PciAddress.Function,
                                 PciAddress.ExtendedRegister
                                 ),
                               Count,
                               Buffer
                               );
  } else {
    return RootBridge->DtIo->WriteReg (
                               RootBridge->DtIo,
                               Width,
                               &Reg,
                               PCI_ECAM_ADDRESS (
                                 PciAddress.Bus,
                                 PciAddress.Device,
                                 PciAddress.Function,
                                 PciAddress.ExtendedRegister
                                 ),
                               Count,
                               Buffer
                               );
  }
}

/**
  Allows read from PCI configuration space.

  @param This     A pointer to EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL
  @param Width    Signifies the width of the memory operation.
  @param Address  The address within the PCI configuration space
                  for the PCI controller.
  @param Count    The number of PCI configuration operations
                  to perform.
  @param Buffer   The destination buffer to store the results.

  @retval EFI_SUCCESS           The data was read from the PCI root bridge.
  @retval EFI_INVALID_PARAMETER Invalid parameters found.
**/
STATIC
EFI_STATUS
EFIAPI
RootBridgeIoPciRead (
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL        *This,
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH  Width,
  IN     UINT64                                 Address,
  IN     UINTN                                  Count,
  IN OUT VOID                                   *Buffer
  )
{
  return RootBridgeIoPciAccess (This, TRUE, Width, Address, Count, Buffer);
}

/**
  Allows write to PCI configuration space.

  @param This     A pointer to EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL
  @param Width    Signifies the width of the memory operation.
  @param Address  The address within the PCI configuration space
                  for the PCI controller.
  @param Count    The number of PCI configuration operations
                  to perform.
  @param Buffer   The source buffer to get the results.

  @retval EFI_SUCCESS            The data was written to the PCI root bridge.
  @retval EFI_INVALID_PARAMETER  Invalid parameters found.
**/
STATIC
EFI_STATUS
EFIAPI
RootBridgeIoPciWrite (
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL        *This,
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_WIDTH  Width,
  IN     UINT64                                 Address,
  IN     UINTN                                  Count,
  IN OUT VOID                                   *Buffer
  )
{
  return RootBridgeIoPciAccess (This, FALSE, Width, Address, Count, Buffer);
}

/**
  Provides the PCI controller-specific address needed to access
  system memory for DMA.

  @param This           A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param Operation      Indicate if the bus master is going to read or write
                        to system memory.
  @param HostAddress    The system memory address to map on the PCI controller.
  @param NumberOfBytes  On input the number of bytes to map.
                        On output the number of bytes that were mapped.
  @param DeviceAddress  The resulting map address for the bus master PCI
                        controller to use to access the system memory's HostAddress.
  @param Mapping        The value to pass to Unmap() when the bus master DMA
                        operation is complete.

  @retval EFI_SUCCESS            Success.
  @retval EFI_INVALID_PARAMETER  Invalid parameters found.
  @retval EFI_UNSUPPORTED        The HostAddress cannot be mapped as a common buffer.
  @retval EFI_DEVICE_ERROR       The System hardware could not map the requested address.
  @retval EFI_OUT_OF_RESOURCES   The request could not be completed due to lack of resources.
**/
STATIC
EFI_STATUS
EFIAPI
RootBridgeIoMap (
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL            *This,
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL_OPERATION  Operation,
  IN     VOID                                       *HostAddress,
  IN OUT UINTN                                      *NumberOfBytes,
  OUT    EFI_PHYSICAL_ADDRESS                       *DeviceAddress,
  OUT    VOID                                       **Mapping
  )
{
  EFI_STATUS                Status;
  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge;
  EFI_PHYSICAL_ADDRESS      PhysicalAddress;
  MAP_INFO                  *MapInfo;

  if ((HostAddress == NULL) || (NumberOfBytes == NULL) || (DeviceAddress == NULL) ||
      (Mapping == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Make sure that Operation is valid
  //
  if ((UINT32)Operation >= EfiPciOperationMaximum) {
    return EFI_INVALID_PARAMETER;
  }

  RootBridge = ROOT_BRIDGE_FROM_THIS (This);

  PhysicalAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)HostAddress;
  if ((!RootBridge->DmaAbove4G ||
       ((Operation != EfiPciOperationBusMasterRead64) &&
        (Operation != EfiPciOperationBusMasterWrite64) &&
        (Operation != EfiPciOperationBusMasterCommonBuffer64))) &&
      ((PhysicalAddress + *NumberOfBytes) > SIZE_4GB))
  {
    //
    // If the root bridge or the device cannot handle performing DMA above
    // 4GB but any part of the DMA transfer being mapped is above 4GB, then
    // map the DMA transfer to a buffer below 4GB.
    //

    if ((Operation == EfiPciOperationBusMasterCommonBuffer) ||
        (Operation == EfiPciOperationBusMasterCommonBuffer64))
    {
      //
      // Common Buffer operations can not be remapped.  If the common buffer
      // if above 4GB, then it is not possible to generate a mapping, so return
      // an error.
      //
      return EFI_UNSUPPORTED;
    }

    //
    // Allocate a MAP_INFO structure to remember the mapping when Unmap() is
    // called later.
    //
    MapInfo = AllocatePool (sizeof (MAP_INFO));
    if (MapInfo == NULL) {
      *NumberOfBytes = 0;
      return EFI_OUT_OF_RESOURCES;
    }

    //
    // Initialize the MAP_INFO structure
    //
    MapInfo->Signature         = MAP_INFO_SIGNATURE;
    MapInfo->Operation         = Operation;
    MapInfo->NumberOfBytes     = *NumberOfBytes;
    MapInfo->NumberOfPages     = EFI_SIZE_TO_PAGES (MapInfo->NumberOfBytes);
    MapInfo->HostAddress       = PhysicalAddress;
    MapInfo->MappedHostAddress = SIZE_4GB - 1;

    //
    // Allocate a buffer below 4GB to map the transfer to.
    //
    Status = gBS->AllocatePages (
                    AllocateMaxAddress,
                    EfiBootServicesData,
                    MapInfo->NumberOfPages,
                    &MapInfo->MappedHostAddress
                    );
    if (EFI_ERROR (Status)) {
      FreePool (MapInfo);
      *NumberOfBytes = 0;
      return Status;
    }

    //
    // If this is a read operation from the Bus Master's point of view,
    // then copy the contents of the real buffer into the mapped buffer
    // so the Bus Master can read the contents of the real buffer.
    //
    if ((Operation == EfiPciOperationBusMasterRead) ||
        (Operation == EfiPciOperationBusMasterRead64))
    {
      CopyMem (
        (VOID *)(UINTN)MapInfo->MappedHostAddress,
        (VOID *)(UINTN)MapInfo->HostAddress,
        MapInfo->NumberOfBytes
        );
    }

    InsertTailList (&RootBridge->Maps, &MapInfo->Link);

    //
    // The DeviceAddress is the address of the maped buffer below 4GB
    //
    *DeviceAddress = MapInfo->MappedHostAddress;
    //
    // Return a pointer to the MAP_INFO structure in Mapping
    //
    *Mapping = MapInfo;
  } else {
    //
    // If the root bridge CAN handle performing DMA above 4GB or
    // the transfer is below 4GB, so the DeviceAddress is simply the
    // HostAddress
    //
    *DeviceAddress = PhysicalAddress;
    *Mapping       = NO_MAPPING;
  }

  return EFI_SUCCESS;
}

/**
  Completes the Map() operation and releases any corresponding resources.

  The Unmap() function completes the Map() operation and releases any
  corresponding resources.
  If the operation was an EfiPciOperationBusMasterWrite or
  EfiPciOperationBusMasterWrite64, the data is committed to the target system
  memory.
  Any resources used for the mapping are freed.

  @param[in] This      A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param[in] Mapping   The mapping value returned from Map().

  @retval EFI_SUCCESS            The range was unmapped.
  @retval EFI_INVALID_PARAMETER  Mapping is not a value that was returned by Map().
  @retval EFI_DEVICE_ERROR       The data was not committed to the target system memory.
**/
STATIC
EFI_STATUS
EFIAPI
RootBridgeIoUnmap (
  IN EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL  *This,
  IN VOID                             *Mapping
  )
{
  MAP_INFO                  *MapInfo;
  LIST_ENTRY                *Link;
  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge;

  RootBridge = ROOT_BRIDGE_FROM_THIS (This);

  //
  // See if the Map() operation associated with this Unmap() required a mapping
  // buffer. If a mapping buffer was not required, then this function simply
  // returns EFI_SUCCESS.
  //
  if (Mapping == NO_MAPPING) {
    return EFI_SUCCESS;
  }

  MapInfo = NO_MAPPING;
  for (Link = GetFirstNode (&RootBridge->Maps)
       ; !IsNull (&RootBridge->Maps, Link)
       ; Link = GetNextNode (&RootBridge->Maps, Link)
       )
  {
    MapInfo = MAP_INFO_FROM_LINK (Link);
    if (MapInfo == Mapping) {
      break;
    }
  }

  //
  // Mapping is not a valid value returned by Map().
  //
  if (MapInfo != Mapping) {
    return EFI_INVALID_PARAMETER;
  }

  RemoveEntryList (&MapInfo->Link);

  //
  // If this is a write operation from the Bus Master's point of view,
  // then copy the contents of the mapped buffer into the real buffer
  // so the processor can read the contents of the real buffer.
  //
  if ((MapInfo->Operation == EfiPciOperationBusMasterWrite) ||
      (MapInfo->Operation == EfiPciOperationBusMasterWrite64))
  {
    CopyMem (
      (VOID *)(UINTN)MapInfo->HostAddress,
      (VOID *)(UINTN)MapInfo->MappedHostAddress,
      MapInfo->NumberOfBytes
      );
  }

  //
  // Free the mapped buffer and the MAP_INFO structure.
  //
  gBS->FreePages (MapInfo->MappedHostAddress, MapInfo->NumberOfPages);
  FreePool (Mapping);
  return EFI_SUCCESS;
}

/**
  Allocates pages that are suitable for an EfiPciOperationBusMasterCommonBuffer
  or EfiPciOperationBusMasterCommonBuffer64 mapping.

  @param This        A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param Type        This parameter is not used and must be ignored.
  @param MemoryType  The type of memory to allocate, EfiBootServicesData or
                     EfiRuntimeServicesData.
  @param Pages       The number of pages to allocate.
  @param HostAddress A pointer to store the base system memory address of the
                     allocated range.
  @param Attributes  The requested bit mask of attributes for the allocated
                     range. Only the attributes
                     EFI_PCI_ATTRIBUTE_MEMORY_WRITE_COMBINE,
                     EFI_PCI_ATTRIBUTE_MEMORY_CACHED, and
                     EFI_PCI_ATTRIBUTE_DUAL_ADDRESS_CYCLE may be used with this
                     function.

  @retval EFI_SUCCESS            The requested memory pages were allocated.
  @retval EFI_INVALID_PARAMETER  MemoryType is invalid.
  @retval EFI_INVALID_PARAMETER  HostAddress is NULL.
  @retval EFI_UNSUPPORTED        Attributes is unsupported. The only legal
                                 attribute bits are MEMORY_WRITE_COMBINE,
                                 MEMORY_CACHED, and DUAL_ADDRESS_CYCLE.
  @retval EFI_OUT_OF_RESOURCES   The memory pages could not be allocated.
**/
STATIC
EFI_STATUS
EFIAPI
RootBridgeIoAllocateBuffer (
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL  *This,
  IN  EFI_ALLOCATE_TYPE                Type,
  IN  EFI_MEMORY_TYPE                  MemoryType,
  IN  UINTN                            Pages,
  OUT VOID                             **HostAddress,
  IN  UINT64                           Attributes
  )
{
  EFI_STATUS                Status;
  EFI_PHYSICAL_ADDRESS      PhysicalAddress;
  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge;
  EFI_ALLOCATE_TYPE         AllocateType;

  //
  // Validate Attributes
  //
  if ((Attributes & EFI_PCI_ATTRIBUTE_INVALID_FOR_ALLOCATE_BUFFER) != 0) {
    return EFI_UNSUPPORTED;
  }

  //
  // Check for invalid inputs
  //
  if (HostAddress == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // The only valid memory types are EfiBootServicesData and
  // EfiRuntimeServicesData
  //
  if ((MemoryType != EfiBootServicesData) &&
      (MemoryType != EfiRuntimeServicesData))
  {
    return EFI_INVALID_PARAMETER;
  }

  RootBridge = ROOT_BRIDGE_FROM_THIS (This);

  AllocateType = AllocateAnyPages;
  if (!RootBridge->DmaAbove4G ||
      ((Attributes & EFI_PCI_ATTRIBUTE_DUAL_ADDRESS_CYCLE) == 0))
  {
    //
    // Limit allocations to memory below 4GB
    //
    AllocateType    = AllocateMaxAddress;
    PhysicalAddress = (EFI_PHYSICAL_ADDRESS)(SIZE_4GB - 1);
  }

  Status = gBS->AllocatePages (
                  AllocateType,
                  MemoryType,
                  Pages,
                  &PhysicalAddress
                  );
  if (!EFI_ERROR (Status)) {
    *HostAddress = (VOID *)(UINTN)PhysicalAddress;
  }

  return Status;
}

/**
  Frees memory that was allocated with AllocateBuffer().

  The FreeBuffer() function frees memory that was allocated with
  AllocateBuffer().

  @param This        A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param Pages       The number of pages to free.
  @param HostAddress The base system memory address of the allocated range.

  @retval EFI_SUCCESS            The requested memory pages were freed.
  @retval EFI_INVALID_PARAMETER  The memory range specified by HostAddress and
                                 Pages was not allocated with AllocateBuffer().
**/
STATIC
EFI_STATUS
EFIAPI
RootBridgeIoFreeBuffer (
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL  *This,
  IN  UINTN                            Pages,
  OUT VOID                             *HostAddress
  )
{
  return gBS->FreePages ((EFI_PHYSICAL_ADDRESS)(UINTN)HostAddress, Pages);
}

/**
  Flushes all PCI posted write transactions from a PCI host bridge to system
  memory.

  The Flush() function flushes any PCI posted write transactions from a PCI
  host bridge to system memory. Posted write transactions are generated by PCI
  bus masters when they perform write transactions to target addresses in
  system memory.
  This function does not flush posted write transactions from any PCI bridges.
  A PCI controller specific action must be taken to guarantee that the posted
  write transactions have been flushed from the PCI controller and from all the
  PCI bridges into the PCI host bridge. This is typically done with a PCI read
  transaction from the PCI controller prior to calling Flush().

  @param This        A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.

  @retval EFI_SUCCESS        The PCI posted write transactions were flushed
                             from the PCI host bridge to system memory.
  @retval EFI_DEVICE_ERROR   The PCI posted write transactions were not flushed
                             from the PCI host bridge due to a hardware error.
**/
STATIC
EFI_STATUS
EFIAPI
RootBridgeIoFlush (
  IN EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL  *This
  )
{
  return EFI_SUCCESS;
}

/**
  Gets the attributes that a PCI root bridge supports setting with
  SetAttributes(), and the attributes that a PCI root bridge is currently
  using.

  The GetAttributes() function returns the mask of attributes that this PCI
  root bridge supports and the mask of attributes that the PCI root bridge is
  currently using.

  @param This        A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param Supported   A pointer to the mask of attributes that this PCI root
                     bridge supports setting with SetAttributes().
  @param Attributes  A pointer to the mask of attributes that this PCI root
                     bridge is currently using.

  @retval  EFI_SUCCESS           If Supports is not NULL, then the attributes
                                 that the PCI root bridge supports is returned
                                 in Supports. If Attributes is not NULL, then
                                 the attributes that the PCI root bridge is
                                 currently using is returned in Attributes.
  @retval  EFI_INVALID_PARAMETER Both Supports and Attributes are NULL.
**/
STATIC
EFI_STATUS
EFIAPI
RootBridgeIoGetAttributes (
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL  *This,
  OUT UINT64                           *Supported,
  OUT UINT64                           *Attributes
  )
{
  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge;

  if ((Attributes == NULL) && (Supported == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  RootBridge = ROOT_BRIDGE_FROM_THIS (This);
  //
  // Set the return value for Supported and Attributes.
  //
  if (Supported != NULL) {
    *Supported = RootBridge->Supports;
  }

  if (Attributes != NULL) {
    *Attributes = RootBridge->Attributes;
  }

  return EFI_SUCCESS;
}

/**
  Sets attributes for a resource range on a PCI root bridge.

  The SetAttributes() function sets the attributes specified in Attributes for
  the PCI root bridge on the resource range specified by ResourceBase and
  ResourceLength. Since the granularity of setting these attributes may vary
  from resource type to resource type, and from platform to platform, the
  actual resource range and the one passed in by the caller may differ. As a
  result, this function may set the attributes specified by Attributes on a
  larger resource range than the caller requested. The actual range is returned
  in ResourceBase and ResourceLength. The caller is responsible for verifying
  that the actual range for which the attributes were set is acceptable.

  @param This            A pointer to the
                         EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param Attributes      The mask of attributes to set. If the
                         attribute bit MEMORY_WRITE_COMBINE,
                         MEMORY_CACHED, or MEMORY_DISABLE is set,
                         then the resource range is specified by
                         ResourceBase and ResourceLength. If
                         MEMORY_WRITE_COMBINE, MEMORY_CACHED, and
                         MEMORY_DISABLE are not set, then
                         ResourceBase and ResourceLength are ignored,
                         and may be NULL.
  @param ResourceBase    A pointer to the base address of the
                         resource range to be modified by the
                         attributes specified by Attributes.
  @param ResourceLength  A pointer to the length of the resource
                                   range to be modified by the attributes
                                   specified by Attributes.

  @retval  EFI_SUCCESS           The current configuration of this PCI root bridge
                                 was returned in Resources.
  @retval  EFI_UNSUPPORTED       The current configuration of this PCI root bridge
                                 could not be retrieved.
**/
STATIC
EFI_STATUS
EFIAPI
RootBridgeIoSetAttributes (
  IN     EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL  *This,
  IN     UINT64                           Attributes,
  IN OUT UINT64                           *ResourceBase,
  IN OUT UINT64                           *ResourceLength
  )
{
  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge;

  RootBridge = ROOT_BRIDGE_FROM_THIS (This);

  if ((Attributes & (~RootBridge->Supports)) != 0) {
    return EFI_UNSUPPORTED;
  }

  RootBridge->Attributes = Attributes;
  return EFI_SUCCESS;
}

/**
  Retrieves the current resource settings of this PCI root bridge in the form
  of a set of ACPI resource descriptors.

  There are only two resource descriptor types from the ACPI Specification that
  may be used to describe the current resources allocated to a PCI root bridge.
  These are the QWORD Address Space Descriptor, and the End Tag. The QWORD
  Address Space Descriptor can describe memory, I/O, and bus number ranges for
  dynamic or fixed resources. The configuration of a PCI root bridge is described
  with one or more QWORD Address Space Descriptors followed by an End Tag.

  @param[in]   This        A pointer to the EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL.
  @param[out]  Resources   A pointer to the resource descriptors that
                           describe the current configuration of this PCI root
                           bridge. The storage for the resource
                           descriptors is allocated by this function. The
                           caller must treat the return buffer as read-only
                           data, and the buffer must not be freed by the
                           caller.

  @retval  EFI_SUCCESS     The current configuration of this PCI root bridge
                           was returned in Resources.
  @retval  EFI_UNSUPPORTED The current configuration of this PCI root bridge
                           could not be retrieved.
**/
STATIC
EFI_STATUS
EFIAPI
RootBridgeIoConfiguration (
  IN  EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL  *This,
  OUT VOID                             **Resources
  )
{
  PCI_RESOURCE_TYPE                  Type;
  PCI_ROOT_BRIDGE_INSTANCE           *RootBridge;
  PCI_RES_NODE                       *ResAllocNode;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR  *Descriptor;
  EFI_ACPI_END_TAG_DESCRIPTOR        *End;

  //
  // Get this instance of the Root Bridge.
  //
  RootBridge = ROOT_BRIDGE_FROM_THIS (This);
  ZeroMem (
    RootBridge->ConfigBuffer,
    TypeMax * sizeof (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR) + sizeof (EFI_ACPI_END_TAG_DESCRIPTOR)
    );
  Descriptor = RootBridge->ConfigBuffer;
  for (Type = TypeIo; Type < TypeMax; Type++) {
    ResAllocNode = &RootBridge->ResAllocNode[Type];

    if (ResAllocNode->Status != ResAllocated) {
      continue;
    }

    Descriptor->Desc = ACPI_ADDRESS_SPACE_DESCRIPTOR;
    Descriptor->Len  = sizeof (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR) - 3;
    //
    // According to UEFI 2.7, RootBridgeIo->Configuration should return address
    // range in CPU view (host address), and ResAllocNode->Base is already a CPU
    // view address (host address).
    //
    Descriptor->AddrRangeMin          = ResAllocNode->Base;
    Descriptor->AddrRangeMax          = ResAllocNode->Base + ResAllocNode->Length - 1;
    Descriptor->AddrLen               = ResAllocNode->Length;
    Descriptor->AddrTranslationOffset = GetTranslationByResourceType (
                                          RootBridge,
                                          Type
                                          );

    switch (Type) {
      case TypeIo:
        Descriptor->ResType = ACPI_ADDRESS_SPACE_TYPE_IO;
        break;

      case TypePMem32:
        Descriptor->SpecificFlag = EFI_ACPI_MEMORY_RESOURCE_SPECIFIC_FLAG_CACHEABLE_PREFETCHABLE;
      case TypeMem32:
        Descriptor->ResType              = ACPI_ADDRESS_SPACE_TYPE_MEM;
        Descriptor->AddrSpaceGranularity = 32;
        break;

      case TypePMem64:
        Descriptor->SpecificFlag = EFI_ACPI_MEMORY_RESOURCE_SPECIFIC_FLAG_CACHEABLE_PREFETCHABLE;
      case TypeMem64:
        Descriptor->ResType              = ACPI_ADDRESS_SPACE_TYPE_MEM;
        Descriptor->AddrSpaceGranularity = 64;
        break;

      case TypeBus:
        Descriptor->ResType = ACPI_ADDRESS_SPACE_TYPE_BUS;
        break;

      default:
        break;
    }

    Descriptor++;
  }

  //
  // Terminate the entries.
  //
  End           = (EFI_ACPI_END_TAG_DESCRIPTOR *)Descriptor;
  End->Desc     = ACPI_END_TAG_DESCRIPTOR;
  End->Checksum = 0x0;

  *Resources = RootBridge->ConfigBuffer;
  return EFI_SUCCESS;
}

/**

  Helper for printing aperture info in RootBridgeCreate.

  @paran Pad               Padding for aligning name.
  @param Name              Name of aperture.
  @param Range             EFI_DT_RANGE.

**/
STATIC
VOID
PrintRangeInfo (
  IN  UINTN         Pad,
  IN  CONST CHAR8   *Name,
  IN  EFI_DT_RANGE  *Range
  )
{
  if (!RANGE_VALID (*Range)) {
    DEBUG ((
      DEBUG_INFO,
      "%*a: disabled\n",
      Pad,
      Name
      ));
  } else {
    DEBUG ((
      DEBUG_INFO,
      "%*a: 0x%016lx-0x%016lx translation 0x%016lx\n",
      Pad,
      Name,
      RB (*Range),
      RL (*Range),
      RT (*Range)
      ));
  }
}

/**

  Validates RootBridge configuration.

  @param RootBridge  PCI_ROOT_BRIDGE_INSTANCE *.

  @retval EFI_SUCCESS           Succeed.
  @retval Other                 EFI_STATUS.

**/
STATIC
EFI_STATUS
RootBridgeValidate (
  IN  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge
  )
{
  EFI_STATUS  Status;

  ASSERT (RootBridge != NULL);

  DEBUG ((DEBUG_INFO, "%s:\n", RootBridge->DevicePathStr));
  DEBUG ((DEBUG_INFO, "  Support/Attr: %lx / %lx\n", RootBridge->Supports, RootBridge->Attributes));
  DEBUG ((DEBUG_INFO, "    DmaAbove4G: %s\n", RootBridge->DmaAbove4G ? L"Yes" : L"No"));
  DEBUG ((DEBUG_INFO, "          PCIe: %s\n", !RootBridge->NoExtendedConfigSpace ? L"Yes" : L"No (PCI)"));
  DEBUG ((
    DEBUG_INFO,
    "     AllocAttr: %lx (%s%s)\n",
    RootBridge->AllocationAttributes,
    (RootBridge->AllocationAttributes & EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM) != 0 ? L"CombineMemPMem " : L"",
    (RootBridge->AllocationAttributes & EFI_PCI_HOST_BRIDGE_MEM64_DECODE) != 0 ? L"Mem64Decode" : L""
    ));

  PrintRangeInfo (14, "Bus", &RootBridge->BusRange);
  PrintRangeInfo (14, "Io", &RootBridge->IoRange);
  PrintRangeInfo (14, "Mem", &RootBridge->MemRange);
  PrintRangeInfo (14, "MemAbove4G", &RootBridge->MemAbove4GRange);
  PrintRangeInfo (14, "PMem", &RootBridge->PMemRange);
  PrintRangeInfo (14, "PMemAbove4G", &RootBridge->PMemAbove4GRange);

  if (!RANGE_VALID (RootBridge->BusRange)) {
    Status = EFI_UNSUPPORTED;
    DEBUG ((
      DEBUG_ERROR,
      "%s: Bus: %r\n",
      RootBridge->DevicePathStr,
      Status
      ));
    return Status;
  }

  if (RANGE_VALID (RootBridge->IoRange)) {
    ASSERT (
      RL (RootBridge->IoRange) < SIZE_4GB &&
      RB (RootBridge->IoRange) < SIZE_4GB
      );
    if ((RL (RootBridge->IoRange) >= SIZE_4GB) ||
        (RB (RootBridge->IoRange) >= SIZE_4GB))
    {
      Status = EFI_UNSUPPORTED;
      DEBUG ((
        DEBUG_ERROR,
        "%s: IoRange: %r\n",
        RootBridge->DevicePathStr,
        Status
        ));
      return Status;
    }
  }

  if (RANGE_VALID (RootBridge->MemRange)) {
    ASSERT (
      RL (RootBridge->MemRange) < SIZE_4GB &&
      RB (RootBridge->MemRange) < SIZE_4GB
      );
    if ((RL (RootBridge->MemRange) >= SIZE_4GB) ||
        (RB (RootBridge->MemRange) >= SIZE_4GB))
    {
      Status = EFI_UNSUPPORTED;
      DEBUG ((
        DEBUG_ERROR,
        "%s: MemRange: %r\n",
        RootBridge->DevicePathStr,
        Status
        ));
      return Status;
    }
  }

  if (RANGE_VALID (RootBridge->PMemRange)) {
    ASSERT (
      RL (RootBridge->PMemRange) < SIZE_4GB &&
      RB (RootBridge->PMemRange) < SIZE_4GB
      );
    if ((RL (RootBridge->PMemRange) >= SIZE_4GB) ||
        (RB (RootBridge->PMemRange) >= SIZE_4GB))
    {
      Status = EFI_UNSUPPORTED;
      DEBUG ((
        DEBUG_ERROR,
        "%s: PMemRange: %r\n",
        RootBridge->DevicePathStr,
        Status
        ));
      return Status;
    }
  }

  return EFI_SUCCESS;
}

/**

  Parses actual DT controller info into RootBridge.

  @param RootBridge  PCI_ROOT_BRIDGE_INSTANCE *.

  @retval EFI_SUCCESS           Succeed.
  @retval Other                 EFI_STATUS.

**/
STATIC
EFI_STATUS
RootBridgeDtInit (
  IN  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge
  )
{
  UINT32              BusMin;
  UINT32              BusMax;
  EFI_STATUS          Status;
  EFI_DT_IO_PROTOCOL  *DtIo;
  EFI_DT_RANGE        Range;
  UINTN               Index;

  DtIo = RootBridge->DtIo;

  //
  // We'll support multiple segments, but:
  // - The first segment will be reported as the PciLib
  //   segment (PcdPciExpressBaseAddress).
  // - Global I/O translation (PcdPciIoTranslation) will be
  //   reported for the first segment that has a PCI I/O
  //   window.
  // - DT I/O Reg* acccessors are used to service configuration
  //   space and BAR reads/writes.
  //
  // The driver does NOT yet use the DT I/O DMA API.
  //
  // Have a DT node that looks like:
  //    pci@2c000000 {
  //            reg = <0x0 0x2C000000 0x0 0x1000000
  //                   0x9 0xc0000000 0x0 0x10000000>;
  //            reg-names = "reg", "config";
  //            ranges = <0x82000000  0x0 0x40000000  0x0 0x40000000 0x0 0x40000000>,
  //                     <0xc3000000  0x9 0x80000000  0x9 0x80000000 0x0 0x40000000>;
  //            dma-coherent;
  //            bus-range = <0x00 0xff>;
  //            linux,pci-domain = <0x00>;
  //            device_type = "pci";
  //            compatible = "pci-host-ecam-generic";
  //            #size-cells = <0x02>;
  //            #address-cells = <0x03>;
  //    };
  //

  Status = DtIo->GetU32 (
                   DtIo,
                   "linux,pci-domain",
                   0,
                   &RootBridge->Segment
                   );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_WARN,
      "%s: no segment info, assuming 0\n",
      RootBridge->DevicePathStr
      ));
  }

  Status = DtIo->GetRegByName (DtIo, "config", &RootBridge->ConfigReg);
  if (EFI_ERROR (Status)) {
    //
    // Not every compatible node will use
    // reg-names, so just treat reg[0] as the ECAM window.
    //
    Status = DtIo->GetReg (DtIo, 0, &RootBridge->ConfigReg);
  }

  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%s: couldn't get the ECAM window: %r\n",
      RootBridge->DevicePathStr,
      Status
      ));
    return Status;
  }

  //
  // PCIe, not PCI.
  //
  RootBridge->NoExtendedConfigSpace = FALSE;

  Status = DtIo->GetU32 (DtIo, "bus-range", 0, &BusMin);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%s: Can't get the min-bus number\n",
      RootBridge->DevicePathStr
      ));
    return Status;
  }

  Status = DtIo->GetU32 (DtIo, "bus-range", 1, &BusMax);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%s: Can't get the max-bus number\n",
      RootBridge->DevicePathStr
      ));
    return Status;
  }

  RootBridge->BusRange.ChildBase                =
    RootBridge->BusRange.ParentBase             =
      RootBridge->BusRange.TranslatedParentBase = BusMin;
  RootBridge->BusRange.Length                   = BusMax - BusMin + 1;

  for (Index = 0,
       Status = DtIo->GetRange (
                        DtIo,
                        "ranges",
                        Index,
                        &Range
                        );
       !EFI_ERROR (Status);
       Status = DtIo->GetRange (
                        DtIo,
                        "ranges",
                        ++Index,
                        &Range
                        ))
  {
    EFI_DT_CELL  SpaceCode;

    Status = FbpRangeToPhysicalAddress (&Range, NULL);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%s: couldn't translate range[%lu] to CPU addresses: %r\n",
        RootBridge->DevicePathStr,
        Index,
        Status
        ));
      ASSERT_EFI_ERROR (Status);
      continue;
    }

    SpaceCode = FbpPciGetRangeAttribute (DtIo, Range.ChildBase);
    switch (SpaceCode) {
      case EFI_DT_PCI_HOST_RANGE_IO:
        RootBridge->IoRange = Range;
        break;
      case EFI_DT_PCI_HOST_RANGE_MMIO32:
        RootBridge->MemRange = Range;
        break;
      case EFI_DT_PCI_HOST_RANGE_MMIO32 | EFI_DT_PCI_HOST_RANGE_PREFETCHABLE:
        RootBridge->PMemRange = Range;
        break;
      case EFI_DT_PCI_HOST_RANGE_MMIO64:
        RootBridge->MemAbove4GRange = Range;
        break;
      case EFI_DT_PCI_HOST_RANGE_MMIO64 | EFI_DT_PCI_HOST_RANGE_PREFETCHABLE:
        RootBridge->PMemAbove4GRange = Range;
        break;
      default:
        //
        // Don't know what to do with EFI_DT_PCI_HOST_RANGE_RELOCATABLE or
        // EFI_DT_PCI_HOST_RANGE_ALIASED, or if they are even expected.
        //
        DEBUG ((
          DEBUG_ERROR,
          "%s: Unknown SpaceCode 0x%x is detected\n",
          RootBridge->DevicePathStr,
          SpaceCode
          ));
        break;
    }
  }

  RootBridge->Supports     =
    RootBridge->Attributes = EFI_PCI_ATTRIBUTE_ISA_IO_16 |
                             EFI_PCI_ATTRIBUTE_ISA_MOTHERBOARD_IO |
                             EFI_PCI_ATTRIBUTE_VGA_IO_16  |
                             EFI_PCI_ATTRIBUTE_VGA_PALETTE_IO_16;

  RootBridge->AllocationAttributes = 0;
  if (!RANGE_VALID (RootBridge->PMemRange) &&
      !RANGE_VALID (RootBridge->PMemAbove4GRange))
  {
    RootBridge->AllocationAttributes |= EFI_PCI_HOST_BRIDGE_COMBINE_MEM_PMEM;
  }

  if (RANGE_VALID (RootBridge->MemAbove4GRange) ||
      RANGE_VALID (RootBridge->PMemAbove4GRange))
  {
    RootBridge->AllocationAttributes |= EFI_PCI_HOST_BRIDGE_MEM64_DECODE;
  }

  Status = DtIo->GetRange (DtIo, "dma-ranges", 0, &Range);
  //
  // Not handled yet, and we don't know the format. Assume TRUE
  // (valid for PC-like systems).
  //
  ASSERT (Status == EFI_NOT_FOUND);
  ASSERT (DtIo->IsDmaCoherent);
  RootBridge->DmaAbove4G = TRUE;

  return EFI_SUCCESS;
}

/**

  Creates a PCI_ROOT_BRIDGE_INSTANCE.

  @param DtIo              DT protocol on Controller.
  @param Controller        DT controller.
  @param DevicePath        Controller device path.
  @param Out               PCI_ROOT_BRIDGE_INSTANCE ** to
                           populate.

  @retval EFI_SUCCESS           Succeed.
  @retval EFI_OUT_OF_RESOURCES  Allocation failure.

**/
EFI_STATUS
RootBridgeCreate (
  IN  EFI_DT_IO_PROTOCOL        *DtIo,
  IN  EFI_HANDLE                Controller,
  IN  EFI_DEVICE_PATH_PROTOCOL  *DevicePath,
  OUT PCI_ROOT_BRIDGE_INSTANCE  **Out
  )
{
  EFI_STATUS                Status;
  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge;
  CHAR16                    *DevicePathStr;
  VOID                      *ConfigBuffer;

  ASSERT (DtIo != NULL);
  ASSERT (Controller != NULL);
  ASSERT (DevicePath != NULL);
  ASSERT (Out != NULL);

  RootBridge    = NULL;
  DevicePathStr = NULL;

  RootBridge = AllocateZeroPool (sizeof (PCI_ROOT_BRIDGE_INSTANCE));
  if (RootBridge == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((
      DEBUG_ERROR,
      "%a: AllocateZeroPool: %r\n",
      __func__,
      Status
      ));
    goto out;
  }

  DevicePathStr = ConvertDevicePathToText (DevicePath, FALSE, FALSE);
  if (DevicePathStr == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((
      DEBUG_ERROR,
      "%a: ConvertDevicePathToText: %r\n",
      __func__,
      Status
      ));
    goto out;
  }

  ConfigBuffer = AllocatePool (
                   TypeMax * sizeof (EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR) + sizeof (EFI_ACPI_END_TAG_DESCRIPTOR)
                   );
  if (ConfigBuffer == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    DEBUG ((
      DEBUG_ERROR,
      "%a: ConfigBuffer: %r\n",
      __func__,
      Status
      ));
    goto out;
  }

  RootBridge->Signature     = PCI_ROOT_BRIDGE_SIGNATURE;
  RootBridge->Controller    = Controller;
  RootBridge->DtIo          = DtIo;
  RootBridge->DevicePathStr = DevicePathStr;
  RootBridge->ConfigBuffer  = ConfigBuffer;
  InitializeListHead (&RootBridge->Maps);

  Status = RootBridgeDtInit (RootBridge);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%s: RootBridgeDtInit: %r\n",
      DevicePathStr,
      Status
      ));
    goto out;
  }

  Status = RootBridgeValidate (RootBridge);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%s: RootBridgeValidate: %r\n",
      DevicePathStr,
      Status
      ));
    goto out;
  }

  if (PcdGet64 (PcdPciExpressBaseAddress) == MAX_UINT64) {
    EFI_PHYSICAL_ADDRESS  EcamBase;

    Status = FbpRegToPhysicalAddress (&RootBridge->ConfigReg, &EcamBase);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%s: couldn't get the ECAM window CPU address: %r\n",
        RootBridge->DevicePathStr,
        Status
        ));
      goto out;
    }

    Status = PcdSet64S (
               PcdPciExpressBaseAddress,
               EcamBase
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%s: PcdSet64S(PcdPciExpressBaseAddress): %r\n",
        RootBridge->DevicePathStr,
        Status
        ));
      goto out;
    }

    DEBUG ((
      DEBUG_INFO,
      "%s: segment %u used for PciLib\n",
      RootBridge->DevicePathStr,
      RootBridge->Segment
      ));
  }

  Status = PcdSetBoolS (PcdPciDisableBusEnumeration, FALSE);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%s: PcdSetBoolS(PcdPciDisableBusEnumeration): %r\n",
      RootBridge->DevicePathStr,
      Status
      ));
    goto out;
  }

  if (RANGE_VALID (RootBridge->IoRange)) {
    Status = AddIoSpace (&RootBridge->IoRange);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%s: AddIoSpace: %r\n",
        RootBridge->DevicePathStr,
        Status
        ));
      goto out;
    }

    if (PcdGet64 (PcdPciIoTranslation) == 0) {
      Status = PcdSet64S (PcdPciIoTranslation, -RT (RootBridge->IoRange));
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%s: PcdSet64S(PcdPciIoTranslation): %r\n",
          RootBridge->DevicePathStr,
          Status
          ));
        goto out;
      }
    }
  }

  RootBridge->RootBridgeIo.SegmentNumber  = RootBridge->Segment;
  RootBridge->RootBridgeIo.PollMem        = RootBridgeIoPollMem;
  RootBridge->RootBridgeIo.PollIo         = RootBridgeIoPollIo;
  RootBridge->RootBridgeIo.Mem.Read       = RootBridgeIoMemRead;
  RootBridge->RootBridgeIo.Mem.Write      = RootBridgeIoMemWrite;
  RootBridge->RootBridgeIo.Io.Read        = RootBridgeIoIoRead;
  RootBridge->RootBridgeIo.Io.Write       = RootBridgeIoIoWrite;
  RootBridge->RootBridgeIo.CopyMem        = RootBridgeIoCopyMem;
  RootBridge->RootBridgeIo.Pci.Read       = RootBridgeIoPciRead;
  RootBridge->RootBridgeIo.Pci.Write      = RootBridgeIoPciWrite;
  RootBridge->RootBridgeIo.Map            = RootBridgeIoMap;
  RootBridge->RootBridgeIo.Unmap          = RootBridgeIoUnmap;
  RootBridge->RootBridgeIo.AllocateBuffer = RootBridgeIoAllocateBuffer;
  RootBridge->RootBridgeIo.FreeBuffer     = RootBridgeIoFreeBuffer;
  RootBridge->RootBridgeIo.Flush          = RootBridgeIoFlush;
  RootBridge->RootBridgeIo.GetAttributes  = RootBridgeIoGetAttributes;
  RootBridge->RootBridgeIo.SetAttributes  = RootBridgeIoSetAttributes;
  RootBridge->RootBridgeIo.Configuration  = RootBridgeIoConfiguration;
  RootBridge->RootBridgeIo.ParentHandle   = Controller;

  HostBridgeInit (RootBridge);

out:
  if (EFI_ERROR (Status)) {
    if (ConfigBuffer != NULL) {
      FreePool (ConfigBuffer);
    }

    if (DevicePathStr != NULL) {
      FreePool (DevicePathStr);
    }

    if (RootBridge != NULL) {
      FreePool (RootBridge);
    }
  } else {
    *Out = RootBridge;
  }

  return Status;
}

/**

  Frees a PCI_ROOT_BRIDGE_INSTANCE.

  @param RootBridge        PCI_ROOT_BRIDGE_INSTANCE *.

**/
VOID
RootBridgeFree (
  IN  PCI_ROOT_BRIDGE_INSTANCE  *RootBridge
  )
{
  ASSERT (RootBridge != NULL);

  FreePool (RootBridge->ConfigBuffer);
  FreePool (RootBridge->DevicePathStr);
  FreePool (RootBridge);
}
