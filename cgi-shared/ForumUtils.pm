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
    message_field
    transform_body
    gen_time
    rel_uri
    get_template
    encode
    recode
    uniquify_params
    fatal
    plaintext
    decode
    get_config_file
    create_directory_structure
  );
}

sub VERSION {(q$Revision: 1.22 $ =~ /([\d.]+)\s*$/)[0] or '0.0'}

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

  return unless defined $str;
  $str = $Clientlib->htmlentities_charset_convert($str,"UTF-8",$fdc->{ExternCharset}->[0]->[0],0) if $fdc->{ExternCharset}->[0]->[0] ne "UTF-8";

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

# {{{ get_config_file
sub get_config_file {
  my $default_conf = shift;
  my $uname        = shift;

  my $cfgfile = sprintf '%s%s/%s/%s/%s%s',$default_conf->{ConfigDirectory}->[0]->[0],substr($uname,0,1),substr($uname,1,1),substr($uname,2,1),$uname,'.conf';

  return $cfgfile;
}
# }}}

# {{{ create_directory_structure
sub create_directory_structure {
  my $default_conf = shift;
  my $uname        = shift;

  my $dir = $default_conf->{ConfigDirectory}->[0]->[0];
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

  if($user_config) {
    if(exists $user_config->{TPLMode}) {
      return sprintf($tplname,$fo_default_conf->{Language}->[0]->[0],$user_config->{TPLMode}->[0]->[0]) if $user_config->{TPLMode}->[0]->[0];
    }
  }

  return sprintf($tplname,$fo_default_conf->{Language}->[0]->[0],$fo_default_conf->{TemplateMode}->[0]->[0]);
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

  setlocale(LC_ALL,$dcfg->{DateLocale}->[0]->[0]);
  return strftime($format,localtime($time));
}
# }}}

