#!/usr/bin/perl -w

#
# this script generates template C files
#
# $Header: /home/users/cvs/selfforum/template_gen.pl,v 1.20 2003/10/20 08:57:39 ckruse Exp $
# $Log: template_gen.pl,v $
# Revision 1.20  2003/10/20 08:57:39  ckruse
# Heavy developement
#
# Revision 1.19  2003/09/15 07:03:22  ckruse
# Developement in Progress...
#
# Revision 1.18  2003/09/05 11:42:30  ckruse
# merged cforum-2_0_beta_3 and main branch
#
# Revision 1.17.2.3  2003/08/21 01:09:29  ckruse
# Internationalization for the classic forum....
#
# Revision 1.17.2.2  2003/08/17 17:37:47  ckruse
# Lots of bugfixes, lot of developement. Entire version is not runnable
#
# Revision 1.17.2.1  2003/06/05 19:41:37  ckruse
# bugfix
#
# Revision 1.17  2003/05/29 13:19:22  ckruse
# some bugfixes & performance enhancements
#
# Revision 1.16  2003/04/27 14:28:58  ckruse
# ----------------------------------------------------------------------
#
# developement in progress...
#
# Revision 1.15  2002/11/24 23:42:45  ckruse
# update of the build-system, several bugfixes
#
# Revision 1.14  2002/11/23 18:58:23  ckruse
# the template lib now uses the hashlib
#
# Revision 1.13  2002/11/19 06:47:32  ckruse
# changed template api to avoid conflicts
#
# Revision 1.12  2002/06/27 17:50:17  ckruse
# partitial implementation, bugfixes, categories are now in fo_default.conf
#
# Revision 1.11  2002/06/19 18:11:05  ckruse
# changed headers
#
# Revision 1.10  2002/06/19 18:09:03  ckruse
# changed header
#
#


use strict;

# forwards
sub treat_dir($);
sub treat_file($);

if(@ARGV < 1) {
  print "Usage:\n\t$0 [templatefile|directory]\n";
  exit 0;
}

foreach(@ARGV) {
  if(-d $_) {
    treat_dir($_);
  }
  else {
    treat_file($_);
  }
}

sub treat_dir($) {
  my $directory = shift;

  opendir DIR,$directory or die $!;

  foreach(readdir DIR) {
    next if $_ eq '.' or $_ eq '..';

    if(-d "$directory/$_") {
      treat_dir("$directory/$_");
      next;
    }

    next unless /\.html?$/;

    treat_file("$directory/$_");
  }

  closedir DIR;
}

sub get_length($) {
  my $str = shift;

  $str =~ s/\\\\/\\/g;
  $str =~ s/\\"/"/g;
  $str =~ s/%%/%/g;
  $str =~ s/\\n/\n/g;
  $str =~ s/\\r/\r/g;

  return length($str);
}

sub treat_file($) {
  my $file	= shift;
  my $fout	= $file;
  my %variables = ();
  my $date	= scalar localtime;
  my $fkt	= 0;
  local $/;

  $fout =~ s/\.html?$/\.c/;

  my $cont = <<C;
/*
 * this is a template file
 *
 * generated: $date
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "defines.h"

#include "utils.h"
#include "hashlib.h"
#include "charconvert.h"
#include "template.h"

static void my_write(const u_char *s) {
  register u_char *ptr;

  for(ptr = (u_char *)s;*ptr;ptr++) {
    fputc(*ptr,stdout);
  }
}

C

  my $parse_head     = "void parse(t_cf_template *tpl) {\n\n";
  my $parse_mem_head = "void parse_to_mem(t_cf_template *tpl) {\n\n";
  my $parse          = '';
  my $parse_mem      = '';

  open IN,"<$file" or die "could not open $file: $!";
  my $parsed    = <IN>;
  close IN;

  $parsed =~ s~\\~\\\\~g;
  $parsed =~ s~\015~\\r~g;
  $parsed =~ s~"~\\"~g;
  $parsed =~ s~^(.*)$~my_write(\"$1\\n\");~mg;
  $parsed =~ s~\{if \$(\w+)\}~");\nif(tpl_cf_getvar(tpl,"$1")) {\n my_write("~g;
  $parsed =~ s~\{if !\$(\w+)\}~");\nif(!tpl_cf_getvar(tpl,"$1")) {\n my_write("~g;
  $parsed =~ s~\{if \$(\w+)\s*==\s*\\"(.+?)\\"\}~");\nv = (t_cf_tpl_variable *)tpl_cf_getvar(tpl,"$1");\n if(v && cf_strcmp(v->data->content,"$2") == 0) {\n my_write("~g;
  $parsed =~ s~\{else\}~");\n}\nelse {\n my_write("~g;
  $parsed =~ s~\{endif\}~");\n}\nmy_write("~g;
  $parsed =~ s~\{\$(\w+)\}~");\nv = (t_cf_tpl_variable *)tpl_cf_getvar(tpl,\"$1\");\n if(v) {\n if(v->escape_html) print_htmlentities_encoded(v->data->content,0,stdout);\n else my_write(v->data->content);\n}\nmy_write("~g;
  $parsed =~ s~"\);\s*my_write\("~~sg;
  $parsed =~ s~my_write\(""\);~~g;

  $parse .= "$parsed\n}\n";

  $parse_mem .= "$parsed\n}\n";
  $parse_mem =~ s~my_write\("(.*?[^\\])"\)~"str_chars_append(&tpl->parsed,\"".$1."\",".get_length($1).")"~eg;
  $parse_mem =~ s~print_htmlentities_encoded\(.*?\);~{\ntmp = htmlentities(v->data->content,0);\n str_chars_append(&tpl->parsed,tmp,strlen(tmp));\n free(tmp)\n;}~g;
  $parse_mem =~ s~my_write\(v->data->content\)~str_chars_append(&tpl->parsed,v->data->content,v->data->len)~g;

  $parse     = $parse_head.($parse =~ /v = \(t_cf_tpl_variable \*\)tpl_cf_getvar/ ? "t_cf_tpl_variable *v = NULL;\n\n" : '').($parse =~ /tmp =/ ? "u_char *tmp = NULL;\n" : '').$parse;
  $parse_mem = $parse_mem_head.($parse_mem =~ /v = \(t_cf_tpl_variable \*\)tpl_cf_getvar/ ? "t_cf_tpl_variable *v = NULL;\n\n" : '').($parse_mem =~ /tmp = / ? "u_char *tmp = NULL;\n" : '').$parse_mem;

  open OUT,">$fout" or die "could not open $fout: $!";
  print OUT "$cont\n\n$parse\n\n$parse_mem\n\n/* eof */\n";
  close OUT;
}

# eof
