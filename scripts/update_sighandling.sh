#!/bin/bash

dir="$@"
if test -z "$dir"; then
  dir="./";
fi

echo "Working on $dir..."
find "$dir" -name "t*.xml" -print0 | xargs -0 perl -pi -e 's!_/_SIG_/_!<br />-- <br />!g'
echo "done!"

