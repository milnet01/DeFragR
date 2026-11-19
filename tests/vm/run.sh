#!/usr/bin/env bash
# Boot the throwaway test machine, run one script in it as root, and exit
# with that script's exit code. ADR-0003 says why image tests run here.
#
# Usage:
#   run.sh --work DIR [--disk NAME=IMAGE[,ssd]]... [--copy FILE]... [--cpu-without FLAG] SCRIPT
#
#   --work DIR    where the machine's boot files and console log go
#   --disk        attach the image file IMAGE as a test disk called NAME.
#                 It spins (rotational 1) unless ",ssd" is given. In the
#                 machine, guard.sh's test_disk NAME finds it.
#   --copy FILE   put FILE in the machine's /test, beside the script
#   --cpu-without FLAG
#                 take the QEMU CPU flag FLAG, such as sse4.2, away from
#                 the machine's CPU, which is the host's own, or QEMU's
#                 "max". Not an older CPU model: the host's busybox may
#                 need the rest, as Ubuntu's amd64v3 build needs AVX.
#   SCRIPT        run as /test/run by the machine's shell, in /test
#
# The machine is the host's own kernel and a small initial filesystem:
# busybox, e2fsprogs, swapon and swapoff, the kernel modules for its disks and filesystems,
# /test/guard.sh and the files above. It has no network and no disk but the
# ones named here.
# Only regular files can be named, so no host drive can be attached.
#
# Exit codes: the script's own; 77 when this host cannot boot the machine
# (no QEMU, no readable kernel), which CTest reports as skipped unless
# DEFRAGR_VM_REQUIRED=1 makes it a failure; 125 for any other failure of
# the machine itself (bad arguments, boot failure, timeout).
#
# The kernel is the one the host is running, so its modules are installed.
# DEFRAGR_VM_TIMEOUT sets how many seconds the machine may run (300).
set -Eeuo pipefail

here=$(cd "$(dirname "$0")" && pwd)

die() { printf 'test machine: %s\n' "$*" >&2; exit 125; }
cannot_boot() {
    printf 'test machine: cannot boot here: %s\n' "$*" >&2
    if [[ ${DEFRAGR_VM_REQUIRED:-0} == 1 ]]; then exit 125; fi
    exit 77
}

work="" script="" without=""
disks=() copies=()
while (( $# )); do
    case $1 in
        --work) work=${2:?--work needs a directory}; shift 2 ;;
        --disk) disks+=("${2:?--disk needs NAME=IMAGE}"); shift 2 ;;
        --copy) copies+=("${2:?--copy needs a file}"); shift 2 ;;
        --cpu-without) without=,-${2:?--cpu-without needs a flag}; shift 2 ;;
        -*) die "unknown option $1" ;;
        *) [[ -z $script ]] || die "one script only"; script=$1; shift ;;
    esac
done
[[ -n $work ]] || die "--work is required"
[[ -f $script ]] || die "no script: ${script:-none given}"

# ── What this host must have ────────────────────────────────────────────────
for tool in qemu-system-x86_64 cpio zstd readelf; do
    command -v "$tool" >/dev/null || cannot_boot "$tool is not installed"
done
kver=$(uname -r)
modules=/lib/modules/$kver
[[ -f $modules/modules.dep ]] || cannot_boot "no modules.dep in $modules"
# openSUSE and Fedora keep the kernel beside its modules, Debian and Ubuntu
# in /boot. Ubuntu's is readable by root only until made readable.
for kernel in "$modules/vmlinuz" "/boot/vmlinuz-$kver" ""; do
    [[ -f $kernel ]] && break
done
[[ -n $kernel && -r $kernel ]] || cannot_boot "no readable kernel image for $kver"
PATH=$PATH:/usr/sbin:/sbin command -v modprobe >/dev/null \
    || cannot_boot "modprobe is not installed (kmod)"

busybox=
for candidate in busybox-static busybox; do
    path=$(command -v "$candidate" || true)
    [[ -n $path ]] || continue
    # The machine has no C library of its own for busybox, so it must be
    # static.
    if ! readelf --program-headers "$path" | grep -q INTERP; then
        busybox=$path
        break
    fi
done
[[ -n $busybox ]] || cannot_boot "no static busybox (busybox-static) installed"

# e2fsprogs and util-linux live in sbin, which is not on every user's PATH.
# busybox's swapon is not in every build.
tool_path() { PATH=$PATH:/usr/sbin:/sbin command -v "$1"; }
guest_tools=()
for tool in mkfs.ext4 e2fsck filefrag swapon swapoff; do
    path=$(tool_path "$tool") || cannot_boot "$tool is not installed (e2fsprogs, util-linux)"
    guest_tools+=("$path")
done

