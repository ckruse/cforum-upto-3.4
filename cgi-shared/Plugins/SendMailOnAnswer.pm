package Plugins::SendMailOnActivity;

#
# \file SendMailOnActivity.pm
# \author Christian Kruse, <ckruse@wwwtech.de>
# \brief a plugin to send the user a mail
#
# This plugin allows the user to choose to get a
# mail if he gets an answer to his posting
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

our $VERSION = (q$Revision$ =~ /([\d.]+)\s*$/)[0] or '0.0';

use ForumUtils qw(
  get_error
);

use BerkeleyDB;
use Fcntl qw(:flock);
use Net::SMTP;
# }}}

push @{$main::Plugins->{newpost}},\&execute;
push @{$main::Plugins->{newthread}},\&execute;

sub execute {
  my ($fo_default_conf,$fo_view_conf,$fo_post_conf,$user_config,$cgi) = @_;
  local *DAT;
  my $db;
  my $sm         = $cgi->param('sendmailonanswer') && $main::UserName;
  my $tidmid     = $cgi->param('fupto');
  my ($tid,$mid) = split /;/,$tidmid;

  return if !$tidmid && !$sm;

  # {{{ open db file for locking
  if(-f $fo_post_conf->{PostingDatabase}->[0]->[0]) {
    open DAT,'<',$fo_post_conf->{PostingDatabase}->[0]->[0];
  }
  else {
    open DAT,'>',$fo_post_conf->{PostingDatabase}->[0]->[0];
  }
  # }}}

  # {{{ open database file as a database
  if($sm) {
    $db = new BerkeleyDB::Btree(
      Filename => $fo_post_conf->{PostingDatabase}->[0]->[0],
      Flags => DB_CREATE
    ) or return;

    flock DAT,LOCK_EX or return;
  }
  else {
    $db = new BerkeleyDB::Btree(
      Filename => $fo_post_conf->{PostingDatabase}->[0]->[0],
      Flags => DB_RDONLY
    ) or return;

    flock DAT,LOCK_SH or return;
  }
  # }}}

  my @send_mail_to = ();
  if($tid) {
    my $receivers;
    $db->db_get($tid,$receivers);
    @send_mail_to = split /\0/, $receivers if $receivers;
  }

  if($sm) {
    if($user_config->{Name} && $user_config->{Name}->[0]->[0] && $user_config->{EMail} && $user_config->{EMail}->[0]->[0]) {
      push @send_mail_to,$user_config->{Name}->[0]->[0]."\x1E".$user_config->{EMail}->[0]->[0];
      $db->db_set($tid,join "\0",@send_mail_to);
      pop @send_mail_to;
    }
  }

  $db->db_close;
  close DAT;

  my $smtp = new Net::SMTP($fo_post_conf->{SMTPHost}->[0]->[0]) or return;
  my $from = $fo_post_conf->{SMTPFrom}->[0]->[0];
  my $subj = get_error($fo_default_conf,'new_post_mail','subject');
  my $body = get_error($fo_default_conf,'new_post_mail','body');

  $body =~ s!\\n!\n!g;

  foreach my $mail (@send_mail_to) {
    my ($name,$email) = split /\x1E/,$mail;
    my $lbody = $body;
    my $lsubj = $subj;

    $lsubj =~ s!\{tid\}!$tid!g;
    $lsubj =~ s!\{mid\}!$mid!g;

    $lbody =~ s!\{tid\}!$tid!g;
    $lbody =~ s!\{mid\}!$mid!g;
    $lbody =~ s!\{name\}!$name!g;
    $lbody =~ s!\{email\}!$email!g;

    $smtp->mail($from);
    $smtp->to($email);
    $smtp->data();
    $smtp->datasend('Subject: '.$lsubj."\n");
    $smtp->datasend('From: '.$from."\n");
    $smtp->datasend('Reply-To: '.$from."\n");
    $smtp->datasend('Errors-To: '.$from."\n\n");
    $smtp->datasend($lbody);
    $smtp->dataend();
  }

  $smtp->quit();

  return 0;
}

