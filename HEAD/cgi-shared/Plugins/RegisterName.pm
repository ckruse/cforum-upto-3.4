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
#

use strict;

sub VERSION {(q$Revision: 1.3 $ =~ /([\d.]+)\s*$/)[0] or '0.0'}

use IO::Socket;

use CGI;
use CGI::Carp qw/fatalsToBrowser/;

use CheckRFC;
use ForumUtils qw(
  recode
  get_error
);

push @{$main::Plugins->{writeconf}},\&execute;
push @{$main::Plugins->{unregister}},\&unregister;

# }}}

sub execute {
  my ($fo_default_conf,$fo_view_conf,$fo_userconf_conf,$user_config,$new_uconf,$cgi) = @_;

  return unless $main::UserName;
  return if !$user_config->{Name} && !$new_uconf->{Name};
  return if !$user_config->{RegisteredName} && !$new_uconf->{RegisteredName};

  my $sock = new IO::Socket::UNIX(
    Type => SOCK_STREAM,
    Peer => $fo_default_conf->{SocketName}->[0]->[0]
  ) or do {
    main::generate_edit_output($cgi,$fo_default_conf,$fo_view_conf,$fo_userconf_conf,$new_uconf,get_error($fo_default_conf,'NO_CONN'));
    return;
  };

  my $auth = 0;
  # shall we register?
  if($new_uconf->{Name} && $new_uconf->{RegisteredName}->[0]->[0] eq 'yes') {

    #yes, we shall; is the previous name already registered?
    if($user_config->{Name} && $user_config->{RegisteredName}->[0]->[0] eq 'yes') {

      # yes, it is; are actual name and previous name the same?
      if($user_config->{Name}->[0]->[0] ne $new_uconf->{Name}->[0]->[0]) {

        # no, they aren't -- register new name, unregister old
        print $sock "AUTH SET\n".
              "Name: ".$user_config->{Name}->[0]->[0]."\n".
              "New-Name: ".$new_uconf->{Name}->[0]->[0]."\n".
              "Pass: ".$main::UserName."\n\n";
      }
      else {
        # yes, they are. Do nothing...
        print $sock "PING\n";
      }
    }
    else {
      # no, previous name does not yet exist. Only register it
      print $sock "AUTH SET\nNew-Name: ".$new_uconf->{Name}->[0]->[0]."\nPass: ".$main::UserName."\n\n";
    }

    $auth = 1;
  }
  else {
    if($user_config->{RegisteredName}->[0]->[0] eq 'yes' && $new_uconf->{RegisteredName}->[0]->[0] eq 'no') {
      print $sock "AUTH DELETE\nName: ".$user_config->{Name}->[0]->[0]."\nPass: ".$main::UserName."\n\n";
      $auth = 1;
    }
    else {
      print $sock "PING\n";
    }
  }

  my $line = <$sock>;

  print $sock "QUIT\n";
  close($sock);

  return if $line =~ /^200/ || $auth == 0;
  main::generate_edit_output($cgi,$fo_default_conf,$fo_view_conf,$fo_userconf_conf,$new_uconf,get_error($fo_default_conf,'FO',substr($line,0,3)));
  exit(0);
}

sub unregister {
  my ($fo_default_conf,$fo_view_conf,$fo_userconf_conf,$user_config,$cgi) = @_;

  return unless $main::UserName;
  return unless $user_config->{Name}->[0]->[0];

  if($user_config->{RegisteredName}->[0]->[0] && $user_config->{RegisteredName}->[0]->[0] eq 'yes') {
    my $sock = new IO::Socket::UNIX(
      Type => SOCK_STREAM,
      Peer => $fo_default_conf->{SocketName}->[0]->[0]
    ) or die(get_error($fo_default_conf,'NO_CONN'));

    print $sock "AUTH DELETE\nName: ".$user_config->{Name}->[0]->[0]."\nPass: ".$main::UserName."\n\n";

    my $line = <$sock>;
    if(!defined $line || $line !~ /^200/) {
      die(recode($fo_default_conf,get_error($fo_default_conf,'error')));
    }

    print $sock "QUIT\n";
    close $sock;
  }

}

# eof
