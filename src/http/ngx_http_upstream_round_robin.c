
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#if (NGX_HTTP_PERSISTENCE)
#include <ngx_http_upstream_persistence.h>
#endif

#define ngx_http_upstream_tries(p) ((p)->tries                                \
                                    + ((p)->next ? (p)->next->tries : 0))


static ngx_http_upstream_rr_peer_t *ngx_http_upstream_get_peer(
    ngx_http_upstream_rr_peer_data_t *rrp);

#if (NGX_HTTP_SSL)

static ngx_int_t ngx_http_upstream_empty_set_session(ngx_peer_connection_t *pc,
    void *data);
static void ngx_http_upstream_empty_save_session(ngx_peer_connection_t *pc,
    void *data);

#endif

#if (NGX_DYNAMIC_RESOLVE)
static ngx_int_t
ngx_http_upstream_reinit_round_robin(ngx_http_request_t *r, ngx_pool_t *pool,
    ngx_http_upstream_srv_conf_t *us, void *data)
{
    if (us->peer.init(r, us) != NGX_OK) {
        return NGX_ERROR;
    }

    return NGX_OK;
}
#endif

ngx_int_t
ngx_http_upstream_init_round_robin(ngx_conf_t *cf,
    ngx_http_upstream_srv_conf_t *us)
{
    ngx_url_t                      u;
    ngx_uint_t                     i, j, n, w, t;
    ngx_http_upstream_server_t    *server;
    ngx_http_upstream_rr_peer_t   *peer, **peerp;
    ngx_http_upstream_rr_peers_t  *peers, *backup;

#if (NGX_DYNAMIC_RESOLVE)
    ngx_uint_t                     color;
    ngx_uint_t                     dyn_resolve = 0;
#if (NGX_HAVE_INET6)
    u_char                         addr6[NGX_INET6_ADDRSTRLEN];
#endif
#endif

    us->peer.init = ngx_http_upstream_init_round_robin_peer;
#if (NGX_DYNAMIC_RESOLVE)
    us->peer.reinit_upstream = ngx_http_upstream_reinit_round_robin;
#endif

    if (us->servers) {
        server = us->servers->elts;

        n = 0;
        w = 0;
        t = 0;

        for (i = 0; i < us->servers->nelts; i++) {
            if (server[i].backup) {
                continue;
            }

            n += server[i].naddrs;
            w += server[i].naddrs * server[i].weight;

            if (!server[i].down) {
                t += server[i].naddrs;
            }
        }

        if (n == 0) {
            ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                          "no servers in upstream \"%V\" in %s:%ui",
                          &us->host, us->file_name, us->line);
            return NGX_ERROR;
        }

        peers = ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_rr_peers_t));
        if (peers == NULL) {
            return NGX_ERROR;
        }

        peer = ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_rr_peer_t) * n);
        if (peer == NULL) {
            return NGX_ERROR;
        }

        peers->single = (n == 1);
        peers->number = n;
        peers->weighted = (w != n);
        peers->total_weight = w;
        peers->tries = t;
        peers->name = &us->host;

        n = 0;
#if (NGX_DYNAMIC_RESOLVE)
        color = 1;
#endif
        peerp = &peers->peer;

        for (i = 0; i < us->servers->nelts; i++) {
            if (server[i].backup) {
                continue;
            }

#if (NGX_DYNAMIC_RESOLVE)
#if (NGX_HAVE_INET6)
            dyn_resolve = (ngx_inet6_addr(server[i].host.data,
                        server[i].host.len, addr6) == NGX_ERROR) ? 1 : 0;
            if (dyn_resolve)
                /* host is not an ipv6 address, check ipv4 */

#endif
            {
                dyn_resolve = (ngx_inet_addr(server[i].host.data,
                            server[i].host.len) == INADDR_NONE) ? 1 : 0;
            }
#endif

            for (j = 0; j < server[i].naddrs; j++) {
                peer[n].sockaddr = server[i].addrs[j].sockaddr;
                peer[n].socklen = server[i].addrs[j].socklen;
                peer[n].name = server[i].addrs[j].name;
                peer[n].weight = server[i].weight;
                peer[n].effective_weight = server[i].weight;
                peer[n].current_weight = 0;
                peer[n].max_conns = server[i].max_conns;
                peer[n].max_fails = server[i].max_fails;
                peer[n].fail_timeout = server[i].fail_timeout;
                peer[n].down = server[i].down;
                peer[n].server = server[i].name;

#if (NGX_DYNAMIC_RESOLVE)
                peer[n].host = server[i].host;
                peer[n].color = color;
                peer[n].dyn_resolve = dyn_resolve;
#endif

                *peerp = &peer[n];
                peerp = &peer[n].next;

#if (NGX_HTTP_UPSTREAM_CHECK)
                if (!server[i].down) {
                    peers->peer[n].check_index =
                        ngx_http_upstream_check_add_peer(cf, us,
                                                         &server[i].addrs[j]);
                } else {
                    peers->peer[n].check_index = (ngx_uint_t) NGX_ERROR;
                }
#endif

                n++;
            }

#if (NGX_DYNAMIC_RESOLVE)
            color++;
#endif

        }

        us->peer.data = peers;

        /* backup servers */

        n = 0;
        w = 0;
        t = 0;

        for (i = 0; i < us->servers->nelts; i++) {
            if (!server[i].backup) {
                continue;
            }

            n += server[i].naddrs;
            w += server[i].naddrs * server[i].weight;

            if (!server[i].down) {
                t += server[i].naddrs;
            }
        }

        if (n == 0) {
            return NGX_OK;
        }

        backup = ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_rr_peers_t));
        if (backup == NULL) {
            return NGX_ERROR;
        }

        peer = ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_rr_peer_t) * n);
        if (peer == NULL) {
            return NGX_ERROR;
        }

        peers->single = 0;
        backup->single = 0;
        backup->number = n;
        backup->weighted = (w != n);
        backup->total_weight = w;
        backup->tries = t;
        backup->name = &us->host;

        n = 0;
        peerp = &backup->peer;

        for (i = 0; i < us->servers->nelts; i++) {
            if (!server[i].backup) {
                continue;
            }

#if (NGX_DYNAMIC_RESOLVE)
#if (NGX_HAVE_INET6)
                dyn_resolve = (ngx_inet6_addr(server[i].host.data,
                            server[i].host.len, addr6) == NGX_ERROR) ? 1 : 0;
                if (dyn_resolve)
                    /* host is not an ipv6 address, check ipv4 */

#endif
                {
                    dyn_resolve = (ngx_inet_addr(server[i].host.data,
                                server[i].host.len) == INADDR_NONE) ? 1 : 0;
                }
#endif

            for (j = 0; j < server[i].naddrs; j++) {
                peer[n].sockaddr = server[i].addrs[j].sockaddr;
                peer[n].socklen = server[i].addrs[j].socklen;
                peer[n].name = server[i].addrs[j].name;
                peer[n].weight = server[i].weight;
                peer[n].effective_weight = server[i].weight;
                peer[n].current_weight = 0;
                peer[n].max_conns = server[i].max_conns;
                peer[n].max_fails = server[i].max_fails;
                peer[n].fail_timeout = server[i].fail_timeout;
                peer[n].down = server[i].down;
                peer[n].server = server[i].name;

#if (NGX_DYNAMIC_RESOLVE)
                peer[n].host = server[i].host;
                peer[n].color = color;
                peer[n].dyn_resolve = dyn_resolve;
#endif

                *peerp = &peer[n];
                peerp = &peer[n].next;
                n++;
#if (NGX_HTTP_UPSTREAM_CHECK)
                if (!server[i].down) {
                    backup->peer[n].check_index =
                        ngx_http_upstream_check_add_peer(cf, us,
                                                         &server[i].addrs[j]);
                } else {
                    backup->peer[n].check_index = (ngx_uint_t) NGX_ERROR;
                }
#endif
            }
#if (NGX_DYNAMIC_RESOLVE)
            color++;
#endif
        }

        peers->next = backup;

        return NGX_OK;
    }


    /* an upstream implicitly defined by proxy_pass, etc. */

    if (us->port == 0) {
        ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                      "no port in upstream \"%V\" in %s:%ui",
                      &us->host, us->file_name, us->line);
        return NGX_ERROR;
    }

    ngx_memzero(&u, sizeof(ngx_url_t));

    u.host = us->host;
    u.port = us->port;

    if (ngx_inet_resolve_host(cf->pool, &u) != NGX_OK) {
        if (u.err) {
            ngx_log_error(NGX_LOG_EMERG, cf->log, 0,
                          "%s in upstream \"%V\" in %s:%ui",
                          u.err, &us->host, us->file_name, us->line);
        }

        return NGX_ERROR;
    }

    n = u.naddrs;

    peers = ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_rr_peers_t));
    if (peers == NULL) {
        return NGX_ERROR;
    }

    peer = ngx_pcalloc(cf->pool, sizeof(ngx_http_upstream_rr_peer_t) * n);
    if (peer == NULL) {
        return NGX_ERROR;
    }

    peers->single = (n == 1);
    peers->number = n;
    peers->weighted = 0;
    peers->total_weight = n;
    peers->tries = n;
    peers->name = &us->host;

#if (NGX_DYNAMIC_RESOLVE)
#if (NGX_HAVE_INET6)
    dyn_resolve = (ngx_inet6_addr(u.host.data,
                u.host.len, addr6) == NGX_ERROR) ? 1 : 0;
    if (dyn_resolve)
        /* host is not an ipv6 address, check ipv4 */

#endif
    {
        dyn_resolve = (ngx_inet_addr(u.host.data,
                    u.host.len) == INADDR_NONE) ? 1 : 0;
    }
#endif

    peerp = &peers->peer;

    for (i = 0; i < u.naddrs; i++) {
        peer[i].sockaddr = u.addrs[i].sockaddr;
        peer[i].socklen = u.addrs[i].socklen;
        peer[i].name = u.addrs[i].name;
        peer[i].weight = 1;
        peer[i].effective_weight = 1;
        peer[i].current_weight = 0;
        peer[i].max_conns = 0;
        peer[i].max_fails = 1;
        peer[i].fail_timeout = 10;
        *peerp = &peer[i];
        peerp = &peer[i].next;
#if (NGX_DYNAMIC_RESOLVE)
        peer[i].host = u.host;
        peers->peer[i].color = 0;
        peers->peer[i].dyn_resolve = dyn_resolve;
#endif
#if (NGX_HTTP_UPSTREAM_CHECK)
        peers->peer[i].check_index = (ngx_uint_t) NGX_ERROR;
#endif
    }

    us->peer.data = peers;

    /* implicitly defined upstream has no backup servers */

    return NGX_OK;
}


ngx_int_t
ngx_http_upstream_init_round_robin_peer(ngx_http_request_t *r,
    ngx_http_upstream_srv_conf_t *us)
{
    ngx_uint_t                         n;
    ngx_http_upstream_rr_peer_data_t  *rrp;

    rrp = r->upstream->peer.data;

    if (rrp == NULL) {
        rrp = ngx_palloc(r->pool, sizeof(ngx_http_upstream_rr_peer_data_t));
        if (rrp == NULL) {
            return NGX_ERROR;
        }
#if (NGX_DYNAMIC_RESOLVE)
        rrp->dyn_peers = NULL;
#endif

        r->upstream->peer.data = rrp;
    }
#if (NGX_DYNAMIC_RESOLVE)
    if (r->upstream->conf->dyn_resolve && us->peer.dyn_data) {
        rrp->peers = rrp->dyn_peers = us->peer.dyn_data;
    } else {
#endif

    rrp->peers = us->peer.data;
#if (NGX_DYNAMIC_RESOLVE)
    }
#endif
    rrp->current = NULL;
    rrp->config = 0;

    n = rrp->peers->number;

    if (rrp->peers->next && rrp->peers->next->number > n) {
        n = rrp->peers->next->number;
    }

    if (n <= 8 * sizeof(uintptr_t)) {
        rrp->tried = &rrp->data;
        rrp->data = 0;

    } else {
        n = (n + (8 * sizeof(uintptr_t) - 1)) / (8 * sizeof(uintptr_t));

        rrp->tried = ngx_pcalloc(r->pool, n * sizeof(uintptr_t));
        if (rrp->tried == NULL) {
            return NGX_ERROR;
        }
    }

#if (NGX_HTTP_PERSISTENCE)
    rrp->request = r;
    rrp->group = &us->group;
#endif

    r->upstream->peer.get = ngx_http_upstream_get_round_robin_peer;
    r->upstream->peer.free = ngx_http_upstream_free_round_robin_peer;
    r->upstream->peer.tries = ngx_http_upstream_tries(rrp->peers);
#if (NGX_HTTP_SSL)
    r->upstream->peer.set_session =
                               ngx_http_upstream_set_round_robin_peer_session;
    r->upstream->peer.save_session =
                               ngx_http_upstream_save_round_robin_peer_session;
#endif

#if (NGX_DYNAMIC_RESOLVE)
    if (rrp->dyn_peers) {
        rrp->dyn_peers->ref++;
    }
#endif

    return NGX_OK;
}


