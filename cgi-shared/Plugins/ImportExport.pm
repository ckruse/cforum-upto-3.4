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
use constant CF_DTD => 'http://wwwtech.de/cforum/download/cfconfig.dtd';

sub VERSION {(q$Revision$ =~ /([\d.]+)\s*$/)[0] or '0.0'}

use CGI::Carp qw(fatalsToBrowser);

use XML::GDOME;
# }}}

push @{$main::Plugins->{export}},\&export;
push @{$main::Plugins->{imprt}},\&imprt;

sub export {
  my ($fo_default_conf,$fo_view_conf,$fo_userconf_conf,$user_config) = @_;

  my $uconf = XML::GDOME->createDocumentFromURI(get_conf_val($fo_userconf_conf,$main::Forum,'ModuleConfig'));
  my $dtd = XML::GDOME->createDocumentType('CFConfig',undef,CF_DTD);
  my $doc = XML::GDOME->createDocument(undef, 'CFConfig', $dtd);
  my $root = $doc->documentElement;

  my @directives = $uconf->findnodes('/config/page/section/directive');
  foreach my $directive (@directives) {
    my @vals = get_conf_val($user_config,'global',$directive->getAttribute('name'));

    my $dir_el = $doc->createElement('Directive');
    $dir_el->setAttribute('name',$directive->getAttribute('name'));
    $root->appendChild($dir_el);

    foreach my $val (@vals) {
      my $val_el = $doc->createElement('Argument');
      $val_el->appendChild($doc->createCDATASection($val));
      $dir_el->appendChild($val_el);
    }
  }

  print "Content-Type: text/xml; charset=UTF-8\015\012\015\012",$doc->toString;

  return 1;
}

sub import {
  my ($fo_default_conf,$fo_view_conf,$fo_userconf_conf,$user_config) = @_;
}

1;
# eof
