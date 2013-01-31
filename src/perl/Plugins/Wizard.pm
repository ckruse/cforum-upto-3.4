package Plugins::Wizard;

# \file Wizard.pm
# \author Christian Kruse, <cjk@wwwtech.de>
#
# a plugin to lead the user through the configuration

# {{{ initial comments
#
# $LastChangedDate: 2009-01-16 14:32:24 +0100 (Fri, 16 Jan 2009) $
# $LastChangedRevision: 1639 $
# $LastChangedBy: ckruse $
#
# }}}

# {{{ program headers
#
use strict;

sub VERSION {(q$Revision: 1639 $ =~ /([\d.]+)\s*$/)[0] or '0.0'}

use CGI::Carp qw/fatalsToBrowser/;

use ForumUtils qw/get_conf_val get_template get_error fatal recode get_user_config_file write_userconf get_node_data merge_config config_to_template/;

use CForum::Template;
# }}}

$main::Plugins->{wizard} = \&execute;

sub execute {
  my ($fo_default_conf,$fo_view_conf,$fo_userconf_conf,$user_config,$cgi) = @_;

  fatal($cgi,$fo_default_conf,$user_config,sprintf(get_error($fo_default_conf,'MUST_AUTH'),"$!"),get_conf_val($fo_default_conf,$main::Forum,'ErrorTemplate')) unless $main::UserName;

  # {{{ get our template
  my $tpl_name = get_conf_val($fo_userconf_conf,$main::Forum,'WizardTemplate');
  fatal($cgi,$fo_default_conf,$user_config,get_error($fo_default_conf,'CONFIG_ERR'),get_conf_val($fo_default_conf,$main::Forum,'ErrorTemplate')) unless $tpl_name;

  $tpl_name = get_template($fo_default_conf,$user_config,$tpl_name);
  fatal($cgi,$fo_default_conf,$user_config,get_error($fo_default_conf,'CONFIG_ERR'),get_conf_val($fo_default_conf,$main::Forum,'ErrorTemplate')) unless $tpl_name;

  my $tpl = new CForum::Template($tpl_name);
  fatal($cgi,$fo_default_conf,$user_config,get_error($fo_default_conf,'CONFIG_ERR'),get_conf_val($fo_default_conf,$main::Forum,'ErrorTemplate')) unless $tpl;
  # }}}

  my $own_conf = merge_config($fo_default_conf,$fo_userconf_conf,$user_config,$cgi,1);

  # everything's ok, write config
  if(ref $own_conf) {
    my $cfile = get_user_config_file($fo_default_conf,$main::UserName);
    if(my $ret = write_userconf($fo_default_conf,$cfile,$own_conf)) {
      fatal($cgi,$fo_default_conf,$user_config,$ret,get_conf_val($fo_default_conf,$main::Forum,'ErrorTemplate'));
    }
  }

  config_to_template($fo_default_conf,$fo_userconf_conf,$fo_view_conf,$user_config,$tpl,$cgi);

  $tpl->setvalue($_,recode($fo_default_conf,$cgi->param($_))) foreach $cgi->param;

  # {{{ set page to correct value
  if(ref $own_conf) {
    if($cgi->param('prev')) {
      if(my $page = $cgi->param('page')) {
        if($page > 1) {
          $tpl->setvalue('page',$cgi->param('page') - 1);
        }
        else {
          $tpl->setvalue('page','1');
        }
      }
    }
    else {
      if(my $page = $cgi->param('page')) {
        $tpl->setvalue('page',$cgi->param('page') + 1);
      }
      else {
        $tpl->setvalue('page','1');
      }
    }
  }
  # }}}

  $tpl->setvalue('err',recode($fo_default_conf,$own_conf)) unless ref $own_conf;
  $tpl->setvalue('forumbase',recode($fo_default_conf,get_conf_val($fo_default_conf,$main::Forum,'UBaseURL')));
  $tpl->setvalue('userconfig',recode($fo_default_conf,get_conf_val($fo_default_conf,$main::Forum,'UserConfig')));
  $tpl->setvalue('script',recode($fo_default_conf,get_conf_val($fo_default_conf,$main::Forum,'UserConfig')));
  $tpl->setvalue('usermanagement',get_conf_val($fo_default_conf,$main::Forum,'UserManagement'));
  $tpl->setvalue('charset',get_conf_val($fo_default_conf,$main::Forum,'ExternCharset'));

  print $cgi->header(-type => 'text/html; charset='.get_conf_val($fo_default_conf,$main::Forum,'ExternCharset'));
  print $tpl->parseToMem;
}

1;
# eof
