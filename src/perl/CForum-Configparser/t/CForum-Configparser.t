# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl CForum-Configparser.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use strict;
use Test;
BEGIN { plan tests => 5 };
use CForum::Configparser qw($fo_default_conf);
ok(1); # If we made it this far, we're ok.
#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.

my $conf = new CForum::Configparser();
if($conf) { ok(2); }
else { fail(2); }

my $save_env = $ENV{CF_CONF_DIR};
(my $dir = `pwd`."/t/") =~ s/\n//g;

$ENV{CF_CONF_DIR} = $dir;

if($conf->read(["fo_default"])) { ok(3); }
else { fail(3); print STDERR $@; exit; }

my $ent;
if($ent = $fo_default_conf->get_entry("TemplateMode")) { ok(4); }
else { fail(4); exit; }

if($ent->get_value(0) eq 'html4') { ok(5); }
else { fail(5); exit; }



