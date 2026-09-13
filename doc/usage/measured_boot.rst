.. SPDX-License-Identifier: GPL-2.0+

Measured Boot
=============

U-Boot can perform a measured boot, the process of hashing various components
of the boot process, extending the results in the TPM and logging the
component's measurement in memory for the operating system to consume.

The functionality is available when booting via the EFI subsystem or 'bootm'
command.

UEFI measured boot
------------------

The EFI subsystem implements the `EFI TCG protocol
<https://trustedcomputinggroup.org/resource/tcg-efi-protocol-specification/>`_
and the `TCG PC Client Specific Platform Firmware Profile Specification
<https://trustedcomputinggroup.org/resource/pc-client-specific-platform-firmware-profile-specification/>`_
which defines the binaries to be measured and the corresponding PCRs to be used.

Requirements
~~~~~~~~~~~~

* A hardware TPM 2.0 supported by an enabled U-Boot driver
* CONFIG_EFI_TCG2_PROTOCOL=y
* optional CONFIG_EFI_TCG2_PROTOCOL_MEASURE_DTB=y will measure the loaded DTB
  in PCR 1

Legacy measured boot
--------------------

The commands booti, bootm, and bootz can be used for measured boot
using the legacy entry point of the Linux kernel.

By default, U-Boot will measure the operating system (linux) image, the
initrd image, and the "bootargs" environment variable. By enabling
CONFIG_MEASURE_DEVICETREE, U-Boot will also measure the devicetree image in PCR1.

The operating system typically would verify that the hashes found in the
TPM PCRs match the contents of the event log. This can further be checked
against the hash results of previous boots.

Requirements
~~~~~~~~~~~~

* A hardware TPM 2.0 supported by an enabled U-Boot driver
* CONFIG_TPMv2=y
* CONFIG_MEASURED_BOOT=y
* Device-tree configuration of the TPM device to specify the memory area
  for event logging. The TPM device node must either contain a phandle to
  a reserved memory region or "linux,sml-base" and "linux,sml-size"
  indicating the address and size of the memory region. An example can be
  found in arch/sandbox/dts/test.dts

Passing the event log to the operating system
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

After measuring, U-Boot adds "linux,sml-base" and "linux,sml-size" to the TPM
node of the devicetree it passes to the operating system, giving the address
and size of the log, and reserves that memory so that it survives until the
kernel's TPM driver reads it. This is the same mechanism Linux uses for a
firmware-provided log on any non-EFI boot, so it works whatever devicetree
the OS is booted with: it need not be U-Boot's own. The node is matched by
the path of U-Boot's TPM node or, failing that, by its compatible string; if
the devicetree has no TPM node, a warning is printed and the OS will not see
the log.
