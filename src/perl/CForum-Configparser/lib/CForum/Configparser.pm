package CForum::Configparser;

use 5.006;
use strict;
use warnings;
use Carp;

require Exporter;
use AutoLoader;

our @ISA = qw(Exporter);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use CForum::Configparser ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our @EXPORT_OK = qw($fo_default_conf $fo_view_conf $fo_post_conf $fo_server_conf);

our $VERSION = '0.01';


our $fo_default_conf;
our $fo_view_conf;
our $fo_post_conf;
our $fo_server_conf;

sub AUTOLOAD {
    # This AUTOLOAD is used to 'autoload' constants from the constant()
    # XS function.

    my $constname;
    our $AUTOLOAD;
    ($constname = $AUTOLOAD) =~ s/.*:://;
    croak "&CForum::Configparser::constant not defined" if $constname eq 'constant';
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
XSLoader::load('CForum::Configparser', $VERSION);

# Preloaded methods go here.

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__
# Below is stub documentation for your module. You'd better edit it!

=head1 NAME

CForum::Configparser - Perl extension for blah blah blah

=head1 SYNOPSIS

  use CForum::Configparser qw($fo_default_conf $fo_view_conf $fo_server_conf $fo_post_conf);
  my $parser = new CForum::Configparser;
  $parser->read(["fo_default","fo_view","fo_server","fo_post"]) or die $@;

  my $ent = $fo_default_conf->get_entry("TemplateMode");
  print $ent->get_value($_)."\n" for 0..2;

  while($ent=$ent->next) {
    print $ent->get_name,"\n";
  }

=head1 DESCRIPTION

Perl interface to the Classic Forum configuration parser. Valid config files are:
fo_default, fo_post, fo_view and fo_server

=head2 EXPORT

None by default. You can import $fo_default_conf, $fo_view_conf, $fo_post_conf and
$fo_server_conf.

=head1 SEE ALSO

perl(1), Documentation of the Classic Forum config parser

=head1 AUTHOR

Christian Kruse, E<lt>ckruse@wwwtech.deE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2004 by Christian Kruse

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.2 or,
at your option, any later version of Perl 5 you may have available.


=cut
