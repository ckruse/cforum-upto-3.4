package CForum::Template;

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

# This allows declaration  use CForum::Template ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(

) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(

);

our $VERSION = '0.01';

sub AUTOLOAD {
    # This AUTOLOAD is used to 'autoload' constants from the constant()
    # XS function.

    my $constname;
    our $AUTOLOAD;
    ($constname = $AUTOLOAD) =~ s/.*:://;
    croak "&CForum::Template::constant not defined" if $constname eq 'constant';
    my ($error, $val) = constant($constname);
    if ($error) { croak $error; }
    {
  no strict 'refs';
  # Fixed between 5.005_53 and 5.005_61
#XXX  if ($] >= 5.00561) {
#XXX      *$AUTOLOAD = sub () { $val };
#XXX  }
#XXX  else {
      *$AUTOLOAD = sub { $val };
#XXX  }
    }
    goto &$AUTOLOAD;
}

sub DESTROY {
  my $self = shift;
  $self->cleanup();
}

sub setVar {
  my $self   = shift;
  my $name   = shift;
  my $value  = shift||'';
  my $escape = shift || 0;

  $self->set_var($name,$value,length($value),$escape);
}

sub appendVar {
  my $self = shift;
  my $name = shift;
  my $value = shift;

  $self->append_var($name,$value,length($value));
}

sub setVars {
  my $self = shift;
  my $ref  = shift;

  foreach my $name (keys %{$ref}) {
    $self->set_var($name,$ref->{$name}->[0],length $ref->{$name}->[1]);
  }
}

require XSLoader;
XSLoader::load('CForum::Template', $VERSION);

# Preloaded methods go here.

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__
# Below is stub documentation for your module. You'd better edit it!

=head1 NAME

CForum::Template - Perl extension for the Classic Forum template language

=head1 SYNOPSIS

  use CForum::Template;
  my $tpl = new CForum::Template('/full/path/template.so');

=head1 DESCRIPTION

  This template engine comes with a script 'template_gen.pl' which
  compiles a template to the C language. The resulting .so-file will
  be bound to the executable at runtime.

=head2 EXPORT

  None.

=head1 SEE ALSO

perl(1), htdocs/docs/

=head1 AUTHOR

Christian Kruse, E<lt>ckruse@wwwtech.deE<gt>

=head1 COPYRIGHT AND LICENSE

=cut
