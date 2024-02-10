# EFI-Specific Devicetree Bindings

> [!NOTE]
> See [FdtBusPkg Documentation Style and Terms Definitions](StyleAndTerms.md) first.

These apply on top of [Devicetree Specification, Chapter 4](https://devicetree-specification.readthedocs.io/en/stable/device-bindings.html).

## Miscellaneous Properties

### _uefi,critical_

| Property | Value Type | Description |
| -------- | :--------: | ----------- |
| _uefi,critical_ | - | Marks a DT controller as critical. A critical controller is always connected at End-of-DXE event (`gEfiEndOfDxeEventGroupGuid`). This facility allows device to be always initialized, even under rapid boot conditions. |

> [!NOTE]
> This property is parsed by FdtBusDxe during processing of child DT
> controllers. Thus, a DT device with _uefi,critical_ must be a child
> of a controller supported by an existing driver, and this controller
> needs to have a _uefi,critical_ property as well.

> [!NOTE]
> Devices of type _memory_ are implicitly treated as having _uefi,critical_.