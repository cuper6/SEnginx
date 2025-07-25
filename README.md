![senginx_logo](https://user-images.githubusercontent.com/19606735/151982748-bdac9e49-6081-4901-8c6b-8d0fe4745360.png)

SEnginx
=======
SEnginx (Security Enhanced Nginx) is a variant of nginx - the famous HTTP and reverse proxy server. It introduces web security features into original nginx and makes improvements on application delivery functionality. SEnginx inherit all original features from nginx and is compatible with nginx's configuration, besides SEnginx has also bundled some handy third party modules.

SEnginx open source project was originally launched by Neusoft Corporation. Since 2021 SEnginx is developed at new SEnginx GitHub repository because Neusoft project was abandoned at 2017.
 
SEnginx is based on nginx 1.22.1.

SEnginx is free.

SEnginx wiki site: http://senginx.ru

SEnginx Pro
=======
SEnginx Pro (http://senginx.ru/en/senginxpro.html) is an evolution of SEnginx with some advanced security functions and options for andi-ddos protection. SEnginx Pro also gets nginx major updates in the first place.

SEnginx Pro is based on nginx 1.28.0.

SEnginx Pro is not free. 

Features
--------
* All features of original nginx, you can find more at: http://nginx.org/en/docs/
* Web Security
    * IP Access Behavior Module
    * Conditional limit_req module
    * HTTP Robot Mitigation:
        * HTTP DDoS Mitigation (Low Orbit Ion Cannon ...)
        * Vulnerability Scanning (AppScan, Acunetix Web Vulnerability Scanner, Metasploit Pro, Nessus ...)
        * Spiders, Crawlers and other robotic evil
    * Dynamic IP Blacklist
    * User-Agent Whitelist with DNS Reverse Resolve
    * Cookie Poisoning
    * Web Defacement
    * Protection of Web Vulnerabilities (Integrated Naxsi and ModSecurity):
        * SQL Injection
        * Cross Site Scripting
        * Directory Traversal
        * Remote File Inclusion
        * Evading Tricks
    * Secure Session Mechanism
    * NetEye Security Layer
* Application Delivery
    * TCP Proxy and Load Balancing
    * Enhanced "if" Direcitve in Rewrite Module
    * Dynamic DNS Resolve in Upstream
    * Proxy HTTPS Client Certificate
    * Load Balancing Algorithm
        * Fastest Load Balancing Algorithm
        * Fair Load Balancing Alogorithm
    * Session Persistence
    * Caching Based on MIME Type
    * Server Health Monitor
    * Statistics for Traffic and Attacks
* 3rd-party modules for nginx
    * ngx_headers_more
    * ngx_http_lua_module
    * ngx_memc
    * ngx_cache_purge
    * ngx_devel_kit
    * ngx_http_substitutions_filter_module
    * srcache-nginx-module


Installation
------------
Almost the same as original nginx installation, but use se-configure.sh to generate Makefile instead.

The se-configure.sh script can also accept original configure.sh's parameters.

Example:

    ./se-configure.sh --with-http_v2_module --prefix=/path/to/some/where
    make
    make install


Auto Test
---------
We have prepared a set of test cases in the test directory, use the auto-test.sh script to run all the test cases.

Example:

    cd test/
    ./auto-test.sh -s ./ -n /path/to/senginx/binary/file -r /path/to/naxsi_core.rules


Other
-----
Neusoft SEnginx repository: https://github.com/NeusoftSecurity/SEnginx
