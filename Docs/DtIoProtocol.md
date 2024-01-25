# EFI Device Tree I/O Protocol

> [!NOTE]
> This is not a USWG-ratified protocol at the moment.

This section provides a detailed description of the EFI_DT_IO_PROTOCOL.
This protocol is used by code, typically platform device drivers,
running in the EFI boot services environment to access device I/O on a
DT controller. In particular, functions for managing platform devices
exposed via Device Tree as DT controllers are defined here.

The interfaces provided in the EFI_DT_IO_PROTOCOL are for performing
basic operations for device property access, register I/O, DMA buffer
handling and device enumeration. The system provides abstracted access
to basic system resources to allow a driver to have a programmatic
method to access these basic system resources. The main goal of this
protocol is to provide an abstraction that simplifies the writing of
device drivers for platform devices. This goal is accomplished by providing
the following features:
- A driver model that does not require the driver to hardcode or
search for platform devices to manage. Instead, drivers are provided the
location of the device to manage or have the capability to be notified
when a DT controller is discovered.
- A device driver model that abstracts device register access. A
driver does not have to manually parse _reg_ properties or traverse
parent Device Tree nodes to map back to a valid CPU address. Instead,
relative addressing is used for all register accesses, and the API
fully hides the complexity of performing such accesses. It even
supports register accesses via a parent DT controller device driver.
- The Device Path for the platform device can be obtained from the same
 device handle that the EFI_DT_IO_PROTOCOL resides on.
- A full set of functions to parse and return various device property values.
- Functions to perform bus mastering DMA. This includes both packet
based DMA and common buffer DMA, and deals with DMA range
restrictions, non-cache coherent DMA and CPU barriers where applicable.

## EFI_DT_IO_PROTOCOL

### Summary

Provides the basic device property, register I/O, DMA buffer and
device enumeration interfaces that a driver uses to access its DT
controller.

### GUID

### Protocol Interface Structure

### Parameters

### Related Definitions

### Description
