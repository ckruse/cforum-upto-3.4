package Plugins::XMLRPCUserManagement;

#
# \file XMLRPCUserManagement.pm
# \author Christian Seiler, <self@christian-seiler.de>
#
# a plugin to handle xmlrpc requests for the management of local users
#

# {{{ initial comments
#
# $LastChangedDate: 2004-12-13 20:11:06 +0100 (Mon, 13 Dec 2004) $
# $LastChangedRevision: 367 $
# $LastChangedBy: ckruse $
#
# }}}

# {{{ plugin header
#

use strict;

sub VERSION {(q$Revision$ =~ /([\d.]+)\s*$/)[0] or '0.0'}

use ForumUtils qw/get_config_files get_user_config_file create_directory_structure read_configuration/;

use File::Path;

$main::methods->{'user.exists'} = \&exists_user;
$main::methods->{'user.add'} = \&add_user;
$main::methods->{'user.delete'} = \&delete_user;

# }}}

# {{{ does a user exist?
sub exists_user {
  my ($user) = @_;
  my $fo_default_conf = $main::fo_default_conf;

  my $ufile = get_user_config_file($fo_default_conf,$user);
  return (-f $ufile);
}
# }}}

# {{{ add a user
sub add_user {
  my ($user) = @_;
  my $fo_default_conf = $main::fo_default_conf;

  my $ufile = get_user_config_file($fo_default_conf,$user);

  # user exists
  return if -f $ufile;

  return unless create_directory_structure($fo_default_conf,$user);

  my $dir = $ufile;
  $dir =~ s!\.conf$!!; #!;

  mkdir $dir,0771 or return;

  my $dat;
  open $dat,'>'.$ufile or return;
  #
  # run config plugins
  #
  foreach(@{$main::Plugins->{register}}) {
    &$_($fo_default_conf,undef,undef,$dir,$ufile,$dat);
  }
  close $dat;

  return 1;
}
# }}}

# {{{ delete a user
sub delete_user {
  my ($user) = @_;
  my $fo_default_conf = $main::fo_default_conf;

  my $cfile = get_user_config_file($fo_default_conf,$user);
  return unless -f $cfile;

  my $dir   = $cfile;
  $dir =~ s!\.conf$!!; #!;

  my $user_config = read_configuration($cfile) or return;


  #
  # run plugins
  #
  $main::UserName = $user;
  foreach(@{$main::Plugins->{unregister}}) {
    &$_($main::fo_default_conf,undef,$main::fo_xmlrpc_conf,$user_config,undef);
  }

  rmtree($dir) or return;
  unlink($cfile) or return;
}
# }}}

# eof
