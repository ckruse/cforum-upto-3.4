package ForumUtils;

# {{{ initial comments
#
# $LastChangedDate$
# $LastChangedRevision$
# $LastChangedBy$
#
# }}}

# {{{ program headers
use strict;
use vars qw($VERSION @ISA @EXPORT_OK);

BEGIN {
  $VERSION   = '0.1';
  @ISA       = qw(Exporter);
  @EXPORT_OK = qw(
    read_configuration
    get_node_data
    generate_unid
    get_error
    decode_params
    gen_time
    rel_uri
    get_template
    encode
    recode
    uniquify_params
    fatal
    decode
    get_config_files
    get_user_config_file
    get_conf_val
    create_directory_structure
  );
}

sub VERSION {(q$Revision$ =~ /([\d.]+)\s*$/)[0] or '0.0'}

# needed because of this fucking Windows-1252
use Text::Iconv;

use BerkeleyDB;
use POSIX qw/setlocale/;

use HTML::Entities;

use CForum::Template;
use CForum::Clientlib;
use CForum::Validator;

use POSIX qw/setlocale strftime LC_ALL/;

my $Msgs = undef;
my $Clientlib = new CForum::Clientlib;

# }}}

# {{{ recode
sub recode {
  my $fdc = shift; # default config
  my $str = shift;
  my $cs = get_conf_val($fdc,$main::Forum,'ExternCharset');

  return unless defined $str;
  if($cs eq 'UTF-8') {
    $str = $str ? $Clientlib->htmlentities($str,0) : '';
  }
  else {
    $str = $Clientlib->htmlentities_charset_convert($str,"UTF-8",$cs,0) if $cs ne "UTF-8";
  }

  return $str;
}
# }}}

# {{{ encode
sub encode {
  my $str = shift;

  $str =~ s/&/&amp;/g;
  $str =~ s/</&lt;/g;
  $str =~ s/>/&gt;/g;
  $str =~ s/"/&quot;/g;

  return $str;
}
# }}}

# {{{ decode
sub decode {
  my $str = shift;

  $str =~ s!&lt;!<!g;
  $str =~ s!&gt;!>!g;
  $str =~ s!&quot;!"!g;
  $str =~ s!&amp;!&!g;

  return $str;
}
# }}}

# {{{ get_config_files
sub get_config_files {
  return unless $ENV{'CF_CONF_DIR'};
  my @ret = ();

  foreach my $fname (@_) {
    my $f = $ENV{'CF_CONF_DIR'}.'/'.$fname.'.conf';
    return unless -f $f || !-r $f;

    push @ret,$f;
  }

  return @ret;
}
# }}}

# {{{ get_user_config_file
sub get_user_config_file {
  my $default_conf = shift;
  my $uname        = shift;

  my $cfgfile = sprintf '%s%s/%s/%s/%s%s',get_conf_val($default_conf,$main::Forum,'ConfigDirectory'),substr($uname,0,1),substr($uname,1,1),substr($uname,2,1),$uname,'.conf';

  return $cfgfile;
}
# }}}

# {{{ create_directory_structure
sub create_directory_structure {
  my $default_conf = shift;
  my $uname        = shift;

  my $dir = get_conf_val($default_conf,$main::Forum,'ConfigDirectory');
  $dir =~ s!/$!!;

  for(0..2) {
    $dir .= '/'.substr($uname,$_,1);
    next if -d $dir;

    mkdir $dir,0771 or do {
      print STDERR $!;
      return;
    };
  }

  return 1;
}
# }}}

# {{{ get_template
sub get_template {
  my ($fo_default_conf,$user_config,$tplname) = (shift,shift,shift);
  my $tplmode = get_conf_val($user_config,'global','TPLMode');
  my $lang = get_conf_val($fo_default_conf,$main::Forum,'Language');
  my $tplmode_global = get_conf_val($fo_default_conf,$main::Forum,'TemplateMode');

  if($user_config) {
    return sprintf($tplname,$lang,$tplmode) if $tplmode;
  }

  return sprintf($tplname,$lang,$tplmode_global);
}
# }}}

