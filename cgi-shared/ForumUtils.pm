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

use Unicode::MapUTF8 qw(from_utf8 to_utf8);

# needed because of this fucking Windows-1252
use Text::Iconv;

use HTML::Entities;
use CForum::Template;

use CheckRFC;

use POSIX qw/setlocale strftime LC_ALL/;

# }}}

# {{{ recode
sub recode {
  my $fdc = shift; # default config
  my $str = shift;

  return unless defined $str;
  $str = from_utf8(-string => $str,-charset => $fdc->{ExternCharset}->[0]->[0]) if $fdc->{ExternCharset}->[0]->[0] ne "UTF-8";

  return encode($str);
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
      return sprintf($tplname,$user_config->{TPLMode}->[0]->[0]) if $user_config->{TPLMode}->[0]->[0];
    }
  }

  return sprintf($tplname,"") unless $fo_default_conf->{TemplateMode};
  return sprintf($tplname,$fo_default_conf->{TemplateMode}->[0]->[0]);
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

  $base =~ s![^/]*$!!;

  #
  # the following section contains lot of code from André Malo -- thanks to him
  #

  # first we transform all newlines to \n
  $txt =~ s/\015\012|\015|\012/\n/g;

  # after that, we collect all links to postings...
  foreach(@{$pcfg->{PostingUrl}}) {
    $txt =~ s~\[link:\s*$_->[0]([\dtm=&]+)(?:#\w+)?\]~\[pref:$1\]~g;
  }

  # now transform all links...
  my @links = ();
  push @links,[$1, $2] while $txt =~ /\[([Ll][Ii][Nn][Kk]):\s*([^\]\s]+)\s*\]/g;
  @links = grep {
    is_URL($_->[1] => qw(http ftp news nntp telnet gopher mailto))
      or is_URL(($_->[1] =~ /^[Vv][Ii][Ee][Ww]-[Ss][Oo][Uu][Rr][Cc][Ee]:(.+)/)[0] || '' => 'http')
      or ($_->[1] =~ m<^(?:\.?\.?/(?!/)|\?)> and is_URL(rel_uri($_ -> [1],$base) => 'http'))
    } @links;

  # lets collect all images
  my @images = ();
  push @images, [$1, $2] while $txt =~ /\[([Ii][Mm][Aa][Gg][Ee]):\s*([^\]\s]+)\s*\]/g;
  @images = grep {
    is_URL($_->[1] => 'strict_http')
      or ($_->[1] =~ m<^(?:\.?\.?/(?!/)|\?)> and is_URL(rel_uri($_->[1], $base) => 'http'))
  } @images;

  # lets collect all iframes
  my @iframes;
  push @iframes,[$1, $2] while $txt =~ /\[([Ii][Ff][Rr][Aa][Mm][Ee]):\s*([^\]\s]+)\s*\]/g;
  @iframes = grep {
    is_URL($_ -> [1] => 'http')
    or ($_ -> [1] =~ m<^(?:\.?\.?/(?!/)|\?)> and is_URL (rel_uri($_ -> [1], $base) => 'http'))
  } @iframes;

  # encode to html
  $txt = recode($dcfg,$txt);

  # now transform...

  # ... links
  $txt =~ s!$_!<a href="$1">$1</a>!g for map {
    '\[[Ll][Ii][Nn][Kk]:\s*('.quotemeta(recode($dcfg,$_->[1])).')\]'
  } @links;

  # ... images
  $txt =~ s!$_!<img src="$1" border="0" alt="">!g for map {
    '\[[Ii][Mm][Aa][Gg][Ee]:\s*('.quotemeta(recode($dcfg,$_->[1])).')\]'
  } @images;

  # ... iframes
  $txt =~ s!$_!<iframe src="$1" width="90%" height="90%"><a href="$1">$1</a></iframe>! for map {
    '\[[Ii][Ff][Rr][Aa][Mm][Ee]:\s*('.quotemeta(recode($dcfg,$_->[1])).')\]'
  } @iframes;

  # ... messages
  foreach(@{$pcfg->{Image}}) {
    my ($name,$url,$alt) = (quotemeta $_->[0],recode($dcfg,$_->[1]),recode($dcfg,$_->[2]));
    $txt =~ s!\[[mM][sS][gG]:\s*$name\]!<img src="$url" alt="$alt">!g;
  }

  # now transform all quoting characters to \177
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
  $txt =~ s/(\s\s+)/('&nbsp;' x (length($1)-1)) . ' '/eg;

  # spaces after a <br /> have to be &nbsp;
  $txt =~ s!(?:^|(<br(?:\s*/)?>))\s!($1?$1:'').'&nbsp;'!eg;

  # after that - transform it to UTF-8
  $txt = to_utf8(-string => $txt,-charset => $dcfg->{ExternCharset}->[0]->[0]) if $dcfg->{ExternCharset}->[0]->[0] ne "UTF-8";

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

  my @array = [0 => []];

# {{{
#  for (split /<br(?:\s*\/)?>/ => $posting) {
#    my $l = length ((/^(\177*)/)[0]);
#    if ($array[-1][0] == $l) {
#      push @{$array[-1][-1]} => $_;
#    }
#    else {
#      push @array => [$l => [$_]];
#    }
#  }
#  shift @array unless @{$array[0][-1]};
#
#  my $ll=0;
#  $posting = join $break => map {
#    my $string = $_->[0]
#      ? (($ll and $ll != $_->[0]) ? $break : '') .
#        join join ($break => @{$_->[-1]})
#          => ('<span class="q">', '</span>')
#            : (join $break => @{$_->[-1]});
#    $ll = $_->[0]; $string;
#  } @array;
# }}}

  #
  # maybe not as fast as the previous code, but even
  # more elegant
  #
  my $txt   = '';
  my $qmode = 0;
  for(my $i=0;$i<length($posting);$i++) {
    if(substr($posting,$i,1) eq "\177") {
      if(!$qmode) {
        $txt .= '<span class="q">';
      }

      $txt  .= recode($fdcfg,$qchar);
      $qmode = 1;
    }
    elsif(substr($posting,$i,6) eq '<br />') {
      $txt .= '<br />';
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
  unless(defined $archive) {
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
      $txt;
    }eg;
  }

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
  my $converter;

  my $val = $cgi->param($fname) or return get_error($dcfg,'manipulated');

  # thanks to André Malo for the following peace of code (great idea):
  # is the given charset UTF-8?
  unless($val =~ /^\303\277/) {
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
          # Windows-1252 has been sent
          if($val =~ /[\x80-\x9F]/) {
            $convert = 0;
            $converter = new Text::Iconv("Windows-1252","UTF-8") unless $converter;

            $nval = $converter->convert($val);
          }
        }

        if($convert) {
          $nval = to_utf8({
            -string => $val,
            -charset => $dcfg->{ExternCharset}->[0]->[0]
          });
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
  local *DAT;

  return 'error not given' unless $err;

  open(DAT,'<'.$dcfg->{ErrorMessages}->[0]->[0]) or die $!;

  while(<DAT>) {
    if(m!^E_$err:!i or m!^E_${err}_$variant:!i) {
      my $val = $_;
      $val =~ s/^\S+: //;
      close DAT and return $val;
    }
  }

  close DAT and return 'error not found: '.$err.' (variant: '.$variant.')';
  return 'Oops. This point should never be reached...';
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
