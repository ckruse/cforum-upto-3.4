package Plugins::PostingAssistant;

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

  my $score = 0;
  my $must  = $fo_post_conf->{PostingAssistantMustValidate}->[0]->[0] eq 'yes';
  my $qmust = $fo_post_conf->{QuoteMustValidate}->[0]->[0] eq 'yes';

  if(($cgi->param('assicheck') && $must) || !$cgi->param('assicheck')) {
    $score = general_checks($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi);
  }

  if($score < 0) {
    $cgi->param('assicheck','1');
    main::show_form($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi,get_error($fo_default_conf,'posting','format'));
    exit(0);
  }

  if(badwords_check($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi) < 0) {
    $cgi->param('assicheck','1');
    main::show_form($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi,get_error($fo_default_conf,'posting','badwords'));
    exit(0);
  }

  if(($cgi->param('assicheck') && $qmust) || !$cgi->param('assicheck')) {
    if(quoting_check($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi) < 0) {
      $cgi->param('assicheck','1');
      main::show_form($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi,get_error($fo_default_conf,'posting','quoting'));
      exit(0);
    }
  }

  if(!$cgi->param('assicheck') && ($user_config->{CheckForLinks}->[0]->[0] || '') eq 'yes') {
    if(links_check($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi) == -1) {
      $cgi->param('assicheck','1');
      main::show_form($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi,get_error($fo_default_conf,'posting','links'));
      exit(0);
    }
  }

}

sub links_check {
  my ($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi) = @_;

  my $body = $cgi->param('body');
  my $base = $ENV{SCRIPT_NAME};

  $base =~ s![^/]*$!!;

  my @links = ();
  push @links,[$1, $2] while $body =~ /\[([Ll][Ii][Nn][Kk]):\s*([^\]\s]+)\s*\]/g;
  @links = grep {
    !(is_URL($_->[1] => qw(http ftp news nntp telnet gopher mailto))
      or is_URL(($_->[1] =~ /^[Vv][Ii][Ee][Ww]-[Ss][Oo][Uu][Rr][Cc][Ee]:(.+)/)[0] || '' => 'http')
      or ($_->[1] =~ m<^(?:\.?\.?/(?!/)|\?)> and is_URL(rel_uri($_ -> [1],$base) => 'http')))
  } @links;

  return -1 if @links;

  # lets collect all images
  my @images = ();
  push @images, [$1, $2] while $body =~ /\[([Ii][Mm][Aa][Gg][Ee]):\s*([^\]\s]+)\s*\]/g;
  @images = grep {
    !(is_URL($_->[1] => 'strict_http')
      or ($_->[1] =~ m<^(?:\.?\.?/(?!/)|\?)> and is_URL(rel_uri($_->[1], $base) => 'http')))
  } @images;

  return -1 if @images;

  # lets collect all iframes
  my @iframes;
  push @iframes,[$1, $2] while $body =~ /\[([Ii][Ff][Rr][Aa][Mm][Ee]):\s*([^\]\s]+)\s*\]/g;
  @iframes = grep {
    !(is_URL($_ -> [1] => 'http')
    or ($_ -> [1] =~ m<^(?:\.?\.?/(?!/)|\?)> and is_URL (rel_uri($_ -> [1], $base) => 'http')))
  } @iframes;

  return -1 if @iframes;

  return 0;
}

sub quoting_check {
  my ($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi) = @_;

  my $body   = $cgi->param('body');
  my $qchars = quotemeta $cgi->param('qchar');
  my $lines  = () = $body =~ /(\n)/sg;
  my $qlines = () = $body =~ /^$qchars/mg;

  if($lines > 0) {
    my $percent = $qlines * 100 / $lines;

    if($percent >= $fo_post_conf->{QuotingPercent}->[0]->[0]) {
      return -1;
    }

    return 1;
  }

  if($qlines) {
    return -1;
  }
  else {
    return 1;
  }
}

sub badwords_check {
  my ($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi) = @_;

  # score is initialized with the number of badwords we allow
  my $score   = $fo_post_conf->{BadwordsAllowed}->[0]->[0];

  my $name    = $cgi->param('Name');
  my $body    = $cgi->param('body');
  my $subject = $cgi->param('subject');
  my $email   = $cgi->param('EMail');

  $body =~ s/\s+/ /sg;
  $body =~ y/_-//d;

  my @badwords = map { quotemeta $_ } split /,/,$fo_post_conf->{BadWords}->[0]->[0];
  foreach(@badwords) {
    $score -= 1 if $body    =~ /$_/i;
    $score -= 3 if $subject =~ /$_/i;
    $score -= 3 if $name    =~ /$_/i;
    $score -= 1 if $email   =~ /$_/i;
  }

  return $score;
}

sub general_checks {
  my ($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi) = @_;

  my $subject = $cgi->param('subject');
  my $name    = $cgi->param('Name');
  my $body    = $cgi->param('body');
  my $email   = $cgi->param('EMail');

  my $score   = $fo_post_conf->{FormateDeficitesAllowed}->[0]->[0];;

  #
  # check subject
  #
  if($subject =~ /([^.!?;])\1{2,}/) {
    $score -= 3;
  }

  if($subject =~ /([!.?;])\1+/) {
    $score -= 3;
  }

  if(uc $subject eq $subject) {
    $score -= 3;
  }
  if(lc $subject eq $subject) {
    $score -= 0.5;
  }

  #
  # check name
  #
  if($name =~ /([^.!?;])\1{2,}/) {
    $score -= 2;
  }

  if($name =~ /([!.?;])\1+/) {
    $score -= 2;
  }

  if(uc $name eq $name) {
    $score -= 3;
  }

  if(lc $name eq $name) {
    $score -= 0.5;
  }

  $score -= 1 unless $email;

  #
  # check body
  #
  if($body =~ s/\n//g < 2) {
    $score -= 3;
  }
  if($body !~ /[!.?;,:]/) {
    $score -= 3;
  }
  if(lc $body eq $body) {
    $score -= 2;
  }
  if(uc $body eq $body) {
    $score -= 3;
  }

  return $score;
}

1;
# eof
