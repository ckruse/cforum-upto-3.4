#!/usr/bin/perl -w

#
# \file testsuite.pl
# \author Christian Kruse, <cjk@wwwtech.de>
# \brief This script creates a "test suite" for the Classic Forum
#
# This script creates a test suite for the Classic Forum. It can do new postings, create new threads
# and request existing postings.
#
# {{{ Initial headers
# $Header: /home/users/cvs/selfforum/testsuite.pl,v 1.3 2003/10/20 08:57:39 ckruse Exp $
# $Log: testsuite.pl,v $
# Revision 1.3  2003/10/20 08:57:39  ckruse
# Heavy developement
#
# Revision 1.2  2003/09/15 07:03:22  ckruse
# Developement in Progress...
#
# Revision 1.1  2003/05/28 20:54:52  ckruse
# heavy developement
# }}}
#

use strict;
use vars qw($VERSION $CONFIG %CHILDS $RUN);

$VERSION = '0.1';
$CONFIG  = {
  verbose => 1,
  'do-new-posts' => 'yes',
  'do-new-threads' => 'yes',
  'request-my-view' => 'yes',
  'request-normal-view' => 'yes',
  'readers' => 2,
  'writers' => 1,
  'read-delay' => 500,
  'write-delay' => 1000,
  'pid-file' => 'logs/fo_pid',
  'threadlist-url' => "http://localhost/forum/",
  'my-threadlist-url' => "http://localhost/forum/my/"
};

%CHILDS = ();
$RUN    = 1;

use Getopt::Long;

use LWP::UserAgent;
use LWP::Simple;
use HTTP::Request;
use HTTP::Request::Common qw(POST);

sub help();
sub reader($);
sub writer($);
sub verbose_message($);

# avoid zombies
$SIG{CHLD} = 'IGNORE';
$SIG{INT}  = sub {
  kill 15,keys %CHILDS;
  $RUN = 0;
};

# first, we get the commandline options
GetOptions(
  $CONFIG,
  'verbose!',
  'threadlist-url=s',
  'posting-url=s',
  'my-threadlist-url=s',
  'my-posting-url=s',
  'my-user=s',
  'my-passwd=s',
  'do-new-posts=s',
  'do-new-threads=s',
  'post-url=s',
  'request-my-view=s',
  'request-normal-view=s',
  'readers=i',
  'writers=i',
  'read-delay=i',
  'write-delay=i',
  'pid-file=s',
  'help' => \&help
);

# setup the socket...

verbose_message("manager pid is: $$\n");

# create the readers
for(1..$CONFIG->{readers}) {
  my $pid = fork();
  die "fork: $!" unless defined $pid;

  unless($pid) {
    verbose_message("created reader number $_ (pid $$)\n");

    if($_ % 2 == 0) {
      reader(1);
    }
    else {
      reader(0);
    }

    exit(0);
  }
  else {
    $CHILDS{$pid} = 1;
  }
}

# create the writers
for(1..$CONFIG->{writers}) {
  my $pid = fork();
  die "fork: $!" unless defined $pid;
  unless($pid) {
    verbose_message("created writer number $_ (pid $$)\n");
    writer(1);
    exit(0);
  }
  else {
    $CHILDS{$pid} = 1;
  }
}

open DAT,'<'.$CONFIG->{'pid-file'} or die "open: $!";
my $SrvPid = <DAT>;
close DAT;

while($RUN) {
  sleep 1;
  unless(kill 0,$SrvPid) {
    print STDERR "hey, server seemed to die!\n";
    kill 2,$$;
  }
}


sub random_data($$$) {
  my ($min,$max,$nl) = (shift,shift,shift);
  #my @rand_chars = (('A'..'Z','a'..'z',0..9),split(//,"#+-*/<>!\"�$%&/'?����\\-_.:,;"));
  my @rand_chars = ('A'..'Z','a'..'z',0..9,' ');
  my $str = '';

  push @rand_chars,"\n" if $nl;

  for(0..($min + int(rand($max - $min)))) {
    $str .= $rand_chars[int(rand @rand_chars)];
  }

  return $str;
}

sub get_fupto($) {
  my $thrlist = shift;
  my @ids = $thrlist =~ m/<a href="[^"]*\?(t=\d+&amp;m=\d+)"/g;

  my $fupto = $ids[rand @ids];
  $fupto =~ s/t=(\d+)&amp;m=(\d+)/$1,$2/;

  return $fupto;
}

