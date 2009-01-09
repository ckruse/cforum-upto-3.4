#!/usr/bin/perl -w

# {{{ initial comments
#
# $LastChangedDate$
# $LastChangedRevision$
# $LastChangedBy$
#
# }}}

# {{{ program header
use strict;

use BerkeleyDB;
use Getopt::Long;

sub VERSION {(q$Revision$ =~ /([\d.]+)\s*$/)[0] or '0.0'}
# }}}

my $messagefile_in = '';
my $messagefile_out = '';
my $lang = '';
my $help = 0;

GetOptions(
  "message-file=s" => \$messagefile_in,
  "message-db=s" => \$messagefile_out,
  "lang=s" => \$lang,
  "help" => \$help
);

usage() if $help;

if(!$messagefile_in || !$messagefile_out) {
  print STDERR "set --message-file!\n" unless $messagefile_in;
  print STDERR "set --message-db!\n" unless $messagefile_out;

  usage();
}

my $db = new BerkeleyDB::Btree(
  -Filename => $messagefile_out,
	-Flags => DB_CREATE
) or die "BDB error: $!";

if(!$lang) {
  if($messagefile_in !~ /\.([a-z]{2})\./) {
    print STDERR "Could not get language! Please set by --lang!\n";
    exit -1;
  }

  $lang = $1;
}

print "Working on $messagefile_in (language: $lang)\n";
open DAT,'<',$messagefile_in or die "$messagefile_in: $!";

while(<DAT>) {
  next unless m/^([a-zA-Z0-9_]+): (.*)\n/;
  my ($key,$value) = ($1,$2);
  $db->db_put("${lang}_$key",$value);
}

close DAT;
undef $db;

sub usage {
  print <<TXT;
USAGE:
  $0 [options]

Options are:
  --message-file=file   Path to the messages file
  --message-db=file     Path to the message database
  --lang=lang           Name of the language (e.g. de, en)
  --help                Show this help screen

TXT

  exit -1;
}

# eof
