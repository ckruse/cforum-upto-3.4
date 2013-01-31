package Plugins::HTTPAuth;

# \file HTTPAuth.pm
# \author Christian Kruse, <cjk@wwwtech.de>
#
# a plugin to generate a preview

# {{{ initial comments
#
# $LastChangedDate: 2009-01-16 14:32:24 +0100 (Fri, 16 Jan 2009) $
# $LastChangedRevision: 1639 $
# $LastChangedBy: ckruse $
#
# }}}

# {{{ program headers
#
use strict;

sub VERSION {(q$Revision: 1639 $ =~ /([\d.]+)\s*$/)[0] or '0.0'}

use CGI::Carp qw(fatalsToBrowser);
use ForumUtils qw(get_user_config_file get_conf_val);
# }}}

push @{$main::Plugins->{auth}},\&execute;

sub execute {
  my ($fo_default_conf,$fo_view_conf,$fo_script_conf) = @_;

  return unless get_conf_val($fo_default_conf,$main::Forum,'AuthMode') eq 'http';

  if($ENV{REMOTE_USER}) {
    if(-f get_user_config_file($fo_default_conf,$ENV{'REMOTE_USER'})) {
      $main::UserName = $ENV{REMOTE_USER};
    }
  }

  return 1;
}

1;
# eof
