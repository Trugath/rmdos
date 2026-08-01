/*
 * LEASE.DAT layout constants (include before code that uses them).
 * File I/O helpers live in netlease.inc — include that after _start.
 */

.ifndef LEASE_SIZE
.set LEASE_SIZE, 24
.endif
.set LEASE_OFF_VER, 4
.set LEASE_OFF_YIADDR, 8
.set LEASE_OFF_GW, 12
.set LEASE_OFF_MASK, 16
.set LEASE_OFF_DNS, 20
.set LEASE_VER, 1
