package Plugins::SpellCheck;

#
# \file SpellCheck.pm
# \author Christian Seiler, <self@christian-seiler.de>
# \brief a plugin to check for spelling errors
#
# This plugin allows the user to check for spelling errors
# in his posting
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

sub VERSION {(q$LastChangedRevision$ =~ /([\d.]+)\s*$/)[0] or '0.0'}

use CGI;
use CGI::Carp qw/fatalsToBrowser/;

use Unicode::MapUTF8 qw(from_utf8);

use Lingua::Ispell qw(spellcheck);

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

push @{$main::Plugins->{newpost}},\&execute;
push @{$main::Plugins->{newthread}},\&execute;

sub execute {
  my ($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi) = @_;

  return unless $cgi->param('spellcheck');
  return unless $fo_post_conf->{'SpellCheckerDictionary'};

  # set options for spell checker
  Lingua::Ispell::allow_compounds(1);
  Lingua::Ispell::use_dictionary($fo_post_conf->{'SpellCheckerDictionary'}->[0]->[0]);
  $Lingua::Ispell::options{'-T'.$fo_post_conf->{'SpellCheckerFormatterType'}->[0]->[0]} = [] if  $fo_post_conf->{'SpellCheckerFormatterType'};

  my $txt = $cgi->param ('newbody');
  $txt =~ s!<br />!\n!g;

  my $html_txt = '';
  my $cwl = 0;
  my $cpos = 0;
  my $txt_modify;
  my $fragment;

  if($cgi->param('spellcheck_ok')) {
    $txt_modify = '';

    my $start;
    my $stop;
    my %errors;
    my @sorted_errors;

    foreach($cgi->param) {
      next unless $cgi->param($_);
      next unless $_ =~ m/^spelling_[0-9]+_[0-9]+$/;

      ($start, $stop) = $_ =~ /^spelling_([0-9]+)_([0-9]+)$/;
      $errors{$start} = $stop;
    }

    @sorted_errors = sort { $a <=> $b } keys %errors;

    $cpos = 0;
    foreach(@sorted_errors) {
      $txt_modify .= substr($txt, $cpos, $_ - $cpos - 1);
      $txt_modify .= $cgi->param('spelling_'.$_.'_'.$errors{$_});
      $cpos = $_ + $errors{$_} - 1;
    }
    $txt_modify .= substr($txt, $cpos);
    $txt_modify =~ s!\n!<br />!g;

    # retransform the body...
    $cgi->param(newbody => $txt_modify);
    $cgi->param(body => plaintext($txt_modify, $cgi->param('qchar'), $fo_post_conf));

    $cgi->param('preview' => 1);

    # return to other plugins!
    return;
  }

  $txt_modify = $txt;
  # remove signature
  $txt_modify =~ s!(_/_SIG_/_.*)!" " x length($1)!gesm;
  # links, iframes, images, etc...
  $txt_modify =~ s/(\[[\w-]+:[^\]]+\])/" " x length($1)/gesm;
  # remove links and link text as link text does not allow that 
  # a combo box is displayed within it!
  $txt_modify =~ s/(<a [^>]+>.*?<\/a>)/" " x length($1)/gesm;
  # remove other tags
  $txt_modify =~ s/(<[^>]+>)/" " x length($1)/gesm;
  # remove entities (<, >, ", &)
  $txt_modify =~ s/&(amp|lt|gt|quot|nbsp);/" " x (length($1) + 2)/eg;

  $txt_modify =~ s/^(\177.*)$/" " x length($1)/meg;
  $txt_modify =~ tr/\n/ /;

  for my $spelling_error (spellcheck($txt_modify)) {
    $cwl = length $spelling_error->{'original'};
    $fragment = substr ($txt,$cpos,$spelling_error->{'offset'} - $cpos - 1);
    $fragment =~ s!\n!<br />\n!g;
    $fragment = message_field(from_utf8(-string => $fragment, -charset => $fo_default_conf->{ExternCharset}->[0]->[0]),$cgi->param('qchar'),$fo_default_conf);
    $html_txt .= $fragment;
    $cpos = $spelling_error->{'offset'} + $cwl - 1;
    $html_txt .= '<select name="spelling_'.$spelling_error->{'offset'}.'_'.$cwl.'">';
    $html_txt .= '<option>'.from_utf8(-string => $spelling_error->{'original'}, -charset => $fo_default_conf->{ExternCharset}->[0]->[0]).'</option>';
    if (defined $spelling_error->{'misses'}) {
      foreach (@{$spelling_error->{'misses'}}) {
        $html_txt .= '<option>'.from_utf8(-string => $_, -charset => $fo_default_conf->{ExternCharset}->[0]->[0]).'</option>';
      }
    }
    $html_txt .= '</select>';
  }

  $fragment = substr($txt, $cpos);
  $fragment =~ s!\n!<br />\n!g;
  $fragment = message_field(from_utf8(-string => $fragment, -charset => $fo_default_conf->{ExternCharset}->[0]->[0]),$cgi->param('qchar'),$fo_default_conf);
  $html_txt .= $fragment;

  $cgi->param('ne_orig_txt',from_utf8(-string => encode($cgi->param ('body')), -charset => $fo_default_conf->{ExternCharset}->[0]->[0]));
  $cgi->param('ne_html_txt', $html_txt);
  $cgi->param('date',gen_time(time,$fo_default_conf,$fo_view_conf->{DateFormatThreadView}->[0]->[0]));
  $cgi->param('do_spellcheck', 1);

  main::show_form($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi);

  exit(0);
}

1;
# eof
