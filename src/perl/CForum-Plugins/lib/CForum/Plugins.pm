package CForum::Plugins;

use 5.006;
use strict;
use warnings;
use Carp;

use constant FLT_OK => 0;
use constant FLT_EXIT => -1;
use constant FLT_DECLINE => -2;

require Exporter;
use AutoLoader;

our @ISA = qw(Exporter);
our @EXPORT_OK = ();

our $VERSION = '0.01';


our %PLUGIN_TYPES = (
  INIT_HANDLER => 1,
  VIEW_HANDLER => 2,
  VIEW_INIT_HANDLER => 3,
  VIEW_LIST_HANDLER => 4,
  POSTING_HANDLER => 5,
  CONNECT_INIT_HANDLER => 6,
  AUTH_HANDLER => 7,
  ARCHIVE_HANDLER => 8
);

sub plugin_type {
  my $name = shift;

  return $PLUGIN_TYPES{$name} if exists $PLUGIN_TYPES{$name};
  return;
}

sub AUTOLOAD {
    # This AUTOLOAD is used to 'autoload' constants from the constant()
    # XS function.

    my $constname;
    our $AUTOLOAD;
    ($constname = $AUTOLOAD) =~ s/.*:://;
    croak "&CForum::Plugins::constant not defined" if $constname eq 'constant';
    my ($error, $val) = constant($constname);
    if ($error) { croak $error; }
    {
	no strict 'refs';
	# Fixed between 5.005_53 and 5.005_61
#XXX	if ($] >= 5.00561) {
#XXX	    *$AUTOLOAD = sub () { $val };
#XXX	}
#XXX	else {
	    *$AUTOLOAD = sub { $val };
#XXX	}
    }
    goto &$AUTOLOAD;
}

require XSLoader;
XSLoader::load('CForum::Plugins', $VERSION);

# Preloaded methods go here.

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__
# Below is stub documentation for your module. You'd better edit it!

=head1 NAME

CForum::Plugins - Perl extension for blah blah blah

=head1 SYNOPSIS

  use CForum::Plugins;
  blah blah blah

=head1 DESCRIPTION

Stub documentation for CForum::Plugins, created by h2xs. It looks like the
author of the extension was negligent enough to leave the stub
unedited.

Blah blah blah.

=head2 EXPORT

None by default.



=head1 SEE ALSO

Mention other useful documentation such as the documentation of
related modules or operating system documentation (such as man pages
in UNIX), or any relevant external documentation such as RFCs or
standards.

If you have a mailing list set up for your module, mention it here.

If you have a web site set up for your module, mention it here.

=head1 AUTHOR

A. U. Thor, E<lt>ckruse@defunced.deE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2004 by A. U. Thor

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.3 or,
at your option, any later version of Perl 5 you may have available.


=cut
