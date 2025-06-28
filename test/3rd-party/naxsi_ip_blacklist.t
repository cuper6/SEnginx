#!/usr/bin/perl

# Tests for naxsi ip blacklist module.

###############################################################################

use warnings;
use strict;

use Test::More;

BEGIN { use FindBin; chdir($FindBin::Bin); }

use lib '../lib';
use Test::Nginx;

###############################################################################

select STDERR; $| = 1;
select STDOUT; $| = 1;

my $naxsi_rule;
if ($ENV{TEST_NAXSI_RULE}) {
    $naxsi_rule = $ENV{TEST_NAXSI_RULE};
} else {
    Test::More::plan(skip_all => "naxsi core rule is not specified");
}

my $t = Test::Nginx->new()->has(qw/http ip_blacklist naxsi/)->plan(1);
my $test_dir = $t->testdir();

$t->write_file_expand('nginx.conf', <<"EOF");

%%TEST_GLOBALS%%

daemon         off;

events {
}

http {
    %%TEST_GLOBALS_HTTP%%

    ip_blacklist on;
    ip_blacklist_size 1000;
    ip_blacklist_timeout 60;
    ip_blacklist_log on;
    ip_blacklist_mode sys;
    ip_blacklist_ttl 4;
    ip_blacklist_syscmd "/bin/touch $test_dir/naxsi%V";

    include  $naxsi_rule;

    server {
        listen       127.0.0.1:8888;
        server_name  localhost;

        location /a.html {
             #LearningMode;
             SecRulesEnabled;
             #SecRulesDisabled;

             DeniedUrl '/rd';
             ## check rules
             CheckRule '\$XSS >= 4' BLOCK;
             CheckRule '\$TRAVERSAL >= 4' BLOCK;
             CheckRule '\$EVADE >= 8' BLOCK;
             CheckRule '\$UPLOAD >= 8' BLOCK;
             CheckRule '\$RFI >= 8' BLOCK;
             CheckRule '\$SQL >= 8' BLOCK;

             naxsi_blacklist 3; 
        }

        location /rd {
             return 403;
        }

    }
}

EOF

$t->write_file('a.html', 'some content');
$t->run();

###############################################################################
    http_get('/a.html?v=1<<<<<<>>>>>>');
    sleep 1;
    http_get('/a.html?v=2<<<<<<>>>>>>');
    sleep 2;
    http_get('/a.html?v=3<<<<<<>>>>>>');
    sleep 3;
    http_get('/a.html?v=4<<<<<<>>>>>>');

ok(-e "$test_dir/naxsi127.0.0.1", 'test file created');
###############################################################################
