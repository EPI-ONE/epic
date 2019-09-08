#pragma once

#include "bitmap.h"
#include "compress.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <new>

typedef word_t proof[PROOFSIZE];

// cuck(ar)oo graph with given limit on number of edges (and on single partition nodes)
template <typename word_t>
class graph {
public:
    // terminates adjacency lists
    static const word_t NIL = ~(word_t) 0;

    struct link { // element of adjacency list
        word_t next;
        word_t to;
    };

    word_t maxEdges;
    word_t maxNodes;
    word_t nlinks;             // aka halfedges, twice number of edges
    word_t* adjlist = nullptr; // index into links array
    link* links     = nullptr;
    bool sharedmem;
    compressor<word_t>* compressu = nullptr;
    compressor<word_t>* compressv = nullptr;
    bitmap<uint32_t> visited;
    uint32_t maxSols;
    proof* sols = nullptr;
    uint32_t nsols;

    graph(){};
    graph(word_t maxedges, word_t maxnodes, uint32_t maxsols) : visited(2 * maxnodes) {
        maxEdges  = maxedges;
        maxNodes  = maxnodes;
        maxSols   = maxsols;
        adjlist   = new word_t[2 * maxNodes]; // index into links array
        links     = new link[2 * maxEdges];
        compressu = compressv = 0;
        sharedmem             = false;
        sols                  = new proof[maxSols + 1]; // extra one for current path
        visited.clear();
    }

    graph(word_t maxedges, word_t maxnodes, uint32_t maxsols, uint32_t compressbits) : visited(2 * maxnodes) {
        maxEdges  = maxedges;
        maxNodes  = maxnodes;
        maxSols   = maxsols;
        adjlist   = new word_t[2 * maxNodes]; // index into links array
        links     = new link[2 * maxEdges];
        compressu = new compressor<word_t>(EDGEBITS, compressbits);
        compressv = new compressor<word_t>(EDGEBITS, compressbits);
        sharedmem = false;
        sols      = new proof[maxSols];
        visited.clear();
    }

    graph(word_t maxedges, word_t maxnodes, uint32_t maxsols, char* bytes) : visited(2 * maxnodes) {
        maxEdges  = maxedges;
        maxNodes  = maxnodes;
        maxSols   = maxsols;
        adjlist   = new (bytes) word_t[2 * maxNodes]; // index into links array
        links     = new (bytes += sizeof(word_t[2 * maxNodes])) link[2 * maxEdges];
        compressu = compressv = 0;
        sharedmem             = true;
        sols                  = new proof[maxSols];
        visited.clear();
    }

    graph(word_t maxedges, word_t maxnodes, uint32_t maxsols, uint32_t compressbits, char* bytes)
        : visited(2 * maxnodes) {
        maxEdges  = maxedges;
        maxNodes  = maxnodes;
        maxSols   = maxsols;
        adjlist   = new (bytes) word_t[2 * maxNodes]; // index into links array
        links     = new (bytes += sizeof(word_t[2 * maxNodes])) link[2 * maxEdges];
        compressu = new compressor<word_t>(EDGEBITS, compressbits, bytes += sizeof(link[2 * maxEdges]));
        compressv = new compressor<word_t>(EDGEBITS, compressbits, bytes + compressu->bytes());
        sharedmem = true;
        sols      = new proof[maxSols];
        visited.clear();
    }

    ~graph() {
        if (!sharedmem) {
            delete[] adjlist;
            delete[] links;
        }
        delete[] sols;
        delete compressu;
        delete compressv;
    }

    // total size of new-operated data, excludes sols and visited bitmap of maxEdges bits
    uint64_t bytes() {
        return sizeof(word_t[2 * maxNodes]) + sizeof(link[2 * maxEdges]) + (compressu ? 2 * compressu->bytes() : 0);
    }

    void reset() {
        memset(adjlist, (char) NIL, sizeof(word_t[2 * maxNodes]));
        if (compressu) {
            compressu->reset();
            compressv->reset();
        }
        resetcounts();
    }

    void resetcounts() {
        nlinks = nsols = 0;
        // visited has entries set only during cycles() call
    }

    static int nonce_cmp(const void* a, const void* b) {
        return *(word_t*) a - *(word_t*) b;
    }

    void cycles_with_link(uint32_t len, word_t u, word_t dest) {
        // printf("cycles_with_link(%d, %x, %x)\n", len, u, dest);
        if (visited.test(u))
            return;
        if (u == dest) {
            spdlog::trace("  {}-cycle found", len);
            if (len == PROOFSIZE && nsols < maxSols) {
                qsort(sols[nsols++], PROOFSIZE, sizeof(word_t), nonce_cmp);
                memcpy(sols[nsols], sols[nsols - 1], sizeof(sols[0]));
            }
            return;
        }
        if (len == PROOFSIZE)
            return;
        word_t au1 = adjlist[u];
        if (au1 != NIL) {
            visited.set(u);
            for (; au1 != NIL; au1 = links[au1].next) {
                sols[nsols][len] = au1 / 2;
                cycles_with_link(len + 1, links[au1 ^ 1].to, dest);
            }
            visited.reset(u);
        }
    }

    void add_edge(word_t u, word_t v) {
        assert(u < maxNodes);
        assert(v < maxNodes);
        v += maxNodes;                                // distinguish partitions
        if (adjlist[u] != NIL && adjlist[v] != NIL) { // possibly part of a cycle
            sols[nsols][0] = nlinks / 2;
            assert(!visited.test(u));
            cycles_with_link(1, u, v);
        }
        word_t ulink = nlinks++;
        word_t vlink = nlinks++; // the two halfedges of an edge differ only in last bit
        assert(vlink != NIL);    // avoid confusing links with NIL; guaranteed if bits in word_t > EDGEBITS + 1
        links[ulink].next            = adjlist[u];
        links[vlink].next            = adjlist[v];
        links[adjlist[u] = ulink].to = u;
        links[adjlist[v] = vlink].to = v;
    }

    void add_compress_edge(word_t u, word_t v) {
        add_edge(compressu->compress(u), compressv->compress(v));
    }
};