ngx_int_t
ngx_http_upstream_create_round_robin_peer(ngx_http_request_t *r,
    ngx_http_upstream_resolved_t *ur)
{
    u_char                            *p;
    size_t                             len;
    socklen_t                          socklen;
    ngx_uint_t                         i, n;
    struct sockaddr                   *sockaddr;
    ngx_http_upstream_rr_peer_t       *peer, **peerp;
    ngx_http_upstream_rr_peers_t      *peers;
    ngx_http_upstream_rr_peer_data_t  *rrp;

    rrp = r->upstream->peer.data;

    if (rrp == NULL) {
        rrp = ngx_palloc(r->pool, sizeof(ngx_http_upstream_rr_peer_data_t));
        if (rrp == NULL) {
            return NGX_ERROR;
        }

        r->upstream->peer.data = rrp;
    }

    peers = ngx_pcalloc(r->pool, sizeof(ngx_http_upstream_rr_peers_t));
    if (peers == NULL) {
        return NGX_ERROR;
    }

    peer = ngx_pcalloc(r->pool, sizeof(ngx_http_upstream_rr_peer_t)
                                * ur->naddrs);
    if (peer == NULL) {
        return NGX_ERROR;
    }

    peers->single = (ur->naddrs == 1);
    peers->number = ur->naddrs;
    peers->tries = ur->naddrs;
    peers->name = &ur->host;

    if (ur->sockaddr) {
        peer[0].sockaddr = ur->sockaddr;
        peer[0].socklen = ur->socklen;
        peer[0].name = ur->name.data ? ur->name : ur->host;
        peer[0].weight = 1;
        peer[0].effective_weight = 1;
        peer[0].current_weight = 0;
        peer[0].max_conns = 0;
        peer[0].max_fails = 1;
        peer[0].fail_timeout = 10;
#if (NGX_HTTP_UPSTREAM_CHECK)
        peer[0].check_index = (ngx_uint_t) NGX_ERROR;
#endif
        peers->peer = peer;

    } else {
        peerp = &peers->peer;

        for (i = 0; i < ur->naddrs; i++) {

            socklen = ur->addrs[i].socklen;

            sockaddr = ngx_palloc(r->pool, socklen);
            if (sockaddr == NULL) {
                return NGX_ERROR;
            }

            ngx_memcpy(sockaddr, ur->addrs[i].sockaddr, socklen);
            ngx_inet_set_port(sockaddr, ur->port);

            p = ngx_pnalloc(r->pool, NGX_SOCKADDR_STRLEN);
            if (p == NULL) {
                return NGX_ERROR;
            }

            len = ngx_sock_ntop(sockaddr, socklen, p, NGX_SOCKADDR_STRLEN, 1);

            peer[i].sockaddr = sockaddr;
            peer[i].socklen = socklen;
            peer[i].name.len = len;
            peer[i].name.data = p;
            peer[i].weight = 1;
            peer[i].effective_weight = 1;
            peer[i].current_weight = 0;
            peer[i].max_conns = 0;
            peer[i].max_fails = 1;
            peer[i].fail_timeout = 10;
#if (NGX_HTTP_UPSTREAM_CHECK)
            peer[i].check_index = (ngx_uint_t) NGX_ERROR;
#endif
            *peerp = &peer[i];
            peerp = &peer[i].next;
        }
    }

    rrp->peers = peers;
    rrp->current = NULL;
    rrp->config = 0;

    if (rrp->peers->number <= 8 * sizeof(uintptr_t)) {
        rrp->tried = &rrp->data;
        rrp->data = 0;

    } else {
        n = (rrp->peers->number + (8 * sizeof(uintptr_t) - 1))
                / (8 * sizeof(uintptr_t));

        rrp->tried = ngx_pcalloc(r->pool, n * sizeof(uintptr_t));
        if (rrp->tried == NULL) {
            return NGX_ERROR;
        }
    }

    r->upstream->peer.get = ngx_http_upstream_get_round_robin_peer;
    r->upstream->peer.free = ngx_http_upstream_free_round_robin_peer;
    r->upstream->peer.tries = ngx_http_upstream_tries(rrp->peers);
#if (NGX_HTTP_SSL)
    r->upstream->peer.set_session = ngx_http_upstream_empty_set_session;
    r->upstream->peer.save_session = ngx_http_upstream_empty_save_session;
#endif

    return NGX_OK;
}


