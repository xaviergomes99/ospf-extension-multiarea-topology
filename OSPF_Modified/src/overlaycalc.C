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
    calc_overlay = false;
    // Run the Dijkstra calculation for the ABR overlay
    overlay_dijkstra();
    // Scan the present Prefix-LSAs and ASBR-LSAs
    prefix_scan();
    printf("after calc\n");
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
        if (abr->lsa->adv_rtr() == my_id()) {
            abr->cost0 = 0;
            abr->cost1 = 0;
            cand.priq_add(abr);
            abr->t_state = DS_ONCAND;
        }
        else
            abr->t_state = DS_UNINIT;
    }

    // Go through the candidate list
    while ((abr = (overlayAbrLSA *) cand.priq_rmhead())) {
        ABRhdr *nbr;
        overlayAbrLSA *abr_nbr;
        uns32 new_cost;
        int i;

        // Put onto SPF tree
        abr->t_state = DS_ONTREE;
        if (!(abr->cost0 > abr->cost))
            abr->cost = abr->cost0;

        // Scan neighboring ABRs
        for (nbr = abr->nbrs, i = 0; i < abr->n_nbrs; nbr++, i++) {
            abr_nbr = (overlayAbrLSA *) abrLSAs.find(nbr->neigh_rid);
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
                }
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
    overlayPrefixLSA *pref;
    overlayAsbrLSA *asbr;
    overlayAbrLSA *abr;
    ASBRrte *rrte;
    uns32 cost, best_cost;
    
    // Go through all the Prefix-LSAs associated to each INrte
    while ((rte = iter.nextrte())) {
        if (rte->prefixes) {
            best_cost = LSInfinity;
            for (pref = rte->prefixes; pref; pref = (overlayPrefixLSA *) pref->link) {
                abr = (overlayAbrLSA *) abrLSAs.find(pref->lsa->adv_rtr());
                if (abr) {
                    cost = abr->cost + pref->prefix->metric;
                    if (cost < best_cost)
                        best_cost = cost;
                }
            }
            // Assign the best cost to the routing table entry and
            // generate the corresponding Summ-LSA, if the cost has changed
            if (rte->cost != best_cost || !rte->has_been_adv) {
                rte->cost = best_cost;
                rte->has_been_adv = true;
                sl_orig(rte);
            }
        }
    }

    // Then go through the all the ASBR-LSAs for each of the ASBRs known
    for (rrte = ASBRs; rrte; rrte = rrte->next()) {
        if (rrte->asbr_lsas) {
            best_cost = LSInfinity;
            for (asbr = rrte->asbr_lsas; asbr; asbr = (overlayAsbrLSA *) asbr->link) {
                abr = (overlayAbrLSA *) abrLSAs.find(asbr->lsa->adv_rtr());
                if (abr) {
                    cost = abr->cost + asbr->asbr->metric;
                    if (cost < best_cost)
                        best_cost = cost;
                } 
            }
            // Assign the best cost to the routing table entry and
            // generate the corresponding Summ-LSA, if the cost has changed
            if (asbr->rte->cost != best_cost || !asbr->rte->has_been_adv) {
                asbr->rte->cost = best_cost;
                asbr->rte->has_been_adv = true;
                asbr_orig(asbr->rte);
            }
        } 
    }
} 