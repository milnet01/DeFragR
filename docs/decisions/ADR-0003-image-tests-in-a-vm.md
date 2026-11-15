# ADR-0003: Image tests run as root inside a throwaway virtual machine

- **Status:** Accepted
- **Date:** 2026-11-15

## Context

Testing a real defrag needs a mounted ext4 filesystem and root.
Mounting an image needs root on the host, and ext4 cannot be mounted
from an unprivileged user namespace. FUSE ext4 does not support
`EXT4_IOC_MOVE_EXT`. The owner's real drives hold data with no other
copy, so a test bug must not be able to reach them.

## Decision

Image tests run inside a small QEMU virtual machine booted by a script
in `tests/vm/`. The test is root inside it. Test disks are image files
attached as virtual disks; the host's drives are not attached as block
devices.

The test scripts refuse any block device that is not one of those
virtual disks.

## Consequences

- No password prompt for a test run, so the local gate can run them.
- A test bug can damage only throwaway images.
- The test machine can present a disk as rotating or not, which would
  test the SSD refusal. This is unverified until the test machine item
  is built.
- More setup than loop devices with `sudo`: a kernel, an initial root
  filesystem, and boot time on every run.
- Where virtualisation is unavailable, the image tests cannot run.
  Whether GitHub's runners allow it is unverified.

## Rejected

Loop devices on the host with `sudo`: simpler, but asks for a password
every run, and runs test code as root beside the real drives.
