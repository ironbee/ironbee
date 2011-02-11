#!/bin/sh

HOME=.
FOP=$HOME/fop-1.0/fop
XMLINPUT=ironbee-reference-manual.xml
OUTPUT=./output
VERSION="0.1.0"

# Generate a clean directory structure
rm -rf $OUTPUT
mkdir $OUTPUT
mkdir $OUTPUT/html
mkdir $OUTPUT/html-chunked

# Generate PDF
$FOP -xml $XMLINPUT -xsl $HOME/pdf.xsl -pdf $OUTPUT/ironbee_reference_manual.pdf

# Generate HTML/single
$FOP -xml $XMLINPUT -xsl $HOME/html.xsl -foout /dev/null -param base.dir $OUTPUT/
mv $OUTPUT/index.html $OUTPUT/ironbee-reference-manual.html
# XXX Copy extra files to $OUTPUT

# Generate HTML/chunked
$FOP -xml $XMLINPUT -xsl $HOME/html-chunked.xsl -foout /dev/null -param base.dir $OUTPUT/html-chunked/
# XXX Copy extra files to $OUTPUT/html-chunked/

# Cover page
cp $HOME/resources/index.html $OUTPUT/index.html
perl -pi -e s/\\\$version/$VERSION/ $OUTPUT/index.html

