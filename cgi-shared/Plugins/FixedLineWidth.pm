package Plugins::FixedLineWidth;

#
# \file FixedLineWidth.pm
# \author Christian Kruse, <ckruse@wwwtech.de>
# \brief Fix line length
#
# This plugin gives the user the ability to fix the line length.
# Wrapping will be done automatically
#

# {{{ initial comments
#
# $LastChangedDate$
# $LastChangedRevision$
# $LastChangedBy$
#
# }}}

use strict;

use Text::Autoformat qw(autoformat break_wrap);

use ForumUtils qw(plaintext transform_body register_plugin);

sub VERSION {(q$Revision: 1.7 $ =~ /([\d.]+)/)[0] or '0.0'}

register_plugin('newthread',\&execute);
register_plugin('newpost',\&execute);

my $ignore = 0;

my %conf = ();
my $directives = [
 { name => 'LineWidth',callback => \&set_value, data => \%conf },
];

sub register_options_userconf {
  my $class = shift;
  my $cfile = shift;

  return $directives;
}

sub set_value {
  my $name = shift;
  my $conf = shift;

  while(my $val = shift) {
    if(exists $conf->{$name}) {
      $conf->{$name} = [ $conf->{$name} ] unless ref $conf->{$name} eq 'ARRAY';
      push @{$conf->{$name}},$val;
    }
    else {
      $conf->{$name} = $val;
    }
  }
}

sub execute {
  my ($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi) = @_;

  return unless $user_config->{LineWidth}->[0]->[0];

  # get body
  my $body = $cgi->param('newbody');

  $body = plaintext($body,'> ',$fo_post_conf);

  $body = autoformat(
    $body,
    {
      right => $user_config->{LineWidth}->[0]->[0],
      break => break_wrap,
      all => 1,
      ignore => sub {
        ignr($user_config->{LineWidth}->[0]->[0],$_);
      }
    }
  );

  $body = transform_body($fo_default_conf,$fo_post_conf,$body,'> ');
  $cgi->param('newbody' => $body);
}

# ignore "small" lines
sub ignr {
  my $len = shift;
  my $txt = shift;

  $ignore = 1 if $txt =~ m!^\[DONT_BREAK\]!;
  $ignore = 0 if $txt =~ m!^\[/DONT_BREAK\]!;

  return 1 if length $txt < $len;
  return $ignore;
}

1;

# eof
