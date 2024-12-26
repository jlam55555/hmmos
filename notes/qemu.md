# qemu notes
I had a few notes about QEMU scattered throughout, now that we've
gotten a little more intimate I think this deserves its own page

## x86 quirks
Both of the following make my life easier when running in QEMU, but
make it harder for me to handle the expected case for real hardware.

- *A20 is set on boot*: To be fair, the OSDev wiki states that this is
  very non-standardized and there are a myriad of ways to
  enable/disable the A20 line depending on the hardware. In QEMU's
  case, [it was already set on
  boot](https://forum.osdev.org/viewtopic.php?f=1&t=26256).
- *No data segment offset limit checks*: In real mode, I noticed I can
  write to 0x0000:0xB8000 without having to use unreal
  mode. Similarly, when in protected mode/unreal mode, I was unable to
  trigger the segment check (which should cause a
  [GPF](https://wiki.osdev.org/Exceptions#General_Protection_Fault))
  even if I manually set the limits low. It seems that QEMU doesn't
  perform the limit checks ([at least as of
  2019](https://lists.gnu.org/archive/html/qemu-devel/2019-02/msg06518.html)).
  I do get GPFs for the code segment though.
- *PCI vendor ID 0x1234*: According to [this blog
  post](https://web.archive.org/web/20200416081308/https://www.kraxel.org/blog/2020/01/qemu-pci-ids/). Indeed
  0x1234 doesn't show up on the [list of PCI
  vendors](https://devicehunt.com/all-pci-vendors).

## QEMU/KVM bugs/weirdness
- See "hmm.md#slow\_memory\_speed" for a QEMU emulation performance issue.
- See "hmm.md#KVM memory invalidation heisenbug" for what seems to be a
KVM bug.
- See "demos.md#locking" which demonstrates that increment (a++)
  appears to be atomic in QEMU emulation

Probably if I tried hard enough both of these can be linked to
QEMU/KVM code but I haven't found the time to look deeper.

## default QEMU setup
The default OS setup uses a Q35 architecture (based on [Intel's ~2009
ICH9 architecture](https://wiki.qemu.org/Features/Q35)), which treats
drivers as AHCI/SATA by default rather than IDE.

We use QEMU emulation by default unless you pass `-accel kvm` (`make
run KVM=1`). It's good to test with both emulation modes to shake out
differences in emulation (just as running with both GCC and clang is a
good idea).

## useful debugging flags
These all feature in some form in the Makefile:

- `-serial stdio` (default): useful for emulating a serial port and
  receiving output on the host machine
- `-display none -serial stdio` (`make run TEST=...`): useful for
  running tests since the OS doesn't take any input
- `-d int,cpu_reset -no-reboot` (`make run SHOWINT=1`): logging of
  interrupts and cpu reset (i.e. during a triple fault). `-no-reboot`
  prevents automatic rebooting during triple-fault
- `-S -s` (`make runi`): start gdbserver at localhost:1234 and wait
  for GDB
- `-monitor unix:qemu-monitor-socket,server,nowait` (`make run
  QMONITOR=1`): allows you to [connect to the QEMU
  monitor](https://wiki.qemu.org/Features/Q35) via socket. Some useful
  monitor commands:
  - `info tlb`: when debugging memory mapping issues
  - `info qtree`: to [dump emulated device
    info](https://serverfault.com/a/913622)
- `-trace enable=ahci_*` or similar. See `qemu-system-i386 -trace ?`
  to view all trace tags.

The GDB and qmonitor `make` commands come in pairs: one to start the
VM/gdbserver/monitor server, and the other to start gdb/QEMU monitor:
- `make runi`/`make gdb`
- `make run QMONITOR=1`/`make qmonitor`
