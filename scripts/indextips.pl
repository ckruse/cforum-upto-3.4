#!/usr/bin/perl -w

use strict;

use Getopt::Long;

our $TIPS_FILE  = 'tipoftheday.txt';
our $INDEX_FILE = 'tipoftheday.idx';
our $help = 0;

GetOptions(
  "tips-file=s" => \$messagefile_in,
  "tips-index=s" => \$messagefile_out,
  "help" => \$help
)

usage() if $help;

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

sub usage {
  print <<TXT;
USAGE:
  $0 [options]

Options are:
  --tips-file=file      Path to the tips file
  --tips-index=file     Path to the tips index file
  --help                Show this help screen

TXT

  exit;
}


# eof
