package Plugins::Setvalue;

# \file Setvalue.pm
# \author Christian Kruse, <cjk@wwwtech.de>
#
# a plugin to set specific configuration values

# {{{ initial comments
#
# $LastChangedDate$
# $LastChangedRevision$
# $LastChangedBy$
#
# }}}

# {{{ program headers
#
use strict;

sub VERSION {(q$Revision$ =~ /([\d.]+)\s*$/)[0] or '0.0'}

use Storable qw(dclone);
use CGI::Carp qw/fatalsToBrowser/;

use ForumUtils qw/get_conf_val get_template get_error fatal recode get_user_config_file write_userconf get_node_data merge_config/;

use CForum::Template;
# }}}

$main::Plugins->{setvalue} = \&execute;
$main::Plugins->{removevalue} = \&remove_execute;

# {{{ execute
sub execute {
  my ($fo_default_conf,$fo_view_conf,$fo_userconf_conf,$user_config,$cgi) = @_;

  fatal($cgi,$fo_default_conf,$user_config,sprintf(get_error($fo_default_conf,'MUST_AUTH'),"$!"),get_conf_val($fo_default_conf,$main::Forum,'ErrorTemplate')) unless $main::UserName;

  my $moddoc = XML::GDOME->createDocFromURI(sprintf(get_conf_val($fo_userconf_conf,$main::Forum,'ModuleConfig'),get_conf_val($fo_default_conf,$main::Forum,'Language'))) or fatal($cgi,$fo_default_conf,$user_config,sprintf(get_error($fo_default_conf,'XML_PARSE'),"$!"),get_conf_val($fo_userconf_conf,$main::Forum,'FatalTemplate'));

  my $dhash = {};
  my @directives = $moddoc->findnodes('/config/directive');

  foreach my $directive (@directives) {
    next if $directive->getAttribute('invisible');
    my $name = $directive->getAttribute('name');
    $dhash->{$name} = $directive;
  }

  my $directive = $cgi->param('directive');
  unless($dhash->{$directive}) {
    print "Status: 500 Internal Server Error\n\n";
    return;
  }

  my @args = $dhash->{$directive}->findnodes('./argument');
  my $i    = 0;
  foreach my $arg (@args) {
    my $type  = $arg->getAttribute('type') || '';
    my $pname = $arg->getAttribute('paramname');
    my $val   = join ',',$cgi->param($pname);

    # {{{ validate user input
    if($val) {
      if($type eq 'http-url') {
        if(is_valid_http_link($val)) {
          print "Status: 500 Internal Server Error\n\n";
          return;
        }
      }
      elsif($type eq 'url') {
        if(is_valid_link($val)) {
          print "Status: 500 Internal Server Error\n\n";
          return;
        }
      }
      elsif($type eq 'email') {
        if(is_valid_mailaddress($val)) {
          print "Status: 500 Internal Server Error\n\n";
          return;
        }
      }
      else {
        my $validate = get_node_data($arg->findnodes('./validate'));
        if($validate && $val !~ /$validate/) {
          print "Status: 500 Internal Server Error\n\n";
          return;
        }
      }
    }
    # }}}

    # {{{ set value
    if($val) {
      my $type = $cgi->param('type') || '';

      if($type eq 'stringlist') {
        if($user_config->{global}->{$directive}->[0]->[$i]) {
          $user_config->{global}->{$directive}->[0]->[$i] .= ','.$val;
        }
        else {
          $user_config->{global}->{$directive}->[0]->[$i] = $val;
        }
      }
      else {
        $user_config->{global}->{$directive}->[0]->[$i] = $val;
      }
    }
    else {
      $user_config->{global}->{$directive}->[0]->[$i] = $val;
    }
    # }}}
  }

  my $file = get_user_config_file($fo_default_conf,$main::UserName);
  if(my $ret = write_userconf($fo_default_conf,$file,$user_config)) {
    print "Status: 500 Internal Server Error\n\n";
  }

  print "Status: 200 Ok\n\n";

}
# }}}

# {{{ remove_execute
sub remove_execute {
  my ($fo_default_conf,$fo_view_conf,$fo_userconf_conf,$user_config,$cgi) = @_;

  fatal($cgi,$fo_default_conf,$user_config,sprintf(get_error($fo_default_conf,'MUST_AUTH'),"$!"),get_conf_val($fo_default_conf,$main::Forum,'ErrorTemplate')) unless $main::UserName;

  my $moddoc = XML::GDOME->createDocFromURI(sprintf(get_conf_val($fo_userconf_conf,$main::Forum,'ModuleConfig'),get_conf_val($fo_default_conf,$main::Forum,'Language'))) or fatal($cgi,$fo_default_conf,$user_config,sprintf(get_error($fo_default_conf,'XML_PARSE'),"$!"),get_conf_val($fo_userconf_conf,$main::Forum,'FatalTemplate'));

  my $dhash = {};
  my @directives = $moddoc->findnodes('/config/directive');

  foreach my $directive (@directives) {
    next if $directive->getAttribute('invisible');
    my $name = $directive->getAttribute('name');
    $dhash->{$name} = $directive;
  }

  my $directive = $cgi->param('directive');
  unless($dhash->{$directive}) {
    print "Status: 500 Internal Server Error\n\n";
    return;
  }

  my @args = $dhash->{$directive}->findnodes('./argument');
  my $i    = 0;
  foreach my $arg (@args) {
    my $type  = $arg->getAttribute('type') || '';
    my $pname = $arg->getAttribute('paramname');
    my $val   = join ',',$cgi->param($pname);

    # {{{ validate user input
    if($val) {
      if($type eq 'http-url') {
        if(is_valid_http_link($val)) {
          print "Status: 500 Internal Server Error\n\n";
          return;
        }
      }
      elsif($type eq 'url') {
        if(is_valid_link($val)) {
          print "Status: 500 Internal Server Error\n\n";
          return;
        }
      }
      elsif($type eq 'email') {
        if(is_valid_mailaddress($val)) {
          print "Status: 500 Internal Server Error\n\n";
          return;
        }
      }
      else {
        my $validate = get_node_data($arg->findnodes('./validate'));
        if($validate && $val !~ /$validate/) {
          print "Status: 500 Internal Server Error\n\n";
          return;
        }
      }
    }
    # }}}

    # {{{ set value
    if($val) {
      my $type = $cgi->param('type') || '';

      if($type eq 'stringlist') {
        if($user_config->{global}->{$directive}->[0]->[$i]) {
          $val = quotemeta $val;
          $user_config->{global}->{$directive}->[0]->[$i] =~ s/,?$val,?//g;
        }
      }
    }
    else {
      delete $user_config->{global}->{$directive};
    }
    # }}}
  }

  delete $user_config->{global}->{$directive} unless $user_config->{global}->{$directive};

  my $file = get_user_config_file($fo_default_conf,$main::UserName);
  if(my $ret = write_userconf($fo_default_conf,$file,$user_config)) {
    print "Status: 500 Internal Server Error\n\n";
  }

  print "Status: 200 Ok\n\n";

}
# }}}

1;
# eof