# {{{ transform_body
sub transform_body {
  my $dcfg   = shift;
  my $pcfg   = shift;
  my $txt    = shift;
  my $qchars = recode($dcfg,shift);
  my $base   = $ENV{SCRIPT_NAME};

  $base =~ s![^/]*$!!; #!;

  #
  # the following section contains lot of code from André Malo -- thanks to him
  #

  # first we transform all newlines to \n
  $txt =~ s/\015\012|\015|\012/\n/g;

  # after that, we collect all links to postings...
  foreach(@{$pcfg->{PostingUrl}}) {
    $txt =~ s{\[link:\s*$_->[0]([\dtm=&]+)(?:#\w+)?\]}{my $tidpid = $1; $tidpid =~ s!&!;!; '[pref:'.$tidpid.']'}eg;
  }

  # encode to html (entities, and so on -- if we do it once,
  # we don't need to do it every time a message will be viewed)
  $txt = recode($dcfg,$txt);

  # now transform...

  # ... messages
  foreach(@{$pcfg->{Image}}) {
    my ($name,$url,$alt) = (quotemeta $_->[0],recode($dcfg,$_->[1]),recode($dcfg,$_->[2]));
    $txt =~ s!\[[mM][sS][gG]:\s*$name\]![image:$url\@alt=$alt]!g;
  }

  # ... all quoting characters to \177
  my $len = length $qchars;
  $txt =~ s!^((?:\Q$qchars\E)+)!"\177" x (length($1)/$len)!gem if $len;

  # save the sig
  $txt =~ s!\n-- \n!_/_SIG_/_!;

  # after that we kill all whitespaces at the end of the line
  $txt =~ s/[^\S\n]+$//gm;

  # now kill all newlines at the end of the string
  $txt =~ s/\s+$//;

  # now lets transform \n to <br />
  $txt =~ s!\n!<br />!g;

  # transform more than one space to &nbsp; (ascii art, etc)
  $txt =~ s/(\s\s+)/'&nbsp;' x length($1)/eg;

  # spaces after a <br /> have to be &nbsp;
  $txt =~ s!(?:^|(<br(?:\s*/)?>))\s!($1?$1:'').'&nbsp;'!eg;

  # after that - transform it to UTF-8
  $txt = $Clientlib->charset_convert($txt,length($txt),$dcfg->{ExternCharset}->[0]->[0],"UTF-8") if $dcfg->{ExternCharset}->[0]->[0] ne "UTF-8";

  return $txt;
}
# }}}

# {{{ message_field
sub message_field {
  my $posting = shift;
  my $qchar   = shift;
  my $fdcfg   = shift;
  my $archive = shift;

  my $break   = '<br />';
  my $xhtml   = $fdcfg->{XHTMLMode} && $fdcfg->{XHTMLMode}->[0]->[0] eq 'yes';

  my $base   = $ENV{SCRIPT_NAME};

  $base =~ s![^/]*$!!; #!;

  #
  # maybe not as fast as the previous code, but even
  # more elegant
  #
  my $txt   = '';
  my $qmode = 0;
  for(my $i=0;$i<length($posting);$i++) {
    if(substr($posting,$i,1) eq "\177") {
      $txt .= '<span class="q">' unless $qmode;
      $txt  .= recode($fdcfg,$qchar);
      $qmode = 1;
    }
    elsif(substr($posting,$i,6) eq '<br />') {
      $txt .= $xhtml ? '<br />' : '<br>';

      if($qmode && substr($posting,$i+6,1) ne "\177") {
        $txt .= '</span>';
        $qmode = 0;
      }

      $i += 5;
    }
    else {
      $txt .= substr($posting,$i,1);
    }
  }

  $posting = $txt;

#  $posting =~ s/\177/recode($fdcfg,$qchar)/eg; # \177 => quote chars
  $posting =~ s!_/_SIG_/_(.*)!$break<span class="sig">-- $break$1</span>!s;

  # we want all posting refs to be transformed to links
  my $posturl;
  if($main::UserName) {
    $posturl = $fdcfg->{UPostingURL}->[0]->[0];
  }
  else {
    $posturl = $fdcfg->{PostingURL}->[0]->[0];
  }

  $posting =~ s{\[pref:t=(\d+);m=(\d+)\]}{
    my $txt = $posturl;
    my ($tid,$mid) = ($1,$2);
    $txt =~ s!\%t!$tid!g;
    $txt =~ s!\%m!$mid!g;
    '<a href="'.$txt.'">'.$txt.'</a>';
  }eg;


  # Phase 1: collect links, images, etc, pp

  # this is much faster than the code used before
  # (efficience analysis is relly nice :-)

  my @links = ();
  while($posting =~ /\[[Ll][Ii][Nn][Kk]:\s*([^\]\s]+?)\s*(?:\@title=([^\]]+)\s*)?\]/g) {
    my ($uri,$title) = ($1,$2,$3);
    next if
      !is_valid_link($uri) &&
      !is_valid_http_url(($uri =~ /^[Vv][Ii][Ee][Ww]-[Ss][Oo][Uu][Rr][Cc][Ee]:(.+)/)[0],CForum::Validator::VALIDATE_STRICT) &&
      !($uri =~ m{^(?:\.?\.?/(?!/)|\?)} and is_valid_http_url(rel_uri($uri,$base)));

    push @links,[$uri,$title];
  }

  my @images = ();
  while($posting =~ /\[[Ii][Mm][Aa][Gg][Ee]:\s*([^\]\s]+?)\s*(?:\@alt=([^\]]+)\s*)?\]/g) {
    my ($uri,$alt) = ($1,$2);
    next if
      !is_valid_http_url($uri,CForum::Validator::VALIDATE_STRICT) &&
      !($uri =~ m{^(?:\.?\.?/(?!/)|\?)} and is_valid_http_url(rel_uri($uri, $base),CForum::Validator::VALIDATE_STRICT));

    push @images,[$uri,$alt];
  }

  my @iframes = ();
  while($posting =~ /\[[Ii][Ff][Rr][Aa][Mm][Ee]:\s*([^\]\s]+)\s*\]/g) {
    my $uri = $1;
    next if
      !is_valid_http_url($uri,CForum::Validator::VALIDATE_STRICT) &&
      !($uri =~ m{^(?:\.?\.?/(?!/)|\?)} and is_valid_http_url(rel_uri($uri, $base),CForum::Validator::VALIDATE_STRICT));

    push @iframes,$uri;
  }

  # Phase 2: Ok, we collected the links, lets transform them
  # ... links
  $posting =~ s!$_!'<a href="'.$1.'">'.($2||$1).'</a>'!eg for map {
    '\[[Ll][Ii][Nn][Kk]:\s*('.
    quotemeta(recode($fdcfg,$_->[0])).
    ')'.
    ($_->[1] ? '\s*\@title=('.quotemeta(recode($fdcfg,$_->[1])).')' : '').
    '\s*\]'
  } @links;

  # ... images
  $posting =~ s!$_!'<img src="'.$1.'" border="0" alt="'.($2?$2:'').'">'!eg for map {
    '\[[Ii][Mm][Aa][Gg][Ee]:\s*('.
    quotemeta(recode($fdcfg,$_->[1])).
    ')'.
    ($_->[1] ? '\s*\@alt=('.quotemeta(recode($fdcfg,$_->[1])).')' : '').
    '\s*\]'
  } @images;

  # ... iframes
  $posting =~ s!$_!<iframe src="$1" width="90%" height="90%"><a href="$1">$1</a></iframe>! for map {
    '\[[Ii][Ff][Rr][Aa][Mm][Ee]:\s*('.quotemeta(recode($fdcfg,$_->[1])).')\]'
  } @iframes;

  # return
  #
  return $posting;
}
# }}}

