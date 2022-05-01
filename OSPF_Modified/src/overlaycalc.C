#include "ospfinc.h"
#include "system.h"
#include "nbrfsm.h"

/* Full overlay calculation process.
 * Includes the Dijkstra calculation performed over the
 * ABR overlay and the prefix and ASBR routes over the
 * overlay, performing the corresponding translation into
 * the Summary-LSAs to be advertise inside the areas
 */
// TODO make sure only ABRs can enter here

void OSPF::overlay_calc()

{

}

/* Dijsktra calculation performed over the ABR overlay.
 * Determines the ABR overlay topology, and the lowest
 * path cost from our ABR to every other one.
 */

/*void OSPF::overlay_dijkstra()

{
    PriQ cand;
    overlayAbrLSA *abr;

    n_overlay_dijkstras++;
    
    // Initialize state of ABR nodes
    // Iterate through all the ABR-LSAs available
    abr = (overlayAbrLSA *) abrLSAs.sllhead;
    for (; abr; abr = (overlayAbrLSA *) abr->sll) {
        // Initialize the overlay Dijkstra calculation, 
        // by adding our ABR-LSA to the candidate list
        if (abr->adv_rtr() == my_id()) {
            abr->cost0 = 0;
            abr->cost1 = 0;
            abr->tie1 = abr->lsa_type;
            cand.priq_add(abr);
            abr->t_state = DS_ONCAND;
        }
        else
            abr->t_state = DS_UNINIT;
    }

    while (abr = (overlayAbrLSA *) cand.priq_rmhead()) {
        RTE *dest;
        overlayAbrLSA *abr_neigh;

        // Put onto SPF tree
        abr->t_state = DS_ONTREE;
        dest = abr->t_dest;
        dest->
    }
} */