package Plugins::CategoryCheck;

#
# \file PostingAssistant.pm
# \author Christian Kruse, <ckruse@wwwtech.de>
#
# a plugin to guide the user to write a 'nice' posting
#

# {{{ initial comments
#
# $LastChangedDate: 2003-12-05 20:18:43 +0100 (Fri, 05 Dec 2003) $
# $LastChangedRevision: 20 $
# $LastChangedBy: ckruse $
#
# }}}

use strict;

sub VERSION {(q$Revision: 1.9 $ =~ /([\d.]+)\s*$/)[0] or '0.0'}

use CGI;
use CGI::Carp qw/fatalsToBrowser/;

use CheckRFC;
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
