package Plugins::Quotings;

#
# \file Quotings.pm
# \author Christian Kruse, <ckruse@wwwtech.de>
# \brief quotings from a pool
#
# This plugin provides preformatted quotings from a pool of quotings
#

# {{{ initial comments
#
# $LastChangedDate$
# $LastChangedRevision$
# $LastChangedBy$
#
# }}}

use strict;

use CForum::Template;

use CheckRFC;
use ForumUtils qw/transform_body get_node_data get_template/;

use XML::GDOME;

push @{$main::Plugins->{newthread}},\&execute;
push @{$main::Plugins->{newpost}},\&execute;

sub execute {
  my ($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi) = @_;

  my $body = $cgi->param('newbody');
  my $file = $fo_post_conf->{QuotingsFile}->[0]->[0];

  return unless $file;

  my @quotes = ();
  push @quotes,[$1,$2] while $body =~ m!\[(b?quote):\#(\d+(:?\.\d+)*)\]!g;

  if(@quotes) {
    my $qdoc = XML::GDOME->createDocFromURI($file);
    my %quotes = ();

    foreach my $q ($qdoc->getElementsByTagName('quote')) {
      $quotes{$q->getAttribute('id')} = $q;
    }

    foreach my $quote (@quotes) {
      my ($qtype,$qid) = @{$quote};
      next unless exists $quotes{$qid};

      my $quote = $quotes{$qid};

      my ($source) = $quote->getElementsByTagName('source');
      my ($author) = $source->getElementsByTagName('author');
      my $uri      = get_node_data(($source->getElementsByTagName('document'))[0]);
      my $text     = get_node_data(($quote->getElementsByTagName('text'))[0]);

      my $name  = get_node_data($author);
      my $email = $author->getAttribute('email');

      my $tpl;

      if($qtype eq 'bquote') {
        $tpl = new CForum::Template(get_template($fo_default_conf,$user_config,$fo_post_conf->{BlockQuoteTemplate}->[0]->[0]));
      }
      else {
        $tpl = new CForum::Template(get_template($fo_default_conf,$user_config,$fo_post_conf->{QuoteTemplate}->[0]->[0]));
      }

      $tpl->setVar('mail',$email);
      $tpl->setVar('name',$name);
      $tpl->setVar('text',$text);
      $tpl->setVar('uri',$uri);

      my $txt = $tpl->parseToMem;

      $body =~ s!\[$qtype:\#$qid\]!$txt!g;
    }

    $cgi->param('newbody' => $body);
  }
}

1;

# eof