ngx_int_t
ngx_http_upstream_get_round_robin_peer(ngx_peer_connection_t *pc, void *data)
{
    ngx_http_upstream_rr_peer_data_t  *rrp = data;

    ngx_int_t                      rc;
    ngx_uint_t                     i, n;
    ngx_http_upstream_rr_peer_t   *peer;
    ngx_http_upstream_rr_peers_t  *peers;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                   "get rr peer, try: %ui", pc->tries);

    pc->cached = 0;
    pc->connection = NULL;

    peers = rrp->peers;
    ngx_http_upstream_rr_peers_wlock(peers);

    if (peers->single) {
        peer = peers->peer;

        if (peer->down) {
            goto failed;
        }

        if (peer->max_conns && peer->conns >= peer->max_conns) {
            goto failed;
        }

#if (NGX_HTTP_UPSTREAM_CHECK)
        if (ngx_http_upstream_check_peer_down(peer->check_index)) {
            goto failed;
        }
#endif

        rrp->current = peer;

    } else {

        /* there are several peers */

        peer = ngx_http_upstream_get_peer(rrp);

        if (peer == NULL) {
            goto failed;
        }

        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                       "get rr peer, current: %p %i",
                       peer, peer->current_weight);
    }

    pc->sockaddr = peer->sockaddr;
    pc->socklen = peer->socklen;
    pc->name = &peer->name;

#if (NGX_DYNAMIC_RESOLVE)
    pc->host = &peer->host;
    pc->dyn_resolve = peer->dyn_resolve;

#if (NGX_DEBUG)
    struct sockaddr_in *in;
    in = (struct sockaddr_in *)peer->sockaddr;

    ngx_log_debug4(NGX_LOG_DEBUG_HTTP, pc->log, 0,
            "peer: %xd, %d, %V, %V",
            in->sin_addr.s_addr,
            peer->socklen,
            &peer->name,
            &peer->host);
#endif
#endif

    peer->conns++;

    ngx_http_upstream_rr_peers_unlock(peers);

    return NGX_OK;

failed:

    if (peers->next) {

        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, pc->log, 0, "backup servers");

        rrp->peers = peers->next;

        n = (rrp->peers->number + (8 * sizeof(uintptr_t) - 1))
                / (8 * sizeof(uintptr_t));

        for (i = 0; i < n; i++) {
            rrp->tried[i] = 0;
        }

        ngx_http_upstream_rr_peers_unlock(peers);

        rc = ngx_http_upstream_get_round_robin_peer(pc, rrp);

        if (rc != NGX_BUSY) {
            return rc;
        }

        ngx_http_upstream_rr_peers_wlock(peers);
    }

    ngx_http_upstream_rr_peers_unlock(peers);

    pc->name = peers->name;

    return NGX_BUSY;
}


