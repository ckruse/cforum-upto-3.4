package Plugins::Variables;

#
# \file Variables.pm
# \author Christian Kruse, <ckruse@wwwtech.de>
#
# a plugin to set some posting variables
#

# {{{ initial comments
#
# $LastChangedDate$
# $LastChangedRevision$
# $LastChangedBy$
#
# }}}

use strict;

use CGI;
use CGI::Carp qw/fatalsToBrowser/;

use ForumUtils qw/recode/;

push @{$main::Plugins->{display}},\&set_variables;

sub set_variables {
  my ($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi,$tpl) = @_;

  if(!defined $cgi->param('newbody')) {
    if(defined $cgi->param('body')) {
      $tpl->setVar(body => recode($fo_default_conf,$cgi->param('body')));
      $cgi->delete('body');
    }
    else {
      my $body  = '';
      my $sig   = exists $user_config->{Signature} ? $user_config->{Signature}->[0]->[0] : '';
      my $bye   = exists $user_config->{Bye} ? $user_config->{Bye}->[0]->[0] : '';
      my $hi    = exists $user_config->{Hi} ? $user_config->{Hi}->[0]->[0] : '';

      $bye =~ s/\\n/\015\012/g;
      $sig =~ s/\\n/\015\012/g;
      $hi  =~ s/\\n/\015\012/g;
      $hi  =~ s/\{\$name\}/alle/g;

      $body .= recode($fo_default_conf,$hi) if $hi;
      $body .= recode($fo_default_conf,$bye) if $bye;
      $body .= "\n-- \n".recode($fo_default_conf,$sig) if $sig;

      $tpl->setVar(body => $body);
    }
  }

  if(exists $user_config->{PreviewSwitchType}) {
    $tpl->setVar('previewswitchtype',$user_config->{PreviewSwitchType}->[0]->[0] || 'checkbox');
  }
  else {
    $tpl->setVar('previewswitchtype',$fo_view_conf->{PreviewSwitchType}->[0]->[0] || 'checkbox');
  }

  $tpl->setVar('Name',recode($fo_default_conf,$user_config->{Name}->[0]->[0])) if exists $user_config->{Name} && !defined $cgi->param('Name');
  $tpl->setVar('EMail',recode($fo_default_conf,$user_config->{EMail}->[0]->[0])) if exists $user_config->{EMail} && !defined $cgi->param('EMail');
  $tpl->setVar('ImageUrl',recode($fo_default_conf,$user_config->{ImageUrl}->[0]->[0])) if exists $user_config->{ImageUrl} && !defined $cgi->param('ImageUrl');
  $tpl->setVar('HomepageUrl',recode($fo_default_conf,$user_config->{HomepageUrl}->[0]->[0])) if exists $user_config->{HomepageUrl} && !defined $cgi->param('HomepageUrl');

  if($user_config->{TextBox}) {
    $tpl->setVar('twidth',$user_config->{TextBox}->[0]->[0]) if $user_config->{TextBox}->[0]->[0];
    $tpl->setVar('theight',$user_config->{TextBox}->[0]->[1]) if $user_config->{TextBox}->[0]->[1];
  }

  if($user_config->{FontSize} || $user_config->{FontFamily} || $user_config->{FontColor} || $user_config->{QuoteColor}) {
    $tpl->setVar('font','1');

    $tpl->setVar('fontsize',$user_config->{FontSize}->[0]->[0]) if $user_config->{FontSize}->[0]->[0];
    $tpl->setVar('fontfamily',recode($fo_default_conf,$user_config->{FontFamily}->[0]->[0])) if $user_config->{FontFamily}->[0]->[0];
    $tpl->setVar('fontcolor',recode($fo_default_conf,$user_config->{FontColor}->[0]->[0])) if $user_config->{FontColor}->[0]->[0];

    $tpl->setVar('qcolor',recode($fo_default_conf,$user_config->{QuoteColor}->[0]->[0])) if $user_config->{QuoteColor}->[0]->[0];
    $tpl->setVar('qcolorback',recode($fo_default_conf,$user_config->{QuoteColor}->[0]->[1])) if $user_config->{QuoteColor}->[0]->[1];
  }

  $tpl->setVar('owncss',recode($fo_default_conf,$user_config->{OwnCSSFile}->[0]->[0])) if $user_config->{OwnCSSFile};
}

1;
# eof
