#!/bin/busybox sh
# The test machine's first program. run.sh puts it at /init. It loads the
# disk drivers, waits for the test disks, runs /test/run as root, prints
# its exit code on the console for run.sh to read, and powers off.
export PATH=/bin

mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
mount -t tmpfs tmpfs /tmp

while read -r module; do
    insmod "/lib/modules/$module" || echo "init: could not load $module"
done </lib/modules/order

# run.sh passes the number of test disks it attached. The disk driver finds
# them after loading, so wait for that many, ten seconds at most.
expected=0
read -r cmdline </proc/cmdline
for word in $cmdline; do
    case $word in dfr.disks=*) expected=${word#dfr.disks=} ;; esac
done
# shellcheck source=SCRIPTDIR/guard.sh
. /test/guard.sh
tries=0
while [ "$(test_disks | wc -l)" -lt "$expected" ] && [ "$tries" -lt 100 ]; do
    sleep 0.1
    tries=$((tries + 1))
done

cd /test || exit 1
if [ "$(test_disks | wc -l)" -eq "$expected" ]; then
    sh ./run
    rc=$?
else
    echo "init: expected $expected test disk(s), found $(test_disks | wc -l)"
    rc=125
fi
sync
echo "DFR-VM-RESULT $rc"
poweroff -f
