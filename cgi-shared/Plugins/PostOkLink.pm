package Plugins::SpellCheck;

#
# \file PostOkLink.pm
# \author Christian Seiler, <self@christian-seiler.de>
# \brief a plugin to assign a link to the posted message after it was made
#
# {{{ initial comments
#
# $LastChangedDate: 2004-08-09 17:17:58 +0200 (Mon, 09 Aug 2004) $
# $LastChangedRevision: 235 $
# $LastChangedBy: cseiler $
#
# }}}

# {{{ Program headers
use strict;

sub VERSION {(q$LastChangedRevision: 235 $ =~ /([\d.]+)\s*$/)[0] or '0.0'}

use CGI;
use CGI::Carp qw/fatalsToBrowser/;

use CheckRFC;
use ForumUtils qw(
  generate_unid
  get_error
  decode_params
  message_field
  transform_body
  plaintext
  gen_time
  recode
  encode
);
# }}}

push @{$main::Plugins->{display}},\&execute;

sub execute {
  my ($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi,$tpl) = @_;

  return unless $cgi->param('new_tid');
  return unless $cgi->param('new_mid');
  
  my $new_tid = $cgi->param('new_tid');
  my $new_mid = $cgi->param('new_mid');

  my $url = $main::UserName ? $fo_default_conf->{UPostingURL}->[0]->[0] : $fo_default_conf->{PostingURL}->[0]->[0];
  $url =~ s!\%t!$new_tid!g;
  $url =~ s!\%m!$new_mid!g;
  
  $tpl->setVar('new_link' => $url);
  return;
}

1;
# eof
