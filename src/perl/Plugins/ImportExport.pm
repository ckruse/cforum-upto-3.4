package Plugins::ImportExport;

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
use constant CF_DTD => 'http://wwwtech.de/cforum/download/cfconfig-0.1.dtd';
use constant CF_VER => '0.1';

sub VERSION {(q$Revision$ =~ /([\d.]+)\s*$/)[0] or '0.0'}

use CGI::Carp qw(fatalsToBrowser);
use XML::GDOME;
use Storable qw(dclone);

use ForumUtils qw/get_conf_val fatal get_template get_user_config_file get_error recode get_node_data/;
use CForum::Validator qw/is_valid_http_link is_valid_link is_valid_mailaddress/;
use CForum::Template;

# }}}

$main::Plugins->{exprt} = \&exprt;
$main::Plugins->{imprt} = \&imprt;
$main::Plugins->{imprtform} = \&imprtform;

sub exprt {
  my ($fo_default_conf,$fo_view_conf,$fo_userconf_conf,$user_config,$cgi) = @_;

  fatal($cgi,$fo_default_conf,$user_config,sprintf(get_error($fo_default_conf,'MUST_AUTH'),"$!"),get_conf_val($fo_userconf_conf,$main::Forum,'FatalTemplate')) unless $main::UserName;

  my $uconf = XML::GDOME->createDocFromURI(sprintf(get_conf_val($fo_userconf_conf,$main::Forum,'ModuleConfig'),get_conf_val($fo_default_conf,$main::Forum,'Language')));
  my $dtd = XML::GDOME->createDocumentType('CFConfig',undef,CF_DTD);
  my $doc = XML::GDOME->createDocument(undef, 'CFConfig', $dtd);
  my $root = $doc->documentElement;

  $root->setAttribute('version',CF_VER);

  my @directives = $uconf->findnodes('/config/page/section/directive');
  foreach my $directive (@directives) {
    if(my @vals = get_conf_val($user_config,'global',$directive->getAttribute('name'))) {
      my $dir_el = $doc->createElement('Directive');
      $dir_el->setAttribute('name',$directive->getAttribute('name'));
      $root->appendChild($dir_el);

      foreach my $val (@vals) {
        my $val_el = $doc->createElement('Argument');
        $val_el->appendChild($doc->createCDATASection($val));
        $dir_el->appendChild($val_el);
      }
    }
  }

  print $cgi->header(-type => "text/xml; charset=UTF-8",-content_disposition => 'attachment; filename='.$main::UserName.'.xml'),$doc->toString;
}