# {{{ rel_uri
sub rel_uri {
  my ($uri, $base) = @_;

  "http://$ENV{HTTP_HOST}".
    ($uri =~ m|^/|
     ? $uri
     : "$base$uri");
}
# }}}

# {{{ gen_time
sub gen_time {
  my $time   = shift;
  my $dcfg   = shift;
  my $format = shift;
  my $dl = get_conf_val($dcfg,$main::Forum,'DateLocale');

  setlocale(LC_ALL,$dl);
  return strftime($format,localtime($time));
}
# }}}

# {{{ generate_unid
sub generate_unid {
  my $rmid  =  $ENV{HTTP_X_FORWARDED_FOR} || $ENV{REMOTE_ADDR} || '654.546.654.546';
  my @chars = split // => 'aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ0123456789-_';
  my $id    = '';

  $rmid =~ tr!.!0!;

  $id .= $chars[$_] foreach split // => $rmid;
  $id .= $chars[rand @chars] for 1..10;

  return $id;
}
# }}}

# {{{ uniquify_params
sub uniquify_params {
  my $dcfg  = shift;
  my $cgi   = shift;
  my $fname = shift;
  my $cs    = get_conf_val($dcfg,$main::Forum,'ExternCharset');

  my $val = $cgi->param($fname) or return get_error($dcfg,'manipulated');

  # thanks to André Malo for the following peace of code (great idea):
  # is the given charset UTF-8?
  if($val =~ /^\303\277/) {
    # seems so, we have to check if all input is valid UTF-8; there
    # are many broken browsers which send e.g. binary input not
    # well-encoded as UTF-8
    foreach($cgi->param) {
      my @nvals  = ();
      my @values = $cgi->param($_);

      foreach my $val (@values) {
        return get_error($dcfg,'posting','charset') unless $Clientlib->is_valid_utf8_string($val,length($val));

        # we want non-breaking space and unicode whitespaces as normal whitespaces
        $val =~ s/\xC2\xA0|\xE2\x80[\x80-\x8B\xA8-\xAF]/ /g;
        push @nvals,$val;
      }

      $cgi->param(-name => $_,-value => \@nvals);
    }
  }
  else {
    foreach($cgi->param) {
      my @values  = $cgi->param($_);
      my @newvals = ();

      foreach my $val (@values) {
        next if !defined $val || $val eq '';
        my $convert = 1;
        my $nval;

        # browsers are broken :(
        if($cs eq 'ISO-8859-1') {
          # Ok, we got characters not present in Latin-1. Due to
          # our knowledge of the browser bugs we assume that
          # Windows-1252 has been sent; THIS IS JUST A HACK!
          if($val =~ /[\x7F-\x9F]/) {
            $convert = 0;
            $nval = $Clientlib->charset_convert($val,length($val),"Windows-1252","UTF-8");
          }
        }

        if($convert) {
          $nval = $Clientlib->charset_convert(
            $val,
            length($val),
            $cs,
            "UTF-8"
          );

        }

        return get_error($dcfg,'posting','charset') if !defined $nval;

        # we want non-breaking space and unicode whitespaces as normal whitespaces
        $nval =~ s/\xC2\xA0|\xE2\x80[\x80-\x8B\xA8-\xAF]/ /g;

        push @newvals,$nval;
      }

      $cgi->param(-name => $_,-value => \@newvals);
    }
  }

  return;
}
# }}}

# {{{ decode_params
sub decode_params {
  my $dcfg = shift;
  my $pcfg = shift;
  my $cgi  = shift;
  my $err  = '';

  $err = uniquify_params($dcfg,$cgi,'qchar');
  return $err if $err;

  # remove the &#255; (encoded as UTF8) from quotechars
  my $c = $cgi->param('qchar');
  $cgi->param('qchar' => substr($c,2));

  return;
}
# }}}

