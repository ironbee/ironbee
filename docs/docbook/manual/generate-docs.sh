#!/bin/sh

set -x
HOME=${1:-.}
OBJDIR=${2:-.}

FOP=$HOME/fop-1.0/fop
XMLINPUT=$HOME/ironbee-reference-manual.xml
OUTPUT=$OBJDIR/output
VERSION="0.3.0"

# Generate a clean directory structure
rm -rf $OUTPUT
mkdir $OUTPUT || exit 1
mkdir $OUTPUT/html || exit 1
mkdir $OUTPUT/html-chunked || exit 1

# Generate PDF
$FOP -c $OBJDIR/fop.xconf -xml $XMLINPUT -xsl $HOME/pdf.xsl -pdf $OUTPUT/ironbee-reference-manual.pdf || exit 1

# Generate HTML/single
$FOP -xml $XMLINPUT -xsl $HOME/html.xsl -foout /dev/null -param base.dir $OUTPUT/ || exit 1
mv $OUTPUT/index.html $OUTPUT/ironbee-reference-manual.html || exit 1
# XXX Copy extra files to $OUTPUT

# Generate HTML/chunked
$FOP -xml $XMLINPUT -xsl $HOME/html-chunked.xsl -foout /dev/null -param base.dir $OUTPUT/html-chunked/ || exit 1
# XXX Copy extra files to $OUTPUT/html-chunked/

# Cover page
cp $HOME/resources/index.html $OUTPUT/index.html || exit 1
perl -pi -e s/\\\$version/$VERSION/ $OUTPUT/index.html || exit 1

