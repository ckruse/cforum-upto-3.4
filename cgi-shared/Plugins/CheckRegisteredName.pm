package Plugins::CheckRegisteredName;

#
# \file CheckRegisteredName.pm
# \author Christian Kruse, <ckruse@wwwtech.de>
#
# a plugin to avoid fake posts

# {{{ initial comments
#
# $LastChangedDate$
# $LastChangedRevision$
# $LastChangedBy$
#
# }}}

# {{{ Program headers
use strict;

sub VERSION {(q$Revision$ =~ /([\d.]+)\s*$/)[0] or '0.0'}

use CGI;
use CGI::Carp qw/fatalsToBrowser/;

use ForumUtils qw(
  get_error
);
# }}}

push @{$main::Plugins->{conn}},\&execute;

sub execute {
  my ($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi,$sock) = @_;
  my $name = $main::UserName;

  if($name) {
    print $sock "AUTH CHECK\nName: ".$cgi->param('Name')."\nPass: ".$name."\n\n";
  }
  else {
    print $sock "AUTH CHECK\nName: ".$cgi->param('Name')."\n\n";
  }
  my $line = <$sock>;

  if($line !~ /^200/) {
    main::show_form($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi,get_error($fo_default_conf,'auth','required'));
    exit(0);
  }
}


1;
# eof

