#!/usr/bin/perl -w

#
# \file testsuite.pl
# \author Christian Kruse, <ckruse@wwwtech.de>
#
# This script makes some statistics for the locking mechanisms in the forum
#
# $Header: /home/users/cvs/selfforum/lock_debug_stats.pl,v 1.1 2003/09/07 15:57:50 ckruse Exp $
# $Log: lock_debug_stats.pl,v $
# Revision 1.1  2003/09/07 15:57:50  ckruse
# tool for lock debuggin
#
#
#

use strict;
use vars qw($VERSION $CONFIG %CHILDS $RUN);

$VERSION = '0.1';
$CONFIG  = {
  verbose => 0,
  logfile => 'logs/fo_errors',
  statistics => 1
};

use Getopt::Long;

GetOptions(
  $CONFIG,
  'verbose!',
  'statistics!',
  'logfile=s',
  'help' => \&help
);

my %locks;
my %lock_times;
my %waiters;

open DAT,'<'.$CONFIG->{logfile} or die "Could not open '".$CONFIG->{logfile}."': $!\n";

while(<DAT>) {
  next if m/cond_lock/;
  next unless m/^\[([^\]]+)\]:([^:]+):(\d+) PTHREAD (MUTEX|RWLOCK) (TRY|RDLOCKED|WRLOCKED|UNLOCK|LOCKED)(?: (WRLOCK|RDLOCK|LOCK|UNLOCK))? '([^']+)'(?: (\d+))?/;

  my ($date,$file,$line,$what,$happening,$type,$name,$time) = ($1,$2,$3,$4,$5,$6||'',$7,$8||0);

  print $happening,"\n" if $CONFIG->{verbose};

  if($happening eq 'TRY') {
    if($type eq 'UNLOCK') {
      delete $locks{$name};
      $lock_times{$name}->{un}--;

      warn 'You unlocked '.$name.' at file '.$file.', line '.$line." but it isn't locked!\n" if $lock_times{$name}->{un} < 0;
    }
    elsif(exists $locks{$name}) {
      warn 'You tried to lock '.$what.' "'.$name.'" at file '.$file.', line '.$line.' but it was already locked at '.$locks{$name}->{file}.', line '.$locks{$name}->{line}."\n";
    }
  }
  else {
    if($happening eq 'UNLOCK') {

    }
    elsif($happening eq 'RDLOCKED' || $happening eq 'WRLOCKED' || $happening eq 'LOCKED') {
      if(exists $locks{$name}) {
        warn "Oops. Perhaps you should ignore the error above, it seems as if the protocol is not synchronous\n";
      }
      else {
        if(exists $waiters{$name}) {
          if(exists $waiters{$name}->{$file}) {
            $waiters{$name}->{$file}->{$line}-- if exists $waiters{$name}->{$file}->{$line};
          }
        }
        $lock_times{$name} = {tm => 0,n => 0,un => 0} unless exists $lock_times{$name};

        $lock_times{$name}->{tm} += $time;
        $lock_times{$name}->{n}++;
        $lock_times{$name}->{un}++;

        $locks{$name} = {
          date => $date,
          file => $file,
          line => $line,
          what => $what,
          happening => $happening,
          type => $type,
          name => $name,
          time => $time
        };
      }
    }
  }

}

close DAT;

print <<TABLE;
     Lock name      |  times locked  |  time elapsed  | lock counter
---------------------------------------------------------------------
TABLE

foreach my $lock (sort { $lock_times{$b}->{n} <=> $lock_times{$a}->{n} } keys %lock_times) {
  print sprintf "\%19s | \%14d | \%14d | \%13d\n",$lock,$lock_times{$lock}->{n},$lock_times{$lock}->{tm},$lock_times{$lock}->{un};
}


sub help {
  print <<HELP;
Usage:
   $0 [options]

Options are:
  --help          Show this help screen
  --logfile       Define error logfile of the forum (by default logs/fo_errors)
  --verbose       Do verbose output
  --statistics    Do lock statistics (default)
  --nostatistics  Do no lock statistics

HELP

  exit(0);
}

# eof
