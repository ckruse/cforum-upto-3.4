package Plugins::CategoryCheck;

#
# \file PostingAssistant.pm
# \author Christian Kruse, <ckruse@wwwtech.de>
#
# a plugin to guide the user to write a 'nice' posting
#

# {{{ initial comments
#
# $LastChangedDate$
# $LastChangedRevision$
# $LastChangedBy$
#
# }}}

use strict;

sub VERSION {(q$Revision$ =~ /([\d.]+)\s*$/)[0] or '0.0'}

use CGI;
use CGI::Carp qw/fatalsToBrowser/;

use ForumUtils qw(
  get_error
  rel_uri
);

push @{$main::Plugins->{newthread}},\&execute;
push @{$main::Plugins->{newpost}},\&execute;

sub execute {
  my ($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi) = @_;

  my $cat        = $cgi->param('cat');
  return if 0 < grep { $cat eq $_->[0] } @{$fo_default_conf->{Category}};

  main::show_form($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi,get_error($fo_default_conf,'posting','category'));
  exit(0);
}

1;
# eof
