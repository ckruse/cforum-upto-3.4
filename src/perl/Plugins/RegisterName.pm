package Plugins::RegisterName;

#
# \file CheckRegisteredName.pm
# \author Christian Kruse, <ckruse@wwwtech.de>
#
# a plugin to register usernames
#

# {{{ initial comments
#
# $LastChangedDate$
# $LastChangedRevision$
# $LastChangedBy$
#
# }}}

# {{{ plugin header
use strict;

sub VERSION {(q$Revision$ =~ /([\d.]+)\s*$/)[0] or '0.0'}

use IO::Socket;

use CGI;
use CGI::Carp qw/fatalsToBrowser/;

use ForumUtils qw(
  recode
  get_error
  get_conf_val
);

push @{$main::Plugins->{writeconf}},\&execute;
push @{$main::Plugins->{unregister}},\&unregister;

# }}}

sub execute {
  my ($fo_default_conf,$fo_view_conf,$fo_userconf_conf,$user_config,$new_uconf,$cgi) = @_;

  return unless $main::UserName;
  return if !get_conf_val($user_config,'global','Name') && !get_conf_val($new_uconf,'global','Name');;
  return if !get_conf_val($user_config,'global','RegisteredName') && !get_conf_val($new_uconf,'global','RegisteredName');

  my $sock = new IO::Socket::UNIX(
    Type => SOCK_STREAM,
    Peer => get_conf_val($fo_default_conf,$main::Forum,'SocketName')
  ) or do {
    main::generate_edit_output($cgi,$fo_default_conf,$fo_view_conf,$fo_userconf_conf,$new_uconf,get_error($fo_default_conf,'NO_CONN'));
    return;
  };

  print $sock "SELECT ".$main::Forum."\n";
  my $line = <$sock>;

  my $auth = 0;
  my $name    = get_conf_val($new_uconf,'global','Name');
  my $oldname = get_conf_val($user_config,'global','Name');

  # shall we register?
  if($name && get_conf_val($new_uconf,'global','RegisteredName') eq 'yes') {

    #yes, we shall; is the previous name already registered?
    if($oldname && get_conf_val($user_config,'global','RegisteredName') eq 'yes') {

      # yes, it is; are actual name and previous name the same?
      if($oldname ne $name) {

        # no, they aren't -- register new name, unregister old
        print $sock "AUTH SET\n".
              "Name: ".$oldname."\n".
              "New-Name: ".$name."\n".
              "Pass: ".$main::UserName."\n\n";
      }
      else {
        # yes, they are. Do nothing...
        print $sock "PING\n";
      }
    }
    else {
      # no, previous name does not yet exist. Only register it
      print $sock "AUTH SET\nNew-Name: ".$name."\nPass: ".$main::UserName."\n\n";
    }

    $auth = 1;
  }
  else {
    my $new = get_conf_val($new_uconf,'global','RegisteredName') || 'no';
    if(get_conf_val($user_config,'global','RegisteredName') eq 'yes' && $new eq 'no') {
      print $sock "AUTH DELETE\nName: ".$oldname."\nPass: ".$main::UserName."\n\n";
      $auth = 1;
    }
    else {
      print $sock "PING\n";
    }
  }

  $line = <$sock>;

  print $sock "QUIT\n";
  close($sock);

  return if $line =~ /^200/ || $auth == 0;
  main::generate_edit_output($cgi,$fo_default_conf,$fo_view_conf,$fo_userconf_conf,$new_uconf,get_error($fo_default_conf,'FO',substr($line,0,3)));
  exit(0);
}

sub unregister {
  my ($fo_default_conf,$fo_view_conf,$fo_userconf_conf,$user_config,$cgi) = @_;

  my $name = get_conf_val($user_config,'global','Name');
  my $regist = get_conf_val($user_config,'global','RegisteredName');

  return unless $main::UserName;
  return unless $name;
  return unless $regist;

  if($regist eq 'yes') {
    my $sock = new IO::Socket::UNIX(
      Type => SOCK_STREAM,
      Peer => get_conf_val($fo_default_conf,$main::Forum,'SocketName')
    ) or die(get_error($fo_default_conf,'NO_CONN'));

    print $sock "SELECT ".$main::Forum."\n";
    my $line = <$sock>;

    print $sock "AUTH DELETE\nName: ".$user_config->{Name}->[0]->[0]."\nPass: ".$main::UserName."\n\n";

    $line = <$sock>;
    if(!defined $line || $line !~ /^200/) {
      die(recode($fo_default_conf,get_error($fo_default_conf,'error')));
    }

    print $sock "QUIT\n";
    close $sock;
  }
}

# eof