sub imprt {
  my ($fo_default_conf,$fo_view_conf,$fo_userconf_conf,$user_config,$cgi) = @_;

  fatal($cgi,$fo_default_conf,$user_config,sprintf(get_error($fo_default_conf,'MUST_AUTH'),"$!"),get_conf_val($fo_userconf_conf,$main::Forum,'FatalTemplate')) unless $main::UserName;

  my $own_conf = dclone($user_config->{global});
  my $fh = $cgi->upload('import');
  my $str = '';

  if(!$fh) {
    fatal($cgi,$fo_default_conf,$user_config,get_error($fo_default_conf,'IMPORT_DATAFAILURE'),get_conf_val($fo_userconf_conf,$main::Forum,'FatalTemplate')) unless $str = $cgi->param('importstr'); 
  }
  else {
    $str .= $_ while(<$fh>);
  }

  my $idoc = XML::GDOME->createDocFromString($str,0) or fatal($cgi,$fo_default_conf,$user_config,sprintf(get_error($fo_default_conf,'XML_PARSE'),"$!"),get_conf_val($fo_userconf_conf,$main::Forum,'FatalTemplate'));
  my $moddoc = XML::GDOME->createDocFromURI(sprintf(get_conf_val($fo_userconf_conf,$main::Forum,'ModuleConfig'),get_conf_val($fo_default_conf,$main::Forum,'Language'))) or fatal($cgi,$fo_default_conf,$user_config,sprintf(get_error($fo_default_conf,'XML_PARSE'),"$!"),get_conf_val($fo_userconf_conf,$main::Forum,'FatalTemplate'));

  my $dhash = {};
  my @directives = $moddoc->findnodes('/config/page/section/directive');

  foreach my $directive (@directives) {
    my $name = $directive->getAttribute('name');
    $dhash->{$name} = $directive;
  }

  my $root = $idoc->documentElement;
  my $ver = $root->getAttribute('version');

  fatal($cgi,$fo_default_conf,$user_config,get_error($fo_default_conf,'IMPORT_VERSION'),get_conf_val($fo_userconf_conf,$main::Forum,'FatalTemplate')) if $ver > CF_VER;

  @directives = $idoc->findnodes('/CFConfig/Directive');
  foreach my $directive (@directives) {
    my @directive_values = ();
    my $name = $directive->getAttribute('name');
    my $uconf_directive = $dhash->{$name};

    next unless $uconf_directive;

    my @arguments_user = $directive->getElementsByTagName('Argument');
    my @arguments_mod  = $uconf_directive->findnodes('./argument/validate');

    fatal($cgi,$fo_default_conf,$user_config,get_error($fo_default_conf,'IMPORT_DATAFAILURE'),get_conf_val($fo_userconf_conf,$main::Forum,'FatalTemplate')) if @arguments_user > @arguments_mod;

    for(my $i=0;$i<@arguments_user;++$i) {
      my $type = $arguments_mod[$i]->getAttribute('type') || '';
      my $val = get_node_data($arguments_user[$i]);

      # {{{ validate user input
      if($val) {
        if($type eq 'http-url') {
          fatal($cgi,$fo_default_conf,$user_config,get_error($fo_default_conf,'IMPORT_DATAINVALID'),get_conf_val($fo_userconf_conf,$main::Forum,'FatalTemplate')) if is_valid_http_link($val);
        }
        elsif($type eq 'url') {
          fatal($cgi,$fo_default_conf,$user_config,get_error($fo_default_conf,'IMPORT_DATAINVALID'),get_conf_val($fo_userconf_conf,$main::Forum,'FatalTemplate')) if is_valid_link($val);
        }
        elsif($type eq 'email') {
          fatal($cgi,$fo_default_conf,$user_config,get_error($fo_default_conf,'IMPORT_DATAINVALID'),get_conf_val($fo_userconf_conf,$main::Forum,'FatalTemplate')) if is_valid_mailaddress($val);
        }
        else {
          my $validate = get_node_data($arguments_mod[$i]);
          if($validate) {
            fatal($cgi,$fo_default_conf,$user_config,get_error($fo_default_conf,'IMPORT_DATAINVALID'),get_conf_val($fo_userconf_conf,$main::Forum,'FatalTemplate')) unless $val =~ /$validate/;
          }
        }
      }
      # }}}

      push @directive_values,$val;
    }


    $own_conf->{$name} = [[@directive_values]];
  }

  my $file = get_user_config_file($fo_default_conf,$main::UserName);
  open DAT,'>',$file or fatal($cgi,$fo_default_conf,$user_config,sprintf(get_error($fo_default_conf,'IO_ERR'),"$!"),get_conf_val($fo_userconf_conf,$main::Forum,'FatalTemplate'));

  foreach my $dir (keys %{$own_conf}) {
    next if !$own_conf->{$dir} || !@{$own_conf->{$dir}};

    print DAT $dir;
    foreach my $entry (@{$own_conf->{$dir}}) {
      foreach(@{$entry}) {
        my $val = $_ || '';

        $val =~ s!\\!\\\\!g;
        $val =~ s/\015\012|\015|\012/\\n/sg;
        $val =~ s/"/\\"/g;

        print DAT ' "'.$val.'"';
      }
    }

    print DAT "\n";
  }

  close DAT;

  my $tpl = new CForum::Template(get_template($fo_default_conf,$user_config,get_conf_val($fo_userconf_conf,$main::Forum,'ImportOk')));

  $tpl->setvalue('forumbase',recode($fo_default_conf,get_conf_val($fo_default_conf,$main::Forum,'UBaseURL')));
  $tpl->setvalue('script',recode($fo_default_conf,get_conf_val($fo_default_conf,$main::Forum,'UserConfig')));
  $tpl->setvalue('charset',get_conf_val($fo_default_conf,$main::Forum,'ExternCharset'));
  $tpl->setvalue('acceptcharset',get_conf_val($fo_default_conf,$main::Forum,'ExternCharset').', UTF-8');

  print $cgi->header(-type => "text/html; charset=".get_conf_val($fo_default_conf,$main::Forum,'ExternCharset')),$tpl->parseToMem;
}

sub imprtform {
  my ($fo_default_conf,$fo_view_conf,$fo_userconf_conf,$user_config,$cgi) = @_;

  fatal($cgi,$fo_default_conf,$user_config,sprintf(get_error($fo_default_conf,'MUST_AUTH'),"$!"),get_conf_val($fo_userconf_conf,$main::Forum,'FatalTemplate')) unless $main::UserName;

  my $tpl = new CForum::Template(get_template($fo_default_conf,$user_config,get_conf_val($fo_userconf_conf,$main::Forum,'ImportForm')));
  $tpl->setvalue('forumbase',recode($fo_default_conf,get_conf_val($fo_default_conf,$main::Forum,'UBaseURL')));
  $tpl->setvalue('script',recode($fo_default_conf,get_conf_val($fo_default_conf,$main::Forum,'UserConfig')));
  $tpl->setvalue('charset',get_conf_val($fo_default_conf,$main::Forum,'ExternCharset'));
  $tpl->setvalue('acceptcharset',get_conf_val($fo_default_conf,$main::Forum,'ExternCharset').', UTF-8');

  print $cgi->header(-type => "text/html; charset=".get_conf_val($fo_default_conf,$main::Forum,'ExternCharset')),$tpl->parseToMem;
}

1;
# eof
