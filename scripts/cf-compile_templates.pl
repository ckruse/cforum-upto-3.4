#!/usr/bin/perl -w

use strict;

use Getopt::Long;

my $template_dir = '';
my $template_lang = '';
my $template_theme = '';
my $libdir = '';
my $help = 0;

GetOptions(
  "dir=s" => \$template_dir,
  "lang=s" => \$template_lang,
  "name=s" => \$template_theme,
  "install=s" => \$libdir,
  "help" => \$help
);

usage() if $help;

if(!$template_dir || !$template_lang || !$template_theme || !$libdir) {
  print STDERR "You must define --dir!\n" unless $template_dir;
  print STDERR "You must define --lang!\n" unless $template_lang;
  print STDERR "You must define --name!\n" unless $template_theme;
  print STDERR "You must define --install!\n" unless $libdir;

  usage();
}

opendir DIR,$template_dir or die "could not open directory $template_dir: $!";

while(my $ent = readdir DIR) {
  next unless $ent =~ /\.html?$/;

  print "working on $template_dir/$ent...\n";

  my $wo_end = $ent;
  $wo_end =~ s/\.html?$//;

  my $so_name = "${template_lang}_${template_theme}_$ent";

  if(-f "$template_dir/$wo_end.c") {
    my $mtime_c = (stat("$template_dir/$wo_end.c"))[9];
    my $mtime_html = (stat("$template_dir/$ent"))[9];

    system "cf-parsetpl $template_dir/$ent" if $mtime_html >= $mtime_c;
  }
  else {
    system "cf-parsetpl $template_dir/$ent";
  }

  if(-f "$libdir/$so_name") {
    my $mtime_c = (stat("$template_dir/$wo_end.c"))[9];
    my $mtime_so = (stat("$libdir/$so_name"))[9];

    system "gcc -shared ".($ENV{CFLAGS}||'')." ".($ENV{LDFLAGS}||'')." -o $libdir/$so_name $template_dir/$wo_end.c -lcfutils -lcftemplate" if $mtime_c > $mtime_so;
  }
  else {
    system "gcc -shared ".($ENV{CFLAGS}||'')." ".($ENV{LDFLAGS}||'')." -o $libdir/$so_name $template_dir/$wo_end.c -lcfutils -lcftemplate";
  }
}

closedir DIR;


sub usage {
  print <<USAGE;
USAGE:
  $0 [options]

Options:
  --dir=DIR      Path to the templates (in HTML format)
  --lang=LANG    Name of the language (e.g. en, de)
  --name=NAME    Name of the theme (e.g. html4, xhtml10)
  --install=DIR  The target directory where to install the compiled templates

USAGE

  exit;
}

# eof
