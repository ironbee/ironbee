#!/bin/sh

# gpg alone isn't good enough: see
# http://bahumbug.wordpress.com/2014/08/01/scripting-with-gpg/

# Workaround snarfed from
# http://serverfault.com/questions/293101/validating-signature-trust-with-gpg
# in the absence of a better solution

tmpfile=$(mktemp gpgverifyXXXXXX)
trap "rm -f $tmpfile" EXIT

# Automatically verify as much as we can
gpg --status-file $tmpfile --verify "$@" || exit 1
egrep -q '^\[GNUPG:] TRUST_(ULTIMATE|FULLY)' $tmpfile || exit 1

# If it's signed by "Evil Hacker <evil.hacker@nsa.gov>"
# give the user a chance to check.

echo
echo "======= SECURITY ========"
echo
echo "We have just downloaded nginx and verified its PGP signature."
echo "Please confirm that you trust the signatory as shown above"
/bin/echo -n "Signature trusted [Yn]? ==> "
read yn
[ "x$yn" = "xY" ] || [ "x$yn" = "xy" ] || [ "x$yn" = "x" ] || exit 1
exit 0
