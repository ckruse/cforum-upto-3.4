package CheckRFC;

################################################################################
#                                                                              #
# File:        shared/CheckRFC.pm                                              #
#                                                                              #
# Authors:     André Malo <nd@o3media.de>                                      #
#                                                                              #
# Description: implement several string checks on RFC correctness              #
#                                                                              #
################################################################################

use strict;
use vars qw(
  $v56
  %url
  @email
  @EXPORT
  @ISA
);

$^W = 0;
$v56 = eval q[
  local $SIG{__DIE__};
  require 5.6.0;
];
$^W = 1;

use Carp qw(croak);

################################################################################
#
# Version check
#
# last modified:
#    $Date$ (GMT)
# by $Author$
#
sub VERSION {(q$Revision: 1.2 $ =~ /([\d.]+)\s*$/)[0] or '0.0'}

################################################################################
#
# Export
#
require Exporter;
@ISA = qw(Exporter);

@EXPORT = qw(
  is_URL
  is_email
);

### is_URL ($@) ################################################################
#
# check URL
#
# Params: $string  string to check
#         @schemes possible URL schemes in $string
#                  qw( http strict_http ftp news nntp telnet
#                      gopher wais mailto strict_mailto file prospero)
#                  if there's no scheme given, 'http' is default
#                  use ':ALL' (without quotes) for all schemes
#
# Return: Status code (Bool)
#
sub is_URL (;$@) {
  my ($string, @schemes) = @_;
  $string = $_ unless defined $string;
  my $scheme;

  return unless (defined ($string) and length ($string));

  @schemes = qw(http) unless (@schemes);
  @schemes = keys %url if (@schemes == 1 and $schemes[0] eq ':ALL');

  for $scheme (@schemes) {
    croak "unknown url scheme '$scheme'" unless exists $url{$scheme};
    unless ($scheme =~ /mailto/) {
      return 1 if ($string =~ /$url{$scheme}/);
    }
    else {
      if ($string =~ /^mailto:(.+)/) {

        if ($scheme eq 'mailto') {
          return 1 if (is_email ($1));
        }
        elsif ($scheme eq 'strict_mailto') {
          return 1 if (is_email ($1,1));
        }
      }
    }
  }

  # no match => return false
  return;
}

