# Makefile for CForum::Configparser

use 5.006;
use ExtUtils::MakeMaker;

WriteMakefile(
    NAME              => 'CForum::Configparser',
    VERSION_FROM      => 'Configparser.pm', # finds $VERSION
    PREREQ_PM         => {}, # e.g., Module::Name => 1.1
    LIBS              => ['-L/home/ckruse/tmp/cforum/cgi-shared/lib/cforum -lcfconfigparser -lcfhashlib -lcfutils'], # e.g., '-lm'
    DEFINE            => '', # e.g., '-DHAVE_SOMETHING'
    INC               => '-I. -I../../', # e.g., '-I. -I/usr/include/other'
    OBJECT            => 'CForum-Configparser_wrap.o'
);


