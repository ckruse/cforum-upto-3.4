package Plugins::HTTPAuth;

# \file HTTPAuth.pm
# \author Christian Kruse, <ckruse@wwwtech.de>
#
# a plugin to generate a preview

# {{{ initial comments
#
# $LastChangedDate$
# $LastChangedRevision$
# $LastChangedBy$
#
# }}}

# {{{ program headers
#
use strict;

sub VERSION {(q$Revision: 1.1 $ =~ /([\d.]+)\s*$/)[0] or '0.0'}

use CGI::Carp qw(fatalsToBrowser);
use ForumUtils qw(get_config_file);
# }}}

push @{$main::Plugins->{auth}},\&execute;

sub execute {
  my ($fo_default_conf,$fo_view_conf,$fo_script_conf) = @_;

  return unless $fo_default_conf->{AuthMode}->[0]->[0] eq 'http';

  if($ENV{REMOTE_USER}) {
    if(-f get_config_file($fo_default_conf,$ENV{'REMOTE_USER'})) {
      $main::UserName = $ENV{REMOTE_USER};
    }
  }

  return 1;
}

1;
# eof
