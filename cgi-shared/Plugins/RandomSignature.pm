package Plugins::RandomSignature;

#
# \file RandomSignature.pm
# \author Christian Kruse, <ckruse@wwwtech.de>
# \brief randomize signatures
#
# This plugin gives the user the ability to use random signatures. The signature has
# to be plain UTF-8 encoded text!
#

# {{{ initial comments
#
# $LastChangedDate$
# $LastChangedRevision$
# $LastChangedBy$
#
# }}}

use strict;

use CheckRFC;
use ForumUtils qw/transform_body/;

use LWP::Simple;

our $VERSION = (q$Revision: 1.2 $ =~ /([\d.]+)\s*$/)[0] or '0.0';

push @{$main::Plugins->{newthread}},\&execute;
push @{$main::Plugins->{newpost}},\&execute;

sub execute {
  my ($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi) = @_;
  my $cl = new CForum::Clientlib;

  my $body = $cgi->param('newbody');

  while($body =~ m!\[remote-signature:(http://[^\]]+)\]!) {
    my $uri = $1;
    if(is_URL($uri)) {
      my $sig = '';

      if($sig = LWP::Simple::get($uri)) {
        if($cl->is_valid_utf8_string($sig,length $sig)) {
          $sig = transform_body($fo_default_conf,$fo_post_conf,$sig,$cgi->param('qchar'));

          $uri  = quotemeta $uri;
          $body =~ s!\[remote-signature:$uri\]!$sig!;
        }
      }
    }
  }

  $cgi->param(newbody => $body);
}

1;

# eof