# {{{ plaintext
sub plaintext {
  my $posting = shift;
  my $qchar   = shift;
  my $pcfg    = shift;

  $posting =~ s!<a href="([^"]+)">\1</a>![link:$1]!g;
  $posting =~ s!<img src="([^"]+)" border="0" alt="">![image:$1]!g;
  $posting =~ s!<iframe src="([^"]+)" width="90%" height="90%"><a href="\1">\1</a></iframe>![iframe:$1]!g;

  foreach($pcfg->{Image}) {
    my ($name,$url,$alt) = (quotemeta $_->[0],quotemeta encode $_->[1],quotemeta encode $_->[2]);
    $posting =~ s!<img src="$url" alt="$alt">![msg:$name]!g;
  }

  $posting =~ s!\177!$qchar!g;
  $posting =~ s!<br />!\n!g;
  $posting =~ s!&nbsp;! !g;
  $posting =~ s!&#39;!'!g;
  $posting =~ s!_/_SIG_/_!\n-- \n!;

  return decode $posting;
}
# }}}

# {{{ generate_unid
sub generate_unid {
  my $rmid  =  $ENV{HTTP_X_FORWARED_FOR} || $ENV{REMOTE_ADDR} || '654.546.654.546';
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

  my $val = $cgi->param($fname) or return get_error($dcfg,'manipulated');

  # thanks to André Malo for the following peace of code (great idea):
  # is the given charset UTF-8?
  if($val =~ /^\303\277/) {
    # seems so, we have to check if all input is valid UTF-8; there
    # are many broken browsers which send e.g. binary input not
    # well-encoded as UTF-8
    foreach($cgi->param) {
      my @values = $cgi->param($_);

      foreach my $val (@values) {
        return get_error($dcfg,'posting','charset') unless $Clientlib->is_valid_utf8_string($val,length($val));
      }
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
        if($dcfg->{ExternCharset}->[0]->[0] eq 'ISO-8859-1') {
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
            $dcfg->{ExternCharset}->[0]->[0],
            "UTF-8"
          );
        }

        return get_error($dcfg,'posting','charset') if !defined $nval;
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

  unless($Msgs) {
    $Msgs = new BerkeleyDB::Btree(
      -Filename => $dcfg->{MessagesDatabase}->[0]->[0],
      -Flags => DB_RDONLY
    ) or return 'Bad database error, go away';
  }

  my $id = $dcfg->{Language}->[0]->[0].'_E_'.($variant ? $err.'_'.$variant : $err);

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
  my $line = shift;

  return 1 if $line =~ /^\s*#/;

  if($line =~ s/^\s*([A-Za-z0-9]+)//) {
    my $directive = $1;
    my $i         = 0;
    my @vals      = ();

    $cfg->{$directive} = [] unless exists $cfg->{$directive};

    for($i=0;$i<length($line);$i++) {
      my $c = substr($line,$i,1);
      if($c eq '"') {
        my $val = read_string($line,\$i);
        push @vals,$val;
      }
    }

    if($#vals != -1) {
      push @{$cfg->{$directive}},[@vals];
      return 1;
    }
    else {
      return;
    }

  }

  return;

}
# }}}

# {{{ read_configuration
sub read_configuration {
  my $cfgfile = shift;
  my $cfg     = {};
  local *DAT;

  open DAT,'<'.$cfgfile or return;

  while(<DAT>) {
    next if m/^\s*(\#|$ )/x;
    next if m/^\s*</;
    next unless parse_argument($cfg,$_);
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

  if(defined $sock) {
    print $sock "QUIT\n";
    close($sock);
  }

  my $tpl  = new CForum::Template($ftpl);
  $tpl->setVar('forumbase',$ENV{REMOTE_USER} ? $dcfg->{UBaseURL}->[0]->[0] : $dcfg->{BaseURL}->[0]->[0]);
  $tpl->setVar('err',recode($dcfg,$err));

  $tpl->setVar('charset',$dcfg->{ExternCharset}->[0]->[0]);
  $tpl->setVar('acceptcharset',$dcfg->{ExternCharset}->[0]->[0].', UTF-8');
  print $cgi->header(-type => 'text/html; charset='.$dcfg->{ExternCharset}->[0]->[0]) unless $hdr;
  print $tpl->parseToMem;

  exit;
}
# }}}

# require
1;
# eof
