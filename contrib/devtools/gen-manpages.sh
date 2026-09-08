#!/bin/sh

TOPDIR=${TOPDIR:-$(git rev-parse --show-toplevel)}
SRCDIR=${SRCDIR:-$TOPDIR/src}
MANDIR=${MANDIR:-$TOPDIR/doc/man}

XCOIND=${XCOIND:-$SRCDIR/xcoind}
RAVENCLI=${RAVENCLI:-$SRCDIR/xcoin-cli}
RAVENTX=${RAVENTX:-$SRCDIR/xcoin-tx}
RAVENQT=${RAVENQT:-$SRCDIR/qt/xcoin-qt}

[ ! -x $XCOIND ] && echo "$XCOIND not found or not executable." && exit 1

# The autodetected version git tag can screw up manpage output a little bit
RVNVER=($($RAVENCLI --version | head -n1 | awk -F'[ -]' '{ print $6, $7 }'))

# Create a footer file with copyright content.
# This gets autodetected fine for xcoind if --version-string is not set,
# but has different outcomes for xcoin-qt and xcoin-cli.
echo "[COPYRIGHT]" > footer.h2m
$XCOIND --version | sed -n '1!p' >> footer.h2m

for cmd in $XCOIND $RAVENCLI $RAVENTX $RAVENQT; do
  cmdname="${cmd##*/}"
  help2man -N --version-string=${RVNVER[0]} --include=footer.h2m -o ${MANDIR}/${cmdname}.1 ${cmd}
  sed -i "s/\\\-${RVNVER[1]}//g" ${MANDIR}/${cmdname}.1
done

rm -f footer.h2m
