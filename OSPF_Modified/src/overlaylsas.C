#include "ospfinc.h"
#include "monitor.h"
#include "system.h"
#include "ifcfsm.h"
#include "nbrfsm.h"
#include "phyint.h"

/* We iterate all of the ABR neighbors to check whether the last state
 * of communication has been achieved with all the current neighbors.
 * This condition will indicate the ABR to send the first overlay LSAs to
 * the other ABRs.
 */

void OSPF::nb_max_state_achieved() {
    IfcIterator ifcIter(ospf);
    SpfIfc *ifc;
    SpfNbr *nbr;

    if (n_adj_pend || n_dbx_nbrs) // Not all DB descriptions are done
        return;
    //go through all the router interfaces
    while (ifc = ifcIter.get_next()) { 
        NbrIterator nbrIter(ifc);
        //iterate through neighbors on each interface
        while (nbr = nbrIter.get_next()) {
            // Max state can be either NBS_FULL or NBS_2WAY
            // Here we verify that every neighbor is in either of those
            // If any neighbor is still not in one of these, we return
            // and try again later
            if (nbr->n_state != NBS_2WAY && nbr->n_state != NBS_FULL)
                return;
        }
    }
    // If every router has reached one of the two final states
    // we send the first overlay-LSAs and set the flag indicating it
    send_first_overlay_lsas();
    ospf->first_overlay_lsas_sent = true;
}


/* Sends this ABR's first overlay-LSAs. This includes its ABR-LSA, all the
 * currently possible Prefix-LSAs and ASBR-LSAs. This method is called when
 * the ABR finishes its synchronization process with all of its directly
 * attached neighbors.
 */

void OSPF::send_first_overlay_lsas() {
    INrte *rte;
    INiterator iter(inrttbl);
    ASBRrte *rrte;
    // There is only one ABR-LSA to advertise, always
    send_abr_lsa();

    // We must advertise all the area-internal destinations and their costs,
    // for the routes with their costs already calculated
    while ((rte = iter.nextrte())) {
        if (rte->intra_area() && !rte->sent_overlay)
            send_prefix_lsa(rte);
    }

    // Iterate through the ASBR routing table entries, and advertise them
    // if they haven't been advertised yet. We will only consider intra-area paths
    for (rrte = ospf->ASBRs; rrte; rrte = rrte->next()) {
        if (rrte-> && rrte->intra_area() && !rrte->sent_overlay)
            send_asbr_lsa(rrte);
    }
    
}

/* Generate and send the ABR-LSA with AS-scope
 */

void OSPF::send_abr_lsa() {
    //TODO store my neighboring ABRs and only then iterate through them
    AreaIterator areaIter(ospf);
    SpfArea *area;
    ABRhdr *body_start, *body;
    int blen;
    lsid_t ls_id;
    RTRrte *abr;
    int n_nbrs = 0, i;
    uns32 rid;
    byte cost;

    // Build the LSA body
    ls_id = OPQ_T_MULTI_ABR << 24;
    body = (ABRhdr *) body_start;

    // For each area, add the respective neighboring ABR information
    while ((area = areaIter.get_next())) { 
        // iterate through this area's ABRs
        abr = (RTRrte *) area->abr_tbl.sllhead;
	    for (; abr; abr = (RTRrte *) abr->sll) {
            rid = abr->rtrid();
            if (rid == my_id()) { 
                continue;
            } else { //neighboring abr
                cost = abr->cost;
                if (n_nbrs > 0) {
                    //consider only the best entry for each neighbor
                    for (i = 0; i < n_nbrs; i++) {
                        //better cost for an already considered neighbor
                        if ((body_start + i)->neigh_rid == rid 
                        && (body_start + i)->metric > cost) {
                            (body_start + i)->metric = cost;
                            break;
                        } else if ((body_start + i)->neigh_rid == rid)
                            break;
                        //new neighbor
                        body->neigh_rid = rid;
                        body->metric = cost;
                        n_nbrs++;
                        body++;
                    }
                } else { //first entry
                    body->neigh_rid = rid;
                    body->metric = cost;
                    n_nbrs++;
                    body++;
                }
            }
        }
    }
    blen = ((byte *) body) - ((byte *) body_start);
    adv_as_opq(ls_id, body, blen, true, 0);
}

/* Generate and send the Prefix-LSA with AS-scope
 */

void OSPF::send_prefix_lsa(INrte *rte) {
    //TODO flag on the routes indicating it should be advertised in a prefix-LSA??? might be easier to go through them, rather than checking for every one
    Prefixhdr *body;
    lsid_t ls_id;

    // Build the LSA body
    ls_id = OPQ_T_MULTI_PREFIX << 24;
    
    body->metric = rte->cost;
    body->subnet_mask = rte->mask();
    body->subnet_addr = rte->net();
    rte->advertise_overlay();

    adv_as_opq(ls_id, body, sizeof(Prefixhdr), true, 0);
}

/* Generate and send the ASBR-LSA with AS-scope
 */

void OSPF::send_asbr_lsa(ASBRrte *rte) { 
    //TODO flag on the routes indicating it should be advertised in an asbr-LSA???
    ASBRhdr *body;
    lsid_t ls_id;

    // Build the LSA body
    ls_id = OPQ_T_MULTI_ASBR << 24;
    
    body->metric = rte->cost;
    body->dest_rid = rte->rtrid();
    rte->advertise_overlay();

    adv_as_opq(ls_id, body, sizeof(ASBRhdr), true, 0);
}

/* Add an ABR to OSPF. If already added, return it. Otherwise
 * allocate entry and add it to the AVL and singly linked list.
 * Similar to the method with the same name used inside the areas,
 * but to be used by the overlay ABRs.
 */

RTRrte *OSPF::add_abr(uns32 rtrid)

{
    RTRrte *rte;

    if (rte = (RTRrte *) ABRtree.find(rtrid))
        return(rte);
    
    rte = new RTRrte(rtrid);
    ABRtree.add(rte);
    return(rte);
}

/* Parse a received ABR-LSA
 */

void overlayAbrLSA::parse(LShdr *hdr)

{
    ABRhdr *nbrs;
    byte *end;
    int n_nbrs;
    int blen;
    int i;

    nbrs = (ABRhdr *) (hdr+1);
    end = ((byte *) hdr) + ntoh16(hdr->ls_length);
    blen = ntoh16(hdr->ls_length) - sizeof(LShdr);
    n_nbrs = blen / sizeof(ABRhdr);

    t_dest = ospf->add_abr(adv_rtr());

    for (i = 0; i < n_nbrs; i++) {
        if (((byte *) nbrs) > end) {
			exception = true;
			break;
		}
        
    }
}

/* Parse a received Prefix-LSA
 */

void overlayPrefixLSA::parse(LShdr *hdr)

{
    
}

/* Parse a received ASBR-LSA
 */

void overlayAsbrLSA::parse(LShdr *hdr)

{
    
}

/* Unparse a received ABR-LSA
 */

void overlayAbrLSA::unparse()

{

}

/* Unparse a received Prefix-LSA
 */

void overlayPrefixLSA::unparse()

{
    
}

/* Unparse a received ASBR-LSA
 */

void overlayAsbrLSA::unparse()

{
    
}