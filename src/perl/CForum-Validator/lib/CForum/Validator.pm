package CForum::Validator;

use 5.006;
use strict;
use warnings;

use constant VALIDATE_STRICT => 1;

require Exporter;

our @ISA = qw(Exporter);

our @EXPORT = qw(
  is_valid_link
  is_valid_ftp_link
  is_valid_gopher_link
  is_valid_news_link
  is_valid_nntp_link
  is_valid_telnet_link
  is_valid_prospero_link
  is_valid_wais_link
  is_valid_mailaddress
  is_valid_mailto_link
  is_valid_http_link
  is_valid_hostname
);

our $VERSION = '0.01';

require XSLoader;
XSLoader::load('CForum::Validator', $VERSION);

# Preloaded methods go here.

1;
__END__
# Below is stub documentation for your module. You'd better edit it!

=head1 NAME

CForum::Validator - Perl extension for validating URIs according to
RFC 1738

=head1 SYNOPSIS

  use CForum::Validator;
  print "valid" if is_valid_link('http://wwwtech.de/');
  print "valid" if is_valid_http_link('http://wwwtech.de/',CForum::Validator::VALIDATE_STRICT);

=head1 DESCRIPTION

Perl interface to the validation functions of the Classic Forum.

=head2 EXPORT

is_valid_link()
is_valid_ftp_link()
is_valid_gopher_link()
is_valid_news_link()
is_valid_nntp_link()
is_valid_telnet_link()
is_valid_prospero_link()
is_valid_wais_link()
is_valid_mailaddress()
is_valid_mailto_link()
is_valid_http_link()
is_valid_hostname()


=head1 SEE ALSO

See also the documentation at <http://wwwtech.de/cforum/doxygen/>

=head1 AUTHOR

Christian Kruse, E<lt>ckruse@wwwtech.deE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2004 by Christian Kruse

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.2 or,
at your option, any later version of Perl 5 you may have available.


=cut
