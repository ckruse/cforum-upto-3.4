#!/usr/bin/perl -w

use strict;

our $TIPS_FILE  = 'cgi-shared/tipoftheday.txt';
our $INDEX_FILE = 'cgi-shared/tipoftheday.idx';

open DAT,'<',$TIPS_FILE or die "$TIPS_FILE: $!";

my $start = 0;
my @lines = ();

while(<DAT>) {
  push @lines,$start;
  $start = tell(DAT);
}

close DAT;

open IDX,'>',$INDEX_FILE or die "$INDEX_FILE: $!";
binmode IDX;
print IDX pack("L",scalar @lines);

foreach my $offset (@lines) {
  print IDX pack("L",$offset);
}

close IDX;

# eof