static ngx_http_upstream_rr_peer_t *
ngx_http_upstream_get_peer(ngx_http_upstream_rr_peer_data_t *rrp)
{
    time_t                        now;
    uintptr_t                     m;
    ngx_int_t                     total;
    ngx_uint_t                    i, n, p;
    ngx_http_upstream_rr_peer_t  *peer, *best;
#if (NGX_HTTP_PERSISTENCE)
    ngx_int_t                     persist_index;
#endif

    now = ngx_time();

    best = NULL;
    total = 0;

#if (NGX_HTTP_PERSISTENCE)
    persist_index = ngx_http_upstream_ps_get(rrp->request,
        rrp->peers->number, rrp->group);
#endif

#if (NGX_SUPPRESS_WARN)
    p = 0;
#endif

    for (peer = rrp->peers->peer, i = 0;
         peer;
         peer = peer->next, i++)
    {
#if (NGX_HTTP_PERSISTENCE)
        if(persist_index >= 0 && i != (ngx_uint_t)persist_index) {
            continue;
        }
#endif
        n = i / (8 * sizeof(uintptr_t));
        m = (uintptr_t) 1 << i % (8 * sizeof(uintptr_t));

        if (rrp->tried[n] & m) {
#if (NGX_HTTP_PERSISTENCE)
            if(persist_index >= 0) {
                persist_index = -1;
                peer = rrp->peers->peer;
                i = 0;
            }
#endif
            continue;
        }

        if (peer->down) {
#if (NGX_HTTP_PERSISTENCE)
            if(persist_index >= 0) {
                persist_index = -1;
                peer = rrp->peers->peer;
                i = 0;
            }
#endif
            continue;
        }

#if (NGX_HTTP_UPSTREAM_CHECK)
        if (ngx_http_upstream_check_peer_down(peer->check_index)) {
            continue;
        }
#endif

        if (peer->max_fails
            && peer->fails >= peer->max_fails
            && now - peer->checked <= peer->fail_timeout)
        {
#if (NGX_HTTP_PERSISTENCE)
            if(persist_index >= 0) {
                persist_index = -1;
                peer = rrp->peers->peer;
                i = 0;
            }
#endif
            continue;
        }

#if (NGX_HTTP_PERSISTENCE)
        if(persist_index >= 0) {
            best = peer;
            p = persist_index;
            break;
        }
#endif

        if (peer->max_conns && peer->conns >= peer->max_conns) {
            continue;
        }

        peer->current_weight += peer->effective_weight;
        total += peer->effective_weight;

        if (peer->effective_weight < peer->weight) {
            peer->effective_weight++;
        }

        if (best == NULL || peer->current_weight > best->current_weight) {
            best = peer;
            p = i;
        }
    }

    if (best == NULL) {
        return NULL;
    }

    rrp->current = best;

    n = p / (8 * sizeof(uintptr_t));
    m = (uintptr_t) 1 << p % (8 * sizeof(uintptr_t));

    rrp->tried[n] |= m;

    best->current_weight -= total;

    if (now - best->checked > best->fail_timeout) {
        best->checked = now;
    }

#if (NGX_HTTP_PERSISTENCE)
    ngx_http_upstream_ps_set(rrp->request, p, rrp->group);
#endif

    return best;
}


void
ngx_http_upstream_free_round_robin_peer(ngx_peer_connection_t *pc, void *data,
    ngx_uint_t state)
{
    ngx_http_upstream_rr_peer_data_t  *rrp = data;

    time_t                       now;
    ngx_http_upstream_rr_peer_t  *peer;

    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                   "free rr peer %ui %ui", pc->tries, state);

    /* TODO: NGX_PEER_KEEPALIVE */

    peer = rrp->current;

    ngx_http_upstream_rr_peers_rlock(rrp->peers);
    ngx_http_upstream_rr_peer_lock(rrp->peers, peer);

    if (rrp->peers->single) {

        peer->conns--;

        ngx_http_upstream_rr_peer_unlock(rrp->peers, peer);
        ngx_http_upstream_rr_peers_unlock(rrp->peers);

        pc->tries = 0;
        return;
    }

    if (state & NGX_PEER_FAILED) {
        now = ngx_time();

        peer->fails++;
        peer->accessed = now;
        peer->checked = now;

        if (peer->max_fails) {
            peer->effective_weight -= peer->weight / peer->max_fails;

            if (peer->fails >= peer->max_fails) {
                ngx_log_error(NGX_LOG_WARN, pc->log, 0,
                              "upstream server temporarily disabled");
            }
        }

        ngx_log_debug2(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                       "free rr peer failed: %p %i",
                       peer, peer->effective_weight);

        if (peer->effective_weight < 0) {
            peer->effective_weight = 0;
        }

    } else {

        /* mark peer live if check passed */

        if (peer->accessed < peer->checked) {
            peer->fails = 0;
        }
    }

    peer->conns--;

    ngx_http_upstream_rr_peer_unlock(rrp->peers, peer);
    ngx_http_upstream_rr_peers_unlock(rrp->peers);

    if (pc->tries) {
        pc->tries--;
    }
}


#if (NGX_HTTP_SSL)

