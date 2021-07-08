#!/usr/bin/perl

# (C) Jason Liu

# Tests for nginx fastest module.

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

plan(skip_all => 'win32') if $^O eq 'MSWin32';

my @port_array = (8081, 8082, 8083);

my $test_num = @port_array + 1;
my $t = Test::Nginx->new()->has(qw/upstream_fastest/)->plan($test_num);

$t->write_file_expand('nginx.conf', <<'EOF');

%%TEST_GLOBALS%%

daemon         off;

events {
}

http {
    %%TEST_GLOBALS_HTTP%%

    upstream pool {
        fastest;
        server 127.0.0.1:8081;
        server 127.0.0.1:8082;
        server 127.0.0.1:8083;
    }

    server {
        listen       127.0.0.1:8888;
        server_name  localhost;
        location / {
            proxy_pass http://pool;
        }
    }
}

EOF

my $fastest = int(rand(@port_array));

my $def_sleep_time = 1;

for(my $index = 0; $index < @port_array; $index++) {
    my $sleep_time;

    if ($index == $fastest) {
        $sleep_time = 0;
    } else {
        $sleep_time = $def_sleep_time;
    }

    $t->run_daemon(\&http_daemon, $port_array[$index], $sleep_time);
}
$t->run();

##########################################################################################################
my $each_port;
foreach $each_port (@port_array) {
    like(http_get('/test.html'), qr/TEST-OK-IF-YOU-SEE-THIS-FROM-$each_port/m, "request $each_port");
}
like(http_get('/test.html'), qr/TEST-OK-IF-YOU-SEE-THIS-FROM-$port_array[$fastest]/m, "request fastest");

##########################################################################################################

sub http_daemon {
    my ($port, $sleep_time) = @_;
	my $server = IO::Socket::INET->new(
		Proto => 'tcp',
		LocalHost => "127.0.0.1:$port",
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

        sleep $sleep_time;

		$uri = $1 if $headers =~ /^\S+\s+([^ ]+)\s+HTTP/i;

		if ($uri eq '/test.html') {

			print $client <<"EOF";
HTTP/1.1 200 OK
Connection: close

TEST-OK-IF-YOU-SEE-THIS-FROM-$port

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
