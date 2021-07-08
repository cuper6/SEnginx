#!/usr/bin/perl

# (C) Jason Liu

# Tests for session module.

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

my $t = Test::Nginx->new()->has(qw/http proxy session/)->plan(4);

$t->write_file_expand('nginx.conf', <<'EOF');

%%TEST_GLOBALS%%

daemon         off;

events {
}

http {
    %%TEST_GLOBALS_HTTP%%

    session_max_size 1000;
    server {
        listen       127.0.0.1:8888;
        server_name  localhost;

        location / {
            session on;
            session_cookie_name auto-test;
            proxy_pass http://127.0.0.1:8081;
        }
    }
}

EOF

$t->run_daemon(\&http_daemon);
$t->run();

###############################################################################

my $r = http_get('/');
like($r, qr/auto-test/, 'http get request');
like($r, qr/TEST-OK-IF-YOU-SEE-THIS/, 'http get request');

my @content = split /\r\n/, $r;
my @cookies;
my $cookie;
my $x;
foreach $x (@content) {
    if ($x =~ /^Set-Cookie:/) {
        @cookies = split /;/, $x;
        $_ = $cookies[0];
        s/Set-Cookie/Cookie/;
        $cookie = $_;
    }
}

$r = http_get_with_header('/', $cookie);
like($r, qr/TEST-OK-IF-YOU-SEE-THIS/, 'pass session with cookie');
unlike($r, qr/auto-test/, 'pass session with cookie');

###############################################################################

sub http_daemon {
	my $server = IO::Socket::INET->new(
		Proto => 'tcp',
		LocalHost => '127.0.0.1:8081',
		Listen => 5,
		Reuse => 1
	)
		or die "Can't create listening socket: $!\n";

	while (my $client = $server->accept()) {
		$client->autoflush(1);

		my $headers = '';
		my $uri = '';

		while (<$client>) {
			$headers .= $_;
			last if (/^\x0d?\x0a?$/);
		}

		$uri = $1 if $headers =~ /^\S+\s+([^ ]+)\s+HTTP/i;

		if ($uri eq '/') {
			print $client <<'EOF';
HTTP/1.1 200 OK
Connection: close

EOF
			print $client "TEST-OK-IF-YOU-SEE-THIS"
				unless $headers =~ /^HEAD/i;
		} elsif ($uri eq '/post') {

			print $client <<"EOF";
HTTP/1.1 200 OK
Connection: close

TEST-OK-IF-YOU-SEE-THIS

EOF
		} else {

			print $client <<"EOF";
HTTP/1.1 404 Not Found
Connection: close

Oops, '$uri' not found
EOF
		}

		close $client;
	}
}

###############################################################################
