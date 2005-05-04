package Plugins::Management::SQL;

#
# \file SQL.pm
# \author Christian Kruse, <ckruse@wwwtech.de>
#
# Backend user management plugin
#

use DBI;
use ForumUtils qw(get_error get_conf_val);

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
  my $host = get_conf_val($fuc,$main::Forum,'SQLHost');
  my $port = get_conf_val($fuc,$main::Forum,'SQLPort');

  my $connstr = 'DBI:'.get_conf_val($fuc,$main::Forum,'SQLDriver').':dbname='.get_conf_val($fuc,$main::Forum,'SQLDatabase').
                ($host ? ';host='.$host : '').
                ($port ? ';port='.$port : '');

  return $connstr;
}
# }}}

# {{{ get column and table data
sub sql_data {
  my $fuc = shift;

  return (
    get_conf_val($fuc,$main::Forum,'SQLUserTable'),
    get_conf_val($fuc,$main::Forum,'SQLUserColumn'),
    get_conf_val($fuc,$main::Forum,'SQLPasswdColumn'),
    get_conf_val($fuc,$main::Forum,'SQLEmailColumn')
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
  my $dbh     = DBI->connect($connstr,get_conf_val($fuc,$main::Forum,'SQLUser'),get_conf_val($fuc,$main::Forum,'SQLPass'));
  return get_error($fdc, 'SQL', 'connect') unless defined $dbh;

  # the password and the username are only ASCII, so it doesn't need a recode
  # to another charset
  my $dbq = $dbh->prepare('UPDATE '.$table.' SET '.$passwdcol.' = ? WHERE '.$usercol.' = ?');
  return get_error($fdc, 'SQL', 'execute') unless defined($dbq) && $dbq;
  $dbq->execute($pass,$uname) or return get_error($fdc, 'SQL', 'execute');
  $dbh->disconnect;
  return undef;
}
# }}}

# {{{ change email
sub change_email {
  my $self  = shift;
  my $fdc   = shift;
  my $fuc   = shift;
  my $uname = shift;
  my $pass  = shift;

  my ($table,$usercol,$passwdcol,$emailcol) = sql_data($fuc);

  my $connstr = connstr($fuc);
  my $dbh     = DBI->connect($connstr,get_conf_val($fuc,$main::Forum,'SQLUser'),get_conf_val($fuc,$main::Forum,'SQLPass'));
  return get_error($fdc, 'SQL', 'connect') unless defined $dbh;

  # the password and the username are only ASCII, so it doesn't need a recode
  # to another charset
  my $dbq = $dbh->prepare('UPDATE '.$table.' SET '.$emailcol.' = ? WHERE '.$usercol.' = ?');
  return get_error($fdc, 'SQL', 'execute') unless $dbq;
  $dbq->execute($pass,$uname) or return get_error($fdc, 'SQL', 'execute');
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
  my $dbh     = DBI->connect($connstr,get_conf_val($fuc,$main::Forum,'SQLUser'),get_conf_val($fuc,$main::Forum,'SQLPass'));
  return get_error($fdc, 'SQL', 'connect') unless defined $dbh;

  my ($table,$usercol,$passwdcol,$emailcol) = sql_data($fuc);

  my $dbq = $dbh->prepare('DELETE FROM '.$table.' WHERE '.$usercol.' = ?');
  return get_error($fdc, 'SQL', 'execute') unless defined($dbq) && $dbq;
  $dbq->execute($uname) or return get_error($fdc, 'SQL', 'execute');
  $dbh->disconnect;
  return undef;
}
# }}}

# {{{ get_password_by_username
sub get_password_by_username {
  my $self  = shift;
  my $fdc   = shift;
  my $fuc   = shift;
  my $uname = shift;

  my $connstr = connstr($fuc);
  my $dbh     = DBI->connect($connstr,get_conf_val($fuc,$main::Forum,'SQLUser'),get_conf_val($fuc,$main::Forum,'SQLPass'));
  return get_error($fdc, 'SQL', 'connect') unless defined $dbh;

  my ($table,$usercol,$passwdcol,$emailcol) = sql_data($fuc);

  my $dbq = $dbh->prepare('SELECT '.$passwdcol.','.$emailcol.' FROM '.$table.' WHERE '.$usercol.' = ?');
  return get_error($fdc, 'SQL', 'execute') unless defined($dbq) && $dbq;
  $dbq->execute($uname) or return get_error($fdc, 'SQL', 'execute');

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

# {{{ get_password_by_email
sub get_password_by_email {
  my $self  = shift;
  my $fdc   = shift;
  my $fuc   = shift;
  my $email = shift;

  my $connstr = connstr($fuc);
  my $dbh     = DBI->connect($connstr,get_conf_val($fuc,$main::Forum,'SQLUser'),get_conf_val($fuc,$main::Forum,'SQLPass'));
  return get_error($fdc, 'SQL', 'connect') unless defined $dbh;

  my ($table,$usercol,$passwdcol,$emailcol) = sql_data($fuc);

  my $dbq = $dbh->prepare('SELECT '.$usercol.','.$passwdcol.','.$emailcol.' FROM '.$table.' WHERE '.$emailcol.' = ?');
  return get_error($fdc, 'SQL', 'execute') unless defined($dbq) && $dbq;
  $dbq->execute($email) or return get_error($fdc, 'SQL', 'execute');

  if($dbq->rows) {
    my $result = $dbq->fetchrow_hashref;
    
    $dbq->finish;
    $dbh->disconnect;

    return {
      pass => $result->{$passwdcol},
      email => $result->{$emailcol},
      uname => $result->{$usercol}
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
  my $dbh     = DBI->connect($connstr,get_conf_val($fuc,$main::Forum,'SQLUser'),get_conf_val($fuc,$main::Forum,'SQLPass'));
  return get_error($fdc, 'SQL', 'connect') unless $dbh;

  my ($table,$usercol,$passwdcol,$emailcol) = sql_data($fuc);

  my $dbq = $dbh->prepare('INSERT INTO '.$table.' ('.$usercol.','.$passwdcol.','.$emailcol.') VALUES (?,?,?)');
  return get_error($fdc, 'SQL', 'execute') unless $dbq;
  $dbq->execute($uname,$pass,$email) or return get_error($fdc, 'SQL', 'execute');
  $dbh->disconnect;
  return undef;
}
# }}}

# require
1;

# eof