### is_email ($) ###############################################################
#
# check email (comments can be nested)
#
# Params: $string - string to check
#         $strict - (optional) check strict RFC syntax (no TLD needed) if true
#
# Return: Status code (Bool)
#
sub is_email (;$$) {
  my $string = shift;
  $string = $_ unless defined $string;
  return unless defined $string;

  my $strict = shift;

  # false if any non-ascii chars
  return unless (defined ($string) and length ($string));
  return if $string =~ /[\200-\377]/;

  # remove nested comments
  1 while ($string =~ s/\([^()]*\)//g);

  return ($string =~ /^$email[0]$/) unless $strict;

  return ($string =~ /^$email[1]$/);
}

### BEGIN # (1) ################################################################
#
# define regex for nearly RFC 822 email address
#
BEGIN {
  # Thanx to J. Friedl:

  my $esc         = '\\\\';
  my $Period      = '\.';
  my $space       = '\040';
  my $tab         = '\t';
  my $OpenBR      = '\[';
  my $CloseBR     = '\]';
  my $OpenParen   = '\(';
  my $CloseParen  = '\)';
  my $NonASCII    = '\x80-\xff';
  my $ctrl        = '\000-\037';
  my $CRlist      = '\n\015';
  my $qtext       = qq/[^$esc$NonASCII$CRlist\"]/;
  my $dtext       = qq/[^$esc$NonASCII$CRlist$OpenBR$CloseBR]/;
  my $quoted_pair = qq< $esc [^$NonASCII] >;
  my $ctext       = qq< [^$esc$NonASCII$CRlist()] >;
  my $Cnested     = qq< $OpenParen $ctext* (?: $quoted_pair $ctext* )* $CloseParen >;
  my $comment     = qq< $OpenParen $ctext* (?: (?: $quoted_pair | $Cnested ) $ctext* )* $CloseParen >;
  my $X           = qq< [$space$tab]* (?: $comment [$space$tab]* )* >;
  my $atom_char   = qq/[^($space)<>\@,;:\".$esc$OpenBR$CloseBR$ctrl$NonASCII]/;
  my $atom        = qq< $atom_char+ (?!$atom_char) >;
  my $quoted_str  = qq< \" $qtext * (?: $quoted_pair $qtext * )* \" >;
  my $word        = qq< (?: $atom | $quoted_str ) >;
  my $domain_ref  = $atom;
  my $domain_lit  = qq< $OpenBR (?: $dtext | $quoted_pair )* $CloseBR >;
  my $sub_domain  = qq< (?: $domain_ref | $domain_lit ) $X >;
  my $domain;

  @email = ();
  for $domain (
    qq< $sub_domain (?: $Period $X $sub_domain )* $Period [A-Za-z][A-Za-z][A-Za-z]?[A-Za-z]? >,
    qq< $sub_domain (?: $Period $X $sub_domain )* >
  ) {
    my $route       = qq< \@ $X $domain (?: , $X \@ $X $domain )* : $X >;
    my $local_part  = qq< $word $X (?: $Period $X $word $X )* >;
    my $addr_spec   = qq< $local_part \@ $X $domain >;
    my $route_addr  = qq[ < $X (?: $route )? $addr_spec > ];
    my $phrase_ctrl = '\000-\010\012-\037';
    my $phrase_char = qq/[^()<>\@,;:\".$esc$OpenBR$CloseBR$NonASCII$phrase_ctrl]/;
    my $phrase      = qq< $word $phrase_char * (?: (?: $comment | $quoted_str ) $phrase_char * )* >;
    my $email       = qq< $X (?: $addr_spec | $phrase  $route_addr ) >;

    if ($v56) {
      eval q<
        local $SIG{__DIE__};
        $email = qr/$email/x;
      >;
    }
    else {
      $email =~ s/\s+//g;
    }

    push @email => $email;
  }
}

### BEGIN # (2) ################################################################
#
# define regexes for URLs
#
BEGIN {
  # credits to an unknown(?) programmer ;)
  # modified by n.d.p.

  my $lowalpha       =  '[a-z]';
  my $hialpha        =  '[A-Z]';
  my $alpha          =  '[a-zA-Z]';
  my $digit          =  '\d';
  my $safe           =  '[$_.+-]';
  my $extra          =  '[!*\'(),]';
  my $national       =  '[{}|\\\\^~\[\]`]';
  my $punctuation    =  '[<>#%"]';
  my $reserved       =  '[;/?:@&=]';
  my $hex            =  '[\dA-Fa-f]';
  my $escape         =  "(?:%$hex$hex)";
  my $unreserved     =  '[{}|\\\\^~\[\]`a-zA-Z\d$_.+-]';    #"(?:$alpha|$digit|$safe|$extra)";
  my $uchar          =  "(?:$unreserved|$escape)";
  my $xchar          =  "(?:$unreserved|$escape|$reserved)";
  my $digits         =  '(?:\d+)';
  my $alphadigit     =  '[a-zA-Z\d]';                       #"(?:$alpha|$digit)";

  # URL schemeparts for ip based protocols:
  my $urlpath        =  "(?:$xchar*)";
  my $user           =  "(?:(?:$uchar|[;?&=])*)";
  my $password       =  "(?:(?:$uchar|[;?&=])*)";
  my $port           =  '(?:[0-5]?\d\d?\d?\d?|6[0-4]\d\d\d|65[0-4]\d\d|655[0-2]\d|6553[0-5])';
  my $ip4part        =  '(?:[01]?\d\d?|2[0-4]\d|25[0-5])';
  my $hostnumber     =  '(?:(?!0+\.0+\.0+\.0+)(?!255\.255\.255\.255)'."$ip4part\\.$ip4part\\.$ip4part\\.$ip4part)";
  my $toplabel       =  "(?:(?:$alpha(?:$alphadigit|-)*$alphadigit)|$alpha)";
  my $domainlabel    =  "(?:(?:$alphadigit(?:$alphadigit|-)*$alphadigit)|$alphadigit)";
  my $hostname       =  "(?:(?:$domainlabel\\.)*$toplabel)";
  my $host           =  "(?:(?:$hostname)|(?:$hostnumber))";
  my $hostport       =  "(?:(?:$host)(?::$port)?)";
  my $login          =  "(?:(?:$user(?::$password)?\@)?$hostport)";
  my $ip_schemepart  =  "(?://$login(?:/$urlpath)?)";

  my $schemepart     =  "(?:$xchar*|$ip_schemepart)";
  my $scheme         =  "(?:(?:$lowalpha|$digit|[+.-])+)";

  # The predefined schemes:

  # FTP (see also RFC959)
  my $fsegment       =  "(?:(?:$uchar|[?:\@&=])*)";
  my $ftptype        =  "(?:[AIDaid])";
  my $fpath          =  "(?:$fsegment(?:/$fsegment)*)";
  my $ftpurl         =  "(?:ftp://$login(?:/$fpath(?:;type=$ftptype)?)?)";

  # FILE
  my $fileurl        =  "(?:file://(?:(?:$host)|localhost)?/$fpath)";

  # HTTP
  my $httpuchar      =  "(?:(?:$alpha|$digit|$safe|(?:[!*\',]))|$escape)";
  my $hsegment       =  "(?:(?:$httpuchar|[;:\@&=~])*)";
  my $search         =  "(?:(?:$httpuchar|[;:\@&=~])*)";
  my $hpath          =  "(?:$hsegment(?:/$hsegment)*)";
  my $httpurl        =  "(?:https?://$hostport(?:/$hpath(?:\\?$search)?)?(?:#$xchar*)?)";
  my $strict_httpurl =  "(?:https?://$hostport(?:/$hpath(?:\\?$search)?)?)";

  # GOPHER (see also RFC1436)
  my $gopher_plus    =  "(?:$xchar*)";
  my $selector       =  "(?:$xchar*)";
  my $gtype          =  "(?:$xchar)";
  my $gopherurl      =  "(?:gopher://$hostport(?:/$gtype(?:$selector(?:%09$search(?:%09$gopher_plus)?)?)?)?)";

  # NEWS (see also RFC1036)
  my $article        =  "(?:(?:$uchar|[;/?:&=])+\@$host)";
  my $group          =  "(?:$alpha(?:$alpha|$digit|[.+_-])*)";
  my $grouppart      =  "(?:$article|$group|\\*)";
  my $newsurl        =  "(?:news:$grouppart)";

  # NNTP (see also RFC977)
  my $nntpurl        =  "(?:nntp://$hostport/$group(?:/$digits)?)";

  # TELNET
  my $telneturl      =  "(?:telnet://$login(?:/)?)";

  # WAIS (see also RFC1625)
  my $wpath          =  "(?:$uchar*)";
  my $wtype          =  "(?:$uchar*)";
  my $database       =  "(?:$uchar*)";
  my $waisdoc        =  "(?:wais://$hostport/$database/$wtype/$wpath)";
  my $waisindex      =  "(?:wais://$hostport/$database\\?$search)";
  my $waisdatabase   =  "(?:wais://$hostport/$database)";
  my $waisurl        =  "(?:$waisdatabase|$waisindex|$waisdoc)";

  # PROSPERO
  my $fieldvalue     =  "(?:(?:$uchar|[?:\@&]))";
  my $fieldname      =  "(?:(?:$uchar|[?:\@&]))";
  my $fieldspec      =  "(?:;$fieldname=$fieldvalue)";
  my $psegment       =  "(?:(?:$uchar|[?:\@&=]))";
  my $ppath          =  "(?:$psegment(?:/$psegment)*)";
  my $prosperourl    =  "(?:prospero://$hostport/$ppath(?:$fieldspec)*)";

  if ($v56) {
    eval q[%url = (
      http          => qr/^$httpurl$/,
      strict_http   => qr/^$strict_httpurl$/,
      ftp           => qr/^$ftpurl$/,
      news          => qr/^$newsurl$/,
      nntp          => qr/^$nntpurl$/,
      telnet        => qr/^$telneturl$/,
      gopher        => qr/^$gopherurl$/,
      wais          => qr/^$waisurl$/,
      mailto        => 0,
      strict_mailto => 0,
      file          => qr/^$fileurl$/,
      prospero      => qr/^$prosperourl$/
    );];
  }
  else {
    %url = (
      http          => "^$httpurl\$",
      strict_http   => "^$strict_httpurl\$",
      ftp           => "^$ftpurl\$",
      news          => "^$newsurl\$",
      nntp          => "^$nntpurl\$",
      telnet        => "^$telneturl\$",
      gopher        => "^$gopherurl\$",
      wais          => "^$waisurl\$",
      mailto        => 0,
      strict_mailto => 0,
      file          => "^$fileurl\$",
      prospero      => "^$prosperourl\$"
    );
  }
}

# keep 'require' happy
1;

#
#
### end of CheckRFC ############################################################