ngx_int_t
ngx_http_upstream_set_round_robin_peer_session(ngx_peer_connection_t *pc,
    void *data)
{
    ngx_http_upstream_rr_peer_data_t  *rrp = data;

    ngx_int_t                      rc;
    ngx_ssl_session_t             *ssl_session;
    ngx_http_upstream_rr_peer_t   *peer;
#if (NGX_HTTP_UPSTREAM_ZONE)
    int                            len;
    const u_char                  *p;
    ngx_http_upstream_rr_peers_t  *peers;
    u_char                         buf[NGX_SSL_MAX_SESSION_SIZE];
#endif

    peer = rrp->current;

#if (NGX_HTTP_UPSTREAM_ZONE)
    peers = rrp->peers;

    if (peers->shpool) {
        ngx_http_upstream_rr_peers_rlock(peers);
        ngx_http_upstream_rr_peer_lock(peers, peer);

        if (peer->ssl_session == NULL) {
            ngx_http_upstream_rr_peer_unlock(peers, peer);
            ngx_http_upstream_rr_peers_unlock(peers);
            return NGX_OK;
        }

        len = peer->ssl_session_len;

        ngx_memcpy(buf, peer->ssl_session, len);

        ngx_http_upstream_rr_peer_unlock(peers, peer);
        ngx_http_upstream_rr_peers_unlock(peers);

        p = buf;
        ssl_session = d2i_SSL_SESSION(NULL, &p, len);

        rc = ngx_ssl_set_session(pc->connection, ssl_session);

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                       "set session: %p", ssl_session);

        ngx_ssl_free_session(ssl_session);

        return rc;
    }
#endif

    ssl_session = peer->ssl_session;

    rc = ngx_ssl_set_session(pc->connection, ssl_session);

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                   "set session: %p", ssl_session);

    return rc;
}


void
ngx_http_upstream_save_round_robin_peer_session(ngx_peer_connection_t *pc,
    void *data)
{
    ngx_http_upstream_rr_peer_data_t  *rrp = data;

    ngx_ssl_session_t             *old_ssl_session, *ssl_session;
    ngx_http_upstream_rr_peer_t   *peer;
#if (NGX_HTTP_UPSTREAM_ZONE)
    int                            len;
    u_char                        *p;
    ngx_http_upstream_rr_peers_t  *peers;
    u_char                         buf[NGX_SSL_MAX_SESSION_SIZE];
#endif

#if (NGX_HTTP_UPSTREAM_ZONE)
    peers = rrp->peers;

    if (peers->shpool) {

        ssl_session = ngx_ssl_get0_session(pc->connection);

        if (ssl_session == NULL) {
            return;
        }

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                       "save session: %p", ssl_session);

        len = i2d_SSL_SESSION(ssl_session, NULL);

        /* do not cache too big session */

        if (len > NGX_SSL_MAX_SESSION_SIZE) {
            return;
        }

        p = buf;
        (void) i2d_SSL_SESSION(ssl_session, &p);

        peer = rrp->current;

        ngx_http_upstream_rr_peers_rlock(peers);
        ngx_http_upstream_rr_peer_lock(peers, peer);

        if (len > peer->ssl_session_len) {
            ngx_shmtx_lock(&peers->shpool->mutex);

            if (peer->ssl_session) {
                ngx_slab_free_locked(peers->shpool, peer->ssl_session);
            }

            peer->ssl_session = ngx_slab_alloc_locked(peers->shpool, len);

            ngx_shmtx_unlock(&peers->shpool->mutex);

            if (peer->ssl_session == NULL) {
                peer->ssl_session_len = 0;

                ngx_http_upstream_rr_peer_unlock(peers, peer);
                ngx_http_upstream_rr_peers_unlock(peers);
                return;
            }

            peer->ssl_session_len = len;
        }

        ngx_memcpy(peer->ssl_session, buf, len);

        ngx_http_upstream_rr_peer_unlock(peers, peer);
        ngx_http_upstream_rr_peers_unlock(peers);

        return;
    }
#endif

    ssl_session = ngx_ssl_get_session(pc->connection);

    if (ssl_session == NULL) {
        return;
    }

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                   "save session: %p", ssl_session);

    peer = rrp->current;

    old_ssl_session = peer->ssl_session;
    peer->ssl_session = ssl_session;

    if (old_ssl_session) {

        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, pc->log, 0,
                       "old session: %p", old_ssl_session);

        /* TODO: may block */

        ngx_ssl_free_session(old_ssl_session);
    }
}


static ngx_int_t
ngx_http_upstream_empty_set_session(ngx_peer_connection_t *pc, void *data)
{
    return NGX_OK;
}


static void
ngx_http_upstream_empty_save_session(ngx_peer_connection_t *pc, void *data)
{
    return;
}

#endif
