#include "ospfinc.h"
#include "system.h"
#include "nbrfsm.h"

/* Full overlay calculation process.
 * Includes the Dijkstra calculation performed over the
 * ABR overlay and the prefix and ASBR routes over the
 * overlay, performing the corresponding translation into
 * the Summary-LSAs to be advertise inside the areas
 */

void OSPF::overlay_calc()

{
    if (first_abrLSA_sent) {
        calc_overlay = false;
        // Run the Dijkstra calculation for the ABR overlay
        overlay_dijkstra();
        // Set the next (ABR) hop to each of the ABRs on the overlay
        set_overlay_nh();
        // Scan the present Prefix-LSAs and ASBR-LSAs
        prefix_scan();
        fa_tbl->resolve();
    }
}

/* Dijsktra calculation performed over the ABR overlay.
 * Determines the ABR overlay topology, and the lowest
 * path cost from our ABR to every other one.
 */

void OSPF::overlay_dijkstra()

{
    PriQ cand;
    overlayAbrLSA *abr;

    n_overlay_dijkstras++;
    
    // Initialize state of ABR nodes and candidate list
    // Iterate through all the ABR-LSAs available
    abr = (overlayAbrLSA *) abrLSAs.sllhead;
    for (; abr; abr = (overlayAbrLSA *) abr->sll) {
        // Initialize the overlay Dijkstra calculation, 
        // by adding our ABR-LSA to the candidate list
        if (abr == my_abr_lsa) {
            abr->cost0 = 0;
            abr->cost1 = 0;
            cand.priq_add(abr);
            abr->t_state = DS_ONCAND;
        }
        else {
            abr->t_state = DS_UNINIT;
            abr->t_parent = 0;
        }
    }

    // Go through the candidate list
    while ((abr = (overlayAbrLSA *) cand.priq_rmhead())) {
        ABRhdr *nbr;
        overlayAbrLSA *abr_nbr = 0;
        uns32 new_cost;
        int i;

        // Put onto SPF tree
        abr->t_state = DS_ONTREE;
        abr->cost = abr->cost0;

        // Scan neighboring ABRs
        nbr = (ABRhdr *) abr->lsa->lsa_body;
        for (i = 0; i < abr->n_nbrs; nbr++, i++) {
            abr_nbr = (overlayAbrLSA *) abrLSAs.find(hton32(nbr->neigh_rid));
            if (abr_nbr) {
                if (abr_nbr->t_state == DS_ONTREE)
                    continue;
                new_cost = abr->cost0 + nbr->metric;
                if (abr_nbr->t_state == DS_ONCAND) {
                    if (new_cost > abr_nbr->cost0)
                        continue;
                    else if (new_cost < abr_nbr->cost0)
                        cand.priq_delete(abr_nbr);
                }

                if (abr_nbr->t_state != DS_ONCAND || new_cost < abr_nbr->cost0) {
                    abr_nbr->cost0 = new_cost;
                    abr_nbr->cost1 = 0;
                    abr_nbr->tie1 = 0;
                    cand.priq_add(abr_nbr);
                    abr_nbr->t_state = DS_ONCAND;
                    abr_nbr->t_parent = abr;
                }
            }
        }
    }
}

/* We go through all the ABRs known to us and determine the next ABR step
 * needed to reach each of them. This is done by tracing back the parent
 * nodes set during the overlay Dijkstra calculation up to the last parent
 * node before our own ABR.
 */

void OSPF::set_overlay_nh()

{
    overlayAbrLSA *abr, *parent;

    abr = (overlayAbrLSA *) abrLSAs.sllhead;
    for (; abr; abr = (overlayAbrLSA *) abr->sll) {
        if (abr->t_state == DS_ONTREE) {
            // Skip our own ABR
            if (abr->t_parent == 0)
                continue;
            // ABR neighbors are their own next ABR hop
            else if (abr->t_parent == my_abr_lsa)
                abr->next_abr_hop = abr;
            // More distant ABRs
            else {
                parent = abr->t_parent;
                while (parent->t_parent != my_abr_lsa)
                    parent = parent->t_parent;
                abr->next_abr_hop = parent;
            }
        }
    }
}

/* Go through all the currently stored prefix-LSAs and ASBR-LSAs,
 * determining the total cost to reach them and originating the corresponding
 * Summary-LSAs. As we can find the advertising router for each of the LSAs,
 * we simply need to sum the cost to the advertising ABR and the cost
 * advertised in the LSA.
 */

