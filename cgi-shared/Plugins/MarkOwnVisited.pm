package Plugins::PostingAssistant;

#
# \file MarkOwnVisited.pm
# \author Christian Kruse, <ckruse@wwwtech.de>
#
# a plugin to mark users posting automatically visited
#

# {{{ initial comments
#
# $LastChangedDate$
# $LastChangedRevision$
# $LastChangedBy$
#
# }}}

use strict;

our $VERSION = (q$Revision$ =~ /([\d.]+)\s*$/)[0] or '0.0';

use CGI;
use CGI::Carp qw/fatalsToBrowser/;

use BerkeleyDB;

use ForumUtils qw(
  get_error
  rel_uri
);

push @{$main::Plugins->{afterpost}},\&execute;

sub execute {
  my ($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi,$sock,$answer) = @_;

  if(substr($answer,0,3) eq '200') {
    if($user_config->{VisitedFile} && $user_config->{VisitedFile}->[0]->[0]) {
      if($user_config->{MarkOwnPostingsVisited} && $user_config->{MarkOwnPostingsVisited}->[0]->[0] eq 'yes') {
        my $mid = $cgi->param('new_mid');

        my $dbfile = new BerkeleyDB::Btree(
          Filename => $user_config->{VisitedFile}->[0]->[0],
          Flags => DB_CREATE
        ) or return;


        $dbfile->db_put($mid,"1");
        $dbfile->db_close;
      }
    }
  }
}


1;
# eof
