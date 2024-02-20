## @file
#
#  Copyright (c) 2023, Intel Corporation. All rights reserved.<BR>
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

[Defines]
  PLATFORM_NAME                  = FdtBusPkg
  PLATFORM_GUID                  = 62ad1d2c-3333-4021-b32d-268d0e71c032
  PLATFORM_VERSION               = 0.98
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/FdtBusPkg
  SUPPORTED_ARCHITECTURES        = AARCH64|RISCV64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  SKUID_IDENTIFIER               = DEFAULT
  PREBUILD                       = sh FdtBusPkg/Drivers/FdtBusDxe/TestDt.sh

!include MdePkg/MdeLibs.dsc.inc

[PcdsFixedAtBuild.common]
  # DEBUG_ASSERT_ENABLED       0x01
  # DEBUG_PRINT_ENABLED        0x02
  # DEBUG_CODE_ENABLED         0x04
  # CLEAR_MEMORY_ENABLED       0x08
  # ASSERT_BREAKPOINT_ENABLED  0x10
  # ASSERT_DEADLOOP_ENABLED    0x20
!if $(TARGET) == RELEASE
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x23
!else
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x2f
!endif

  #  DEBUG_INIT      0x00000001  // Initialization
  #  DEBUG_WARN      0x00000002  // Warnings
  #  DEBUG_LOAD      0x00000004  // Load events
  #  DEBUG_FS        0x00000008  // EFI File system
  #  DEBUG_POOL      0x00000010  // Alloc & Free (pool)
  #  DEBUG_PAGE      0x00000020  // Alloc & Free (page)
  #  DEBUG_INFO      0x00000040  // Informational debug messages
  #  DEBUG_DISPATCH  0x00000080  // PEI/DXE/SMM Dispatchers
  #  DEBUG_VARIABLE  0x00000100  // Variable
  #  DEBUG_BM        0x00000400  // Boot Manager
  #  DEBUG_BLKIO     0x00001000  // BlkIo Driver
  #  DEBUG_NET       0x00004000  // SNP Driver
  #  DEBUG_UNDI      0x00010000  // UNDI Driver
  #  DEBUG_LOADFILE  0x00020000  // LoadFile
  #  DEBUG_EVENT     0x00080000  // Event messages
  #  DEBUG_GCD       0x00100000  // Global Coherency Database changes
  #  DEBUG_CACHE     0x00200000  // Memory range cachability changes
  #  DEBUG_VERBOSE   0x00400000  // Detailed debug messages that may
  #                              // significantly impact boot performance
  #  DEBUG_ERROR     0x80000000  // Error
!if $(TARGET) == RELEASE
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x80000000
!else
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x8000004F
!endif

[PcdsDynamicDefault]
#
# These 3 are here only to compile FdtPciPcdProducerLib.
#
  gEfiMdePkgTokenSpaceGuid.PcdPciExpressBaseAddress|0xffffffffffffffff
  gEfiMdeModulePkgTokenSpaceGuid.PcdPciDisableBusEnumeration|TRUE
  gEfiMdePkgTokenSpaceGuid.PcdPciIoTranslation|0x0

[LibraryClasses.common]
  DebugLib|MdePkg/Library/UefiDebugLibConOut/UefiDebugLibConOut.inf
  # DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  # DebugLib|MdePkg/Library/BaseDebugLibSerialPort/BaseDebugLibSerialPort.inf
  # SerialPortLib|MdePkg/Library/BaseSerialPortLibNull/BaseSerialPortLibNull.inf

  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
  NULL|MdePkg/Library/BaseStackCheckLib/BaseStackCheckLib.inf
  HobLib|MdePkg/Library/DxeHobLib/DxeHobLib.inf
  FdtLib|EmbeddedPkg/Library/FdtLib/FdtLib.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLibDevicePathProtocol/UefiDevicePathLibDevicePathProtocol.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  FbpUtilsLib|FdtBusPkg/Library/FbpUtilsLib/FbpUtilsLib.inf
  FbpPciUtilsLib|FdtBusPkg/Library/FbpPciUtilsLib/FbpPciUtilsLib.inf
  DxeServicesTableLib|MdePkg/Library/DxeServicesTableLib/DxeServicesTableLib.inf

[LibraryClasses.AARCH64]
  NULL|ArmPkg/Library/CompilerIntrinsicsLib/CompilerIntrinsicsLib.inf
  TimerLib|ArmPkg/Library/ArmArchTimerLib/ArmArchTimerLib.inf
  ArmLib|ArmPkg/Library/ArmLib/ArmBaseLib.inf
  ArmGenericTimerCounterLib|ArmPkg/Library/ArmGenericTimerVirtCounterLib/ArmGenericTimerVirtCounterLib.inf

[LibraryClasses.RISCV64]
  TimerLib|UefiCpuPkg/Library/BaseRiscV64CpuTimerLib/BaseRiscV64CpuTimerLib.inf

[Components]
  FdtBusPkg/Library/PciHostBridgeLibEcam/PciHostBridgeLibEcam.inf
  FdtBusPkg/Library/FdtPciPcdProducerLib/FdtPciPcdProducerLib.inf

  FdtBusPkg/Drivers/SampleDeviceDxe/Driver.inf
  FdtBusPkg/Drivers/SampleBusDxe/Driver.inf
  FdtBusPkg/Drivers/PciHostBridgeFdtDxe/Driver.inf
  FdtBusPkg/Drivers/FdtBusDxe/FdtBusDxe.inf
  FdtBusPkg/Drivers/VirtioFdtDxe/VirtioFdtDxe.inf {
    <LibraryClasses>
       IoLib|MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsic.inf
       VirtioLib|OvmfPkg/Library/VirtioLib/VirtioLib.inf
       VirtioMmioDeviceLib|OvmfPkg/Library/VirtioMmioDeviceLib/VirtioMmioDeviceLib.inf
  }
  FdtBusPkg/Drivers/HighMemDxe/HighMemDxe.inf {
    <LibraryClasses>
       DxeServicesTableLib|MdePkg/Library/DxeServicesTableLib/DxeServicesTableLib.inf
  }
  FdtBusPkg/Drivers/HighMemDxe/HighMemDxeNoBinding.inf {
    <LibraryClasses>
       DxeServicesTableLib|MdePkg/Library/DxeServicesTableLib/DxeServicesTableLib.inf
  }
  FdtBusPkg/Drivers/PciSioSerialDxe/PciSioSerialDxe.inf {
    <LibraryClasses>
       IoLib|MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsic.inf
       ReportStatusCodeLib|MdeModulePkg/Library/DxeReportStatusCodeLib/DxeReportStatusCodeLib.inf
  }
