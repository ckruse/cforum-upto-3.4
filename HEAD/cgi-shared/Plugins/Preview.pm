package Plugins::Preview;

#
# \file Preview.pm
# \author Christian Kruse, <ckruse@wwwtech.de>
# \brief a plugin to generate a preview
#
# This plugin allows the user to generate a preview
# of his posting
#

# {{{ initial comments
#
# $LastChangedDate$
# $LastChangedRevision$
# $LastChangedBy$
#
# }}}

# {{{ Program headers
use strict;

sub VERSION {(q$Revision: 1.6 $ =~ /([\d.]+)\s*$/)[0] or '0.0'}

use CGI;
use CGI::Carp qw/fatalsToBrowser/;

use Unicode::MapUTF8 qw(from_utf8);

use CheckRFC;
use ForumUtils qw(
  generate_unid
  get_error
  decode_params
  message_field
  transform_body
  gen_time
  recode
  encode
);
# }}}

push @{$main::Plugins->{newpost}},\&execute;
push @{$main::Plugins->{newthread}},\&execute;

sub execute {
  my ($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi) = @_;
  return unless $cgi->param('preview');

  my $body = message_field(from_utf8(-string => $cgi->param('newbody'),-charset => $fo_default_conf->{ExternCharset}->[0]->[0]),$cgi->param('qchar'),$fo_default_conf);

  $cgi->param('date',gen_time(time,$fo_default_conf,$fo_view_conf->{DateFormatThreadView}->[0]->[0]));
  $cgi->param('ne_message',$body);
  $cgi->param('genprev',1) if $cgi->param('preview');
  $cgi->param('preview',0) if $cgi->param('preview');

  main::show_form($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi);

  exit(0);
}

1;
# eof