# ── The disks ───────────────────────────────────────────────────────────────
# Each is a regular file, never a symlink and never a device, so the host's
# drives cannot be attached.
disk_args=()
names=" "
index=0
for spec in "${disks[@]}"; do
    name=${spec%%=*}
    rest=${spec#*=}
    image=${rest%,ssd}
    rotation=7200
    [[ $rest == *,ssd ]] && rotation=1
    [[ $spec == *=* && $name =~ ^[a-z][a-z0-9-]{0,15}$ ]] \
        || die "disk name must be 1-16 of a-z, 0-9 and -, starting with a letter: $spec"
    [[ $names != *" $name "* ]] || die "two disks are called $name"
    names+="$name "
    [[ ! -L $image && -f $image ]] || die "$image is not a regular file"
    [[ -w $image ]] || die "$image is not writable"
    # QEMU reads a comma as the start of its next option.
    [[ $image != *,* ]] || die "$image has a comma in its path"
    # The vendor marks every test disk; guard.sh refuses any device without
    # it. The product is the name test_disk looks up. rotation_rate 1 is
    # SCSI's "not rotating", which Linux shows as rotational 0.
    # cache=unsafe: the machine's flushes complete without reaching the
    # host's drive. Inside the machine nothing changes, and the image is
    # throwaway; on a spinning host drive each flush otherwise waits for
    # the platter, which made image_build ten times slower.
    disk_args+=(
        -drive "file=$image,format=raw,if=none,cache=unsafe,id=disk$index"
        -device "scsi-hd,drive=disk$index,bus=scsi0.0,vendor=DEFRAGR,product=$name,serial=dfr-$name,rotation_rate=$rotation"
    )
    index=$(( index + 1 ))
done

# ── The machine's initial filesystem ────────────────────────────────────────
mkdir -p "$work"
work=$(cd "$work" && pwd)
root=$work/root
rm -rf "$root"
mkdir -p "$root"/{bin,dev,proc,sys,tmp,mnt,test,lib/modules}

install -m 0755 "$busybox" "$root/bin/busybox"
# One link per command; not every busybox build can make them itself.
for applet in $("$busybox" --list); do
    [[ -e $root/bin/$applet ]] || ln -s busybox "$root/bin/$applet"
done
install -m 0755 "$here/init.sh" "$root/init"
install -m 0644 "$here/guard.sh" "$root/test/guard.sh"
install -m 0755 "$script" "$root/test/run"
for file in "${copies[@]}"; do
    [[ -f $file ]] || die "no file to copy: $file"
    install -m 0755 "$file" "$root/test/$(basename "$file")"
done

# Each tool, and every library it loads, at the path it loads it from.
for tool in "${guest_tools[@]}"; do
    install -m 0755 "$tool" "$root/bin/$(basename "$tool")"
    while read -r lib; do
        mkdir -p "$root$(dirname "$lib")"
        cp -L "$lib" "$root$lib"
    done < <(ldd "$tool" | grep -o '/[^ ]*')
done

# The modules the disks and filesystems need, in load order, decompressed
# because busybox's insmod cannot read a compressed module. A module built
# into the kernel prints "builtin" and needs nothing. autofs is built in on
# openSUSE but a module on Ubuntu, so it must be named here.
load_order=$(for module in virtio_scsi sd_mod ext4 loop autofs4 squashfs; do
    PATH=$PATH:/usr/sbin:/sbin modprobe -S "$kver" --show-depends "$module"
done) || die "a module the machine needs is missing for $kver"
: >"$root/lib/modules/order"
while read -r verb module _; do
    [[ $verb == insmod ]] || continue
    base=$(basename "$module")
    base=${base%.zst}; base=${base%.xz}; base=${base%.gz}
    grep -qx "$base" "$root/lib/modules/order" && continue
    case $module in
        *.zst) zstd -dcq "$module" ;;
        *.xz) xz -dc "$module" ;;
        *.gz) gzip -dc "$module" ;;
        *) cat "$module" ;;
    esac >"$root/lib/modules/$base"
    printf '%s\n' "$base" >>"$root/lib/modules/order"
done <<<"$load_order"

(cd "$root" && find . -print0 | cpio --null --create --format=newc --quiet) \
    >"$work/initramfs.cpio"

# ── Boot ────────────────────────────────────────────────────────────────────
accel=(-accel tcg -cpu "max$without")
if [[ -r /dev/kvm && -w /dev/kvm ]]; then
    accel=(-accel kvm -cpu "host$without")
fi
printf 'test machine: kernel %s, %s, %d disk(s)\n' "$kver" "${accel[1]}" "$index" >&2

console=$work/console.log
rc=0
timeout "${DEFRAGR_VM_TIMEOUT:-300}" qemu-system-x86_64 \
    "${accel[@]}" -m 1024 -smp 2 \
    -nodefaults -display none -no-reboot -serial stdio -monitor none -nic none \
    -kernel "$kernel" -initrd "$work/initramfs.cpio" \
    -append "console=ttyS0 quiet panic=-1 dfr.disks=$index" \
    -device virtio-scsi-pci,id=scsi0 "${disk_args[@]}" \
    </dev/null 2>&1 | tr -d '\r' | tee "$console" || rc=$?
(( rc == 0 )) || die "QEMU failed or timed out (exit $rc); console log: $console"

result=$(sed -n 's/^DFR-VM-RESULT \([0-9]*\)$/\1/p' "$console" | tail -n 1)
[[ -n $result ]] || die "the machine stopped without a result; console log: $console"
exit "$result"
