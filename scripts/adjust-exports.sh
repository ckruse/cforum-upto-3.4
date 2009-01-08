#!/bin/sh

# Adjust exports of SWIG-generated Perl modules

PERL=$1
FILE=$2
shift
shift
QW="$*"

${PERL} -pi -e "s!\@EXPORT = qw\(\s*\);!\@EXPORT_OK = qw($QW);!g" $FILE