void OSPF::prefix_scan()

{
    INrte *rte;
    INiterator iter(inrttbl);
    ASBRrte *rrte;

    // Go through all the Prefix-LSAs associated to each INrte
    while ((rte = iter.nextrte())) {
        adv_best_prefix(rte);
    }

    // Then go through the all the ASBR-LSAs for each of the ASBRs known
    for (rrte = ASBRs; rrte; rrte = rrte->next()) {
         adv_best_asbr(rrte);
    }
}

/* Update our path to an inter-area destination, we get the new best cost
 * to a destination and the ABR advertising it, and determine the new next-hop
 * to reach that destination.
 */

void OSPF::update_path_overlay(RTE *rte, overlayAbrLSA *abr, uns32 cost)

{
    rtid_t next_abr;
    ABRNbr *nbr;
    RTE *rtr;

    // We are using our advertised cost (our best intra-area path)
    if (abr == my_abr_lsa) {
        rte->cost = cost;
        rte->r_type = RT_SPF;
        rte->update(rte->intra_path);
        return;
    }

    // Gather all the necessary information in order to update the entry
    next_abr = abr->next_abr_hop->index1();
    nbr = (ABRNbr *) added_nbrs.sllhead;
    for (; nbr; nbr = (ABRNbr *) nbr->sll) {
        if (nbr->get_rid() == next_abr)
            break;
    }
    rtr = nbr->rtr->t_dest;

    // Update the entry with the new cost and path
    rte->cost = cost;
    rte->r_type = RT_SPFIA;
    rte->update(rtr->r_mpath);
}

/* Determine the best Prefix-LSA for a given destination, update our own 
 * routing table to it and advertise the corresponding Summ-LSA.
 */

void OSPF::adv_best_prefix(INrte *rte)

{
    overlayPrefixLSA *pref, *in_use;
    overlayAbrLSA *abr, *best_abr;
    uns32 cost, best_cost;
    bool found;

    if (rte->prefixes) {
        found = false;
        best_cost = LSInfinity;
        for (pref = rte->prefixes; pref; pref = (overlayPrefixLSA *) pref->link) {
            if (pref->index1() == my_id()) {
                found = true;
                cost = pref->prefix.metric;
                if (cost < best_cost) {
                    best_cost = cost;
                    best_abr = my_abr_lsa;
                    in_use = pref;
                }
            }
            else if ((abr = (overlayAbrLSA *) abrLSAs.find(pref->index1()))) {
                found = true;
                cost = abr->cost + pref->prefix.metric;
                if (cost < best_cost) {
                    best_cost = cost;
                    best_abr = abr;
                    in_use = pref;
                }
            }
        }
        // Assign the best cost to the routing table entry and
        // generate the corresponding Summ-LSA, if the cost has changed
        if (found && (rte->changed || rte->cost != best_cost || !rte->has_been_adv)) {
            update_path_overlay(rte, best_abr, best_cost);
            rte->has_been_adv = true;
            rte->in_use = in_use;
            sl_orig(rte);
        }
    }
}

/* Determine the best ASBR-LSA for a given destination, update our own 
 * routing table to it and advertise the corresponding ASBR-Summ-LSA.
 */

void OSPF::adv_best_asbr(ASBRrte *rte)

{
    overlayAsbrLSA *asbr;
    overlayAbrLSA *abr, *best_abr;
    uns32 cost, best_cost;
    bool found;

    if (rte->asbr_lsas) {
        found = false;
        best_cost = LSInfinity;
        for (asbr = rte->asbr_lsas; asbr; asbr = (overlayAsbrLSA *) asbr->link) {
            if ((abr = (overlayAbrLSA *) abrLSAs.find(asbr->index1()))) {
                found = true;
                cost = abr->cost + asbr->asbr.metric;
                if (cost < best_cost) {
                    best_cost = cost;
                    best_abr = abr;
                }
            } 
        }
        // Assign the best cost to the routing table entry and
        // generate the corresponding Summ-LSA, if the cost has changed
        if (found && (rte->cost != best_cost || !rte->has_been_adv)) {
            update_path_overlay(rte, best_abr, best_cost);
            rte->has_been_adv = true;
            asbr_orig(rte);
        }
    }
}