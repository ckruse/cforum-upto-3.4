package Plugins::Pathes;

#
# \file CheckRegisteredName.pm
# \author Christian Kruse, <cjk@wwwtech.de>
#
# a plugin to register usernames
#

# {{{ initial comments
#
# $LastChangedDate: 2009-01-16 14:32:24 +0100 (Fri, 16 Jan 2009) $
# $LastChangedRevision: 1639 $
# $LastChangedBy: ckruse $
#
# }}}

# {{{ plugin header
use strict;

sub VERSION {(q$Revision: 1639 $ =~ /([\d.]+)\s*$/)[0] or '0.0'}

use IO::Socket;

push @{$main::Plugins->{register}},\&execute;
# }}}

sub execute {
  my $fo_default_conf = shift;
  my $fo_usermanagement_conf = shift;
  my $user_config = shift;
  my $uconf_dir = shift;
  my $uconf_file = shift;
  my $fh = shift;

  print $fh 'DeletedFile "',$uconf_dir,"/dt.dat\"\n";
  print $fh 'VisitedFile "',$uconf_dir,"/vt.dat\"\n";
  print $fh 'MailUserDB "',$uconf_dir,"/ml.dat\"\n";
  print $fh 'OcDbFile "',$uconf_dir,"/oc.dat\"\n";
  print $fh 'InterestingFile "',$uconf_dir,"/it.dat\"\n";
}


1;
# eof
