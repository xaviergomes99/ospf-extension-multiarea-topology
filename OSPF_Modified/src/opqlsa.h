/*
 *   OSPFD routing daemon
 *   Copyright (C) 2000 by John T. Moy
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2
 *   of the License, or (at your option) any later version.
 *   
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *   
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* Internal support for Opaque-LSAs.
 */

/* We don't look at the body of Opaque-LSAs.
 * Howver, if we are requested to originate an
 * Opaque-LSA, we store the body separately so
 * that it can be refreshed from local storage.
 */

class overlayAbrLSA;
class overlayPrefixLSA;
class overlayAsbrLSA;

class opqLSA : public LSA {
    bool adv_opq;
    byte *local_body;
    int local_blen;
    // Stored for opaque-LSA upload, in case
    // LSA is deleted before it can be uploaded
    int phyint;
    InAddr if_addr;
    aid_t a_id;
public:
    overlayAbrLSA *abrLSA;  // Link to corresponding ABR-LSA
    overlayPrefixLSA *prefixLSA; // Link to corresponding Prefix-LSA
    overlayAsbrLSA *asbrLSA; // Link to corresponding ASBR-LSA
    
    opqLSA(class SpfIfc *, class SpfArea *, LShdr *, int blen);
    virtual void reoriginate(int forced);
    virtual void parse(LShdr *hdr);
    virtual void unparse();
    virtual void build(LShdr *hdr);
    virtual void update_in_place(LSA *);
    virtual void parse_overlay_lsa(LShdr *hdr);
    virtual void unparse_overlay_lsa();
    SpfNbr *grace_lsa_parse(byte *, int, int &);
    friend class OSPF;
    friend class INrte;
};

/* Holding queue for Opaque-LSAs that are to be delivered
 * to an application.
 */

class OpqHoldQ : public AVLitem {
    LsaList holdq;
 public:
    OpqHoldQ(int conn_id);
    friend class OSPF;
};

class overlayAbrLSA : public PriQElt, public AVLitem {
    opqLSA *lsa;    // Corresponding opaque-LSA
    uns32 cost;     // Cost to the ABR
    byte overlay_dijk_run:1;    // Overlay Dijkstra run
    byte t_state;   // Current state of this ABR, in the dijkstra calc
    // ABRhdr *nbrs;   // Neighbors described in the LSA
    int n_nbrs;     // Number of neighbors described
public:
    overlayAbrLSA(class opqLSA *);
    ~overlayAbrLSA();
    friend class OSPF;
    friend class opqLSA;
};

class overlayPrefixLSA : public AVLitem {
    opqLSA *lsa;        // Corresponding opaque-LSA
    INrte *rte;         // Associated routing table entry
    Prefixhdr prefix;  // Prefix information
    overlayPrefixLSA *link; // Link together
public:
    overlayPrefixLSA(class opqLSA *, Prefixhdr *p);
    ~overlayPrefixLSA();
    friend class OSPF;
    friend class opqLSA;
    friend class INrte;
};

class overlayAsbrLSA : public AVLitem {
    opqLSA *lsa;    // Corresponding opaque-LSA
    ASBRrte *rte;   // Associated routing table entry
    ASBRhdr asbr;  // Destination ASBR information
    overlayAsbrLSA *link;   // Link together
public:
    overlayAsbrLSA(class opqLSA *, ASBRhdr *asbr);
    ~overlayAsbrLSA();
    friend class OSPF;
    friend class opqLSA;
    friend class ASBRrte;
};
    
    