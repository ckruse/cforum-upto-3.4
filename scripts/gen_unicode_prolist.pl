#!/usr/bin/perl -w

use strict;

my $input_file = $ARGV[0];

open FD,"<$input_file" or die "Failure opening $input_file: $!";

my $function = "int cf_char_property(u_int32_t c) {\n";
my %properties = ();

while(my $line = <FD>) {
  next if $line =~ /^\s*#/;
  $line =~ s/\s*#.*$//g;
  $line =~ s/\s+//g;

  next unless $line =~ /^([a-fA-F0-9]{4,6})(?:\.\.([a-fA-F0-9]{4,6}))?;(\w+)$/;

  my $start = $1;
  my $end = $2;
  my $prop = $3;

  $start =~ s/^0+//;
  $end =~ s/^0+// if $end;
  $prop = uc $prop;

  $start = '0' if !$start;

  $properties{uc $prop} = 1;

  if($end) {
    $function .= <<C;
  if(c >= 0x$start && c <= 0x$end) return CF_UNI_PROP_$prop;
C
  }
  else {
    $function .= <<C;
  if(c == 0x$start) return CF_UNI_PROP_$prop;
C
  }

}

$function .= "  return -1; /* not known */\n}\n\n/* eof */\n";

my $i = 0;
foreach my $prop (keys %properties) {
  print "#define CF_UNI_PROP_",$prop,"  ",++$i,"\n";
}

print "\n\n",$function;

close FD;

# eof
