# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl CForum-Validator.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use Test;
BEGIN { plan tests => 13 };
use CForum::Validator;

ok(1); # If we made it this far, we're ok.

#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.

ok(is_valid_link('http://wwwtech.de:8080/'),1,'check link');
ok(is_valid_http_link('http://wwwtech.de:8080/',1),1,'check http link');
ok(is_valid_mailaddress('ckruse@wwwtech.de'),1,'check mail address');
ok(is_valid_mailto_link('mailto:ckruse@wwwtech.de'),1,'check mailto link');
ok(is_valid_hostname('wwwtech.de'),1,'check hostname');
ok(is_valid_telnet_link('telnet://wwwtech.de/'),1,'check telnet link');
ok(is_valid_nntp_link('nntp://wwwtech.de/de.alt.gruppenkasper'),1,'check nntp link');
ok(is_valid_news_link('news://de.alt.gruppenkasper/'),1,'check news link');
ok(is_valid_ftp_link('ftp://wwwtech.de/'),1,'check ftp link');
ok(is_valid_prospero_link('prospero://wwwtech.de/'),1,'check prospero link');
ok(is_valid_wais_link('wais://wwwtech.de/'),1,'check wais link');
ok(is_valid_gopher_link('gopher://wwwtech.de/'),1,'check gopher link');


