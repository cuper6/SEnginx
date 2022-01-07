SEnginx
=======
SEnginx (Security Enhanced Nginx) is a variant of nginx - the famous HTTP and reverse proxy server. It introduces web security features into original nginx and makes improvements on application delivery functionality. SEnginx inherit all original features from nginx and is compatible with nginx's configuration, besides SEnginx has also bundled some handy third party modules.

SEnginx open source project was originally launched by Neusoft Corporation. Currently this fork maintained by upgrading sources to the latest stable versions of nginx and 3rd party modules, adding new functionality and fixing some bugs.

Features
--------
* All features of original nginx 1.18.0, you can find more at: http://nginx.org/en/docs/
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
        * ...
    * Secure Session Mechanism
    * NetEye Security Layer
* Managment
    * Syslog Support

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
More information, check website: http://senginx.ru
