package Plugins::Management::SQL;

#
# \file SQL.pm
# \author Christian Kruse, <ckruse@wwwtech.de>
#
# Backend user management plugin
#

use DBI;
use ForumUtils qw(
  get_error
);

$main::Management = new Plugins::Management::SQL;

# {{{ sub new
sub new {
  my $class = shift;
  return bless {},ref $class || $class;
}
# }}}

# {{{ create connection string
sub connstr {
  my $fuc = shift;
  my $connstr = 'DBI:'.$fuc->{SQLDriver}->[0]->[0].':dbname='.$fuc->{SQLDatabase}->[0]->[0].
                ($fuc->{SQLHost} ? ';host='.$fuc->{SQLHost}->[0]->[0] : '').
                ($fuc->{SQLPort} ? ';port='.$fuc->{SQLPort}->[0]->[0] : '');

  return $connstr;
}
# }}}

# {{{ get column and table data
sub sql_data {
  my $fuc = shift;

  return (
    $fuc->{SQLUserTable}->[0]->[0],
    $fuc->{SQLUserColumn}->[0]->[0],
    $fuc->{SQLPasswdColumn}->[0]->[0],
    $fuc->{SQLEmailColumn}->[0]->[0]
  );
}
# }}}

# {{{ change password
sub change_pass {
  my $self  = shift;
  my $fdc   = shift;
  my $fuc   = shift;
  my $uname = shift;
  my $pass  = shift;

  my ($table,$usercol,$passwdcol,$emailcol) = sql_data($fuc);

  my $connstr = connstr($fuc);
  my $dbh     = DBI->connect($connstr,$fuc->{SQLUser}->[0]->[0],$fuc->{SQLPass}->[0]->[0]);
  return get_error($main::fo_default_conf, 'SQL', 'connect') unless defined $dbh;

  # the password and the username are only ASCII, so it doesn't need a recode
  # to another charset
  my $dbq = $dbh->prepare('UPDATE '.$table.' SET '.$passwdcol.' = ? WHERE '.$usercol.' = ?');
  return get_error($main::fo_default_conf, 'SQL', 'execute') unless defined($dbq) && $dbq;
  $dbq->execute($pass,$uname) or return get_error($main::fo_default_conf, 'SQL', 'execute');
  $dbh->disconnect;
  return undef;
}
# }}}

# {{{ remove user
sub remove_user {
  my $self  = shift;
  my $fdc   = shift;
  my $fuc   = shift;
  my $uname = shift;

  my $connstr = connstr($fuc);
  my $dbh     = DBI->connect($connstr,$fuc->{SQLUser}->[0]->[0],$fuc->{SQLPass}->[0]->[0]);
  return get_error($main::fo_default_conf, 'SQL', 'connect') unless defined $dbh;

  my ($table,$usercol,$passwdcol,$emailcol) = sql_data($fuc);

  my $dbq = $dbh->prepare('DELETE FROM '.$table.' WHERE '.$usercol.' = ?');
  return get_error($main::fo_default_conf, 'SQL', 'execute') unless defined($dbq) && $dbq;
  $dbq->execute($uname) or return get_error($main::fo_default_conf, 'SQL', 'execute');
  $dbh->disconnect;
  return undef;
}
# }}}

# {{{ get_password
sub get_password {
  my $self  = shift;
  my $fdc   = shift;
  my $fuc   = shift;
  my $uname = shift;

  my $connstr = connstr($fuc);
  my $dbh     = DBI->connect($connstr,$fuc->{SQLUser}->[0]->[0],$fuc->{SQLPass}->[0]->[0]);
  return get_error($main::fo_default_conf, 'SQL', 'connect') unless defined $dbh;

  my ($table,$usercol,$passwdcol,$emailcol) = sql_data($fuc);

  my $dbq = $dbh->prepare('SELECT '.$passwdcol.','.$emailcol.' FROM '.$table.' WHERE '.$usercol.' = ?');
  return get_error($main::fo_default_conf, 'SQL', 'execute') unless defined($dbq) && $dbq;
  $dbq->execute($uname) or return get_error($main::fo_default_conf, 'SQL', 'execute');

  if($dbq->rows) {
    my $result = $dbq->fetchrow_hashref;
    
    $dbq->finish;
    $dbh->disconnect;

    return {
      pass => $result->{$passwdcol},
      email => $result->{$emailcol}
    };
  }

  $dbq->finish;
  $dbh->disconnect;
  return;
}
# }}}

# {{{ add_user
sub add_user {
  my $self  = shift;
  my $fdc   = shift;
  my $fuc   = shift;
  my $uname = shift;
  my $pass  = shift;
  my $email = shift;

  my $connstr = connstr($fuc);
  my $dbh     = DBI->connect($connstr,$fuc->{SQLUser}->[0]->[0],$fuc->{SQLPass}->[0]->[0]);
  return get_error($main::fo_default_conf, 'SQL', 'connect') unless defined $dbh;

  my ($table,$usercol,$passwdcol,$emailcol) = sql_data($fuc);
  
  my $dbq = $dbh->prepare('INSERT INTO '.$table.' ('.$usercol.','.$passwdcol.','.$emailcol.') VALUES (?,?,?)');
  return get_error($main::fo_default_conf, 'SQL', 'execute') unless defined($dbq) && $dbq;
  $dbq->execute($uname,$pass,$email) or return get_error($main::fo_default_conf, 'SQL', 'execute');
  $dbh->disconnect;
  return undef;
}
# }}}

# require
1;

# eof
