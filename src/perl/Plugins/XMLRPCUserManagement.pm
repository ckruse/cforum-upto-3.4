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

use ForumUtils qw/read_configuration get_template get_error recode uniquify_params get_config_file create_directory_structure fatal/;

use File::Path;
use CGI;
use CGI::Carp qw/fatalsToBrowser/;

$main::methods->{'user.exists'} = \&exists_user;
$main::methods->{'user.add'} = \&add_user;
$main::methods->{'user.delete'} = \&delete_user;

# }}}

# {{{ does a user exist?
sub exists_user {
  my ($user) = @_;
  my $fo_default_conf = $main::fo_default_conf;
  
  my $ufile = get_config_file($fo_default_conf,$user);
  return (-f $ufile);
}
# }}}

# {{{ add a user
sub add_user {
  my ($user) = @_;
  my $fo_default_conf = $main::fo_default_conf;
  
  my $ufile = get_config_file($fo_default_conf,$user);
  # user exists
  if(-f $ufile) {
    return;
  }
  
  if(!create_directory_structure($fo_default_conf,$user)) {
    return;
  }
  
  my $dir = $ufile;
  $dir =~ s!\.conf$!!; #!;

  mkdir $dir,0771 or return;
  
  open DAT,'>'.$ufile or return;
  print DAT 'DeletedFile "',$dir,"/dt.dat\"\n";
  print DAT 'VisitedFile "',$dir,"/vt.dat\"\n";
  close DAT;
  
  return 1;
}
# }}}

# {{{ delete a user
sub delete_user {
  my ($user) = @_;
  my $fo_default_conf = $main::fo_default_conf;
  
  my $cfile = get_config_file($fo_default_conf,$user);
  if(!-f $cfile) {
    return 0;
  }
  my $dir   = $cfile;
  $dir =~ s!\.conf$!!; #!;

  rmtree($dir) or return;
  unlink($cfile) or return;
}
# }}}

# eof