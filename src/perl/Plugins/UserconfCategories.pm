package Plugins::UserconfCategories;

# \file ImportExport.pm
# \author Christian Kruse, <ckruse@wwwtech.de>
#
# a plugin to import or export configuration data

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

use CGI::Carp qw(fatalsToBrowser);

use ForumUtils qw/get_conf_val/;
# }}}

push @{$main::Plugins->{display}},\&execute;

sub execute {
  my ($fo_default_conf,$fo_userconf_conf,$user_config,$cgi,$tpl) = @_;

  my $cats = get_conf_val($user_config,'global','ShowCategories');

  if($cats) {
    my $catsvar = new CForum::Template::t_cf_tpl_variable($CForum::Template::TPL_VARIABLE_ARRAY);
    my @catsary = split /,/,$cats;

    $catsvar->addvalue($_) foreach @catsary;
    $tpl->setvar('hidedcats',$catsvar);
  }

  $cats = get_conf_val($user_config,'global','HighlightCategories');
  if($cats) {
    my $catsvar = new CForum::Template::t_cf_tpl_variable($CForum::Template::TPL_VARIABLE_ARRAY);
    my @catsary = split /,/,$cats;

    $catsvar->addvalue($_) foreach @catsary;
    $tpl->setvar('highcats',$catsvar);
  }

  my $blacklist = get_conf_val($user_config,'global','BlackList');
  if($blacklist) {
    $blacklist =~ s!,!\n!g;
    $tpl->setvalue('blacklist',$blacklist);
  }

  my $whitelist = get_conf_val($user_config,'global','WhiteList');
  if($whitelist) {
    $whitelist =~ s!,!\n!g;
    $tpl->setvalue('whitelst',$whitelist);
  }


  return 0;
}

1;
# eof