sub writer($) {
  my %fields = (
    # fieldname => { min => minlen, max => maxlen, newline => 0|1 }
    unid => { min => 5, max => 10, newline => 0 },
    qchar => "\377> ",
    Name => ["Christian","Erwin","G�nther","Hugo","Harald","Herbert","Ina","Iris","Veronika","Daniela","Daniel","Andres","Gundula","Raimund","Richard","Bernhard","Hinrich","Anne","Stefanie","Christine","Anja"],
    EMail => 'cjk@wwwtech.de',
    cat => ["ZU DIESEM FORUM","ZUR INFO","BROWSER","HTTP","HTML","E-MAIL","DHTML","GRAFIK","PROVIDER","RECHT","DESIGN","INTERNET-ANBINDUNG","FTP","ASP","CGI","CSS","DATENBANK","JAVA","JAVASCRIPT","MEINUNG","MENSCHELEI","PERL","PHP","PROGRAMMIERTECHNIK"],
    subject => { min => 15, max => 20, newline => 0 },
    body => { min => 50, max => 500, newline => 1 }
  );

  die "need post-url!" unless $CONFIG->{'post-url'};

  while(1) {
    my %filled_fields = ();

    foreach my $field (keys %fields) {
      if(!ref($fields{$field})) {
        $filled_fields{$field} = $fields{$field};
      }
      else {
        if(ref $fields{$field} eq 'HASH') {
          $filled_fields{$field} = random_data($fields{$field}->{min},$fields{$field}->{max},$fields{$field}->{newline});
        }
        elsif(ref $fields{$field} eq 'ARRAY') {
          $filled_fields{$field} = $fields{$field}->[rand @{$fields{$field}}];
        }
      }
    }

    if($CONFIG->{'do-new-posts'} eq 'yes' && ($CONFIG->{'do-new-threads'} eq 'no' || rand(100) > 40)) { # 60% chance for an fupto
      my $threadlist = get_url($CONFIG->{'threadlist-url'},0);
      my $fupto      = get_fupto($threadlist);

      $filled_fields{fupto} = $fupto;
    }

    if($CONFIG->{'do-new-posts'} eq 'yes' || $CONFIG->{'do-new-threads'} eq 'yes') {
      my $ua  = new LWP::UserAgent;
      my $rq  = POST $CONFIG->{'post-url'},[%filled_fields];
      my $rsp = $ua->request($rq);

      if($rsp->is_success) {
        my $cnt = $rsp->content;

        if($cnt =~ /SELFHTML Forum - Fehler/) {
          verbose_message("error posting...\n");
        }
        else {
          verbose_message("posted new ".(exists $filled_fields{fupto} ? 'posting (fupto:'.$filled_fields{fupto}.')' : 'thread')."\n");
        }
      }
      else {
        die $rsp->error_as_HTML;
      }
    }
    else {
      exit(0);
    }

    sleep($CONFIG->{'write-delay'}/1000);
  }

}

sub get_url($$) {
  my ($url,$my) = (shift,shift);
  my $cnt = '';

  if($my) {
    die "need authorization credentials!\n" unless defined $CONFIG->{'my-user'} && defined $CONFIG->{'my-passwd'};

    my $lwp = new LWP::UserAgent;
    my $rq  = new HTTP::Request(GET => $url);
    $rq->authorization_basic($CONFIG->{'my-user'},$CONFIG->{'my-passwd'});
    my $res = $lwp->request($rq);

    if($res->is_success) {
      $cnt = $res->content;
    }
    else {
      verbose_message("Error: ".$res->status_line."\n")
    }
  }
  else {
    $cnt = get($url);
  }

  return $cnt;
}

sub reader($) {
  my $my  = shift;
  my $t_url = '';
  my $p_url = '';

  if($my) {
    if($CONFIG->{'request-my-view'} eq 'yes') {
      $t_url = $CONFIG->{'my-threadlist-url'} || 'http://localhost/forum/my/';
      $p_url = $CONFIG->{'my-posting-url'} || $t_url;
    }
    else {
      $t_url = $CONFIG->{'threadlist-url'};
      $p_url = $CONFIG->{'posting-url'} || $t_url;
    }
  }
  else {
    $t_url = $CONFIG->{'threadlist-url'};
    $p_url = $CONFIG->{'posting-url'} || $t_url;
  }

  while(1) {
    verbose_message("getting threadlist (url: $t_url)\n");
    my $threadlist = get_url($t_url,$my);
    if(!$threadlist || $threadlist =~ /<title>SELFHTML Forum: Fehler<\/title>/) {
      die "Server returned error!\n";
    }

    while($threadlist =~ /<a href="[^"]+\?t=(\d+)&amp;m=(\d+)">/) {
      sleep $CONFIG->{'read-delay'} / 1000;
      my $post_url = $p_url.'?t='.$1.'&m='.$2;
      verbose_message("getting url $post_url...\n");
      get_url($post_url,$my);
    }
  }
}

sub help() {
  print <<HELP;
Usage:
  $0 [options]

Valid options are:
  --verbose                    Turn on verbose messages (default)
  --noverbose                  Turn off verbose messages
  --threadlist-url=URL         The URL to the threadlist script
  --posting-url=URL            The URL to the posting script. By default, the
                               URL will be extracted from the threadlist
  --my-threadlist-url=URL      The URL to the user defined view of the
                               threadlist
  --my-posting-url=URL         The URL to the user defined view to postings. By
                               default, the URL will be extracted from the
                               threadlist.
  --my-user=USERNAME           The username used for the authentification
  --my-passwd=PASSWD           The password used for the authentification
  --do-new-posts=yes|no        Create new postings (random data!) (default yes)
  --do-new-threads=yes|no      Create new threads (random data!) (default yes)
  --request-my-view=yes|no     Do requests to the userdefined views
  --request-normal-view=yes|no Do requests to the normal, anonymous view
  --readers                    The number of processes going to read
                               postings (default 5)
  --writers                    The number of processes going to write new
                               threads/postings (default 2)
  --read-delay                 The number of microseconds to wait between
                               two read calls (default 500, a half of a second)
  --write-delay                The number of microseconds to wait between
                               two write calls (default 1000, one second)
  --pid-file                   The path to the PID file of the fo_server
                               program (default: ./logs/fo_pid)
  --post-url=URL               The URL where to send the new postings/threads to
  --help                       Print this help screen

HELP

  exit(0);
}

sub verbose_message($) {
  print shift if($CONFIG->{verbose});
}

# eof