# {{{ get_error
sub get_error {
  my ($dcfg,$err) = (shift,shift);
  my $variant     = shift || '';
  my $msgdb       = get_conf_val($dcfg,$main::Forum,'MessagesDatabase');
  my $lang        = get_conf_val($dcfg,$main::Forum,'Language');

  unless($Msgs) {
    $Msgs = new BerkeleyDB::Btree(
      -Filename => $msgdb,
      -Flags => DB_RDONLY
    ) or return 'Bad database error, go away';
  }

  my $id = $lang.'_E_'.($variant ? $err.'_'.$variant : $err);

  my $msg = '';
  my $rc = $Msgs->db_get($id,$msg);
  return $msg||'Error not found: '.$id;
}
# }}}

# {{{ read_string
sub read_string {
  my $line = shift;
  my $i    = shift;
  my $val  = "";

  for($$i++;$$i<length($line);$$i++) {
    my $c = substr($line,$$i,1);
    if($c eq '\\') {
      my $c = substr($line,$$i+1,1);

      if($c eq 'n') {
        $val .= "\n";
      }
      elsif($c eq 't') {
        $val .= "\t";
      }
      else {
        $val .= $c;
      }

      $$i++;
    }
    elsif($c eq '"') {
      return $val;
    }
    else {
      $val .= $c;
    }
  }

}
# }}}

# {{{ parse_argument
sub parse_argument {
  my $cfg  = shift;
  my $context = shift;
  my $line = shift;

  return 1 if $line =~ /^\s*#/;

  if($line =~ s/^\s*([A-Za-z0-9]+)//) {
    my $directive = $1;
    my $i         = 0;
    my @vals      = ();

    $cfg->{$context}->{$directive} = [] unless exists $cfg->{$context}->{$directive};

    for($i=0;$i<length($line);$i++) {
      my $c = substr($line,$i,1);
      if($c eq '"') {
        my $val = read_string($line,\$i);
        push @vals,$val;
      }
    }

    if(@vals != 0) {
      push @{$cfg->{$context}->{$directive}},[@vals];
      return 1;
    }
  }

  return;

}
# }}}

# {{{ get_conf_val
sub get_conf_val {
  my $conf = shift;
  my $context = shift;
  my $name = shift;

  return unless $conf->{$context};
  return unless $conf->{$context}->{$name};

  my $vals = $conf->{$context}->{$name};
  if(wantarray) {
    return @$vals if @$vals > 1;
    return @{$vals->[0]} if @{$vals->[0]} > 0;
  }
  return $vals->[0]->[0];
}
# }}}

# {{{ read_configuration
sub read_configuration {
  my $cfgfile = shift;
  my $cfg     = {};
  local *DAT;
  my $context = 'global';

  open DAT,'<'.$cfgfile or return;

  while(<DAT>) {
    next if m/^\s*(\#|$ )/x;
    next if m/^\s*</;
    if(m/\s*Forum\s+(\S+)\s+{/) {
      $context = $1;
      next;
    }
    elsif(m/\s*}$/) {
      $context = 'global';
      next;
    }

    next unless parse_argument($cfg,$context,$_);
  }

  close DAT and return $cfg;

  return;
}
# }}}

# {{{ get_node_data
sub get_node_data {
  my $node = shift;

  if($node) {
    my $chld = $node->getFirstChild;

    if($chld) {
      return $chld->getData() || '';
    }

    return $node->getData() || '';
  }

  return '';
}
# }}}

# {{{ fatal
sub fatal {
  my $cgi  = shift;
  my $dcfg = shift;
  my $ucfg = shift;
  my $err  = shift;
  my $ftpl = get_template($dcfg,$ucfg,shift);
  my $sock = shift;
  my $hdr  = shift;
  my $cs   = get_conf_val($dcfg,$main::Forum,'ExternCharset');
  my $burl = get_conf_val($dcfg,$main::Forum,$main::UserName?'UBaseURL':'BaseURL');

  if(defined $sock) {
    print $sock "QUIT\n";
    close($sock);
  }

  my $tpl  = new CForum::Template($ftpl);
  $tpl->setVar('forumbase',);
  $tpl->setVar('err',recode($dcfg,$err));

  $tpl->setVar('charset',$cs);
  $tpl->setVar('acceptcharset',$cs.', UTF-8');
  print $cgi->header(-type => 'text/html; charset='.$cs) unless $hdr;
  print $tpl->parseToMem;

  exit;
}
# }}}

# require
1;
# eof
