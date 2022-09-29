/*
 * Class to define an ABR overlay neighbor. This is used exclusively
 * by the ABRs to store the necessary information to build and flood
 * their ABR-LSAs.
 */

class rtrLSA;

class ABRNbr : public AVLitem {
    rtid_t rid;     // ABR neighbor RID
    uns32 cost;     // Intra-area shortest path cost
    rtrLSA *rtr;    // Link to corresponding Router-LSA
    SpfArea *area;  // Area in which we are neighbors with the ABR
    bool use_in_lsa;    // This ABRNbr is to be considered when building the ABR-LSA
public:
    ABRNbr(rtrLSA *lsa, SpfArea *a);
    virtual ~ABRNbr();

    inline rtid_t get_rid();
    inline uns32 get_cost();
    inline rtrLSA *get_rtrLSA();
    inline SpfArea *get_area();
    // void remove_abr_nb();
    friend class OSPF;
    friend class LSA;
    friend class rtrLSA;
};

// Inline functions
inline rtid_t ABRNbr::get_rid()
{
    return(rid);
}
inline uns32 ABRNbr::get_cost()
{
    return(cost);
}
inline rtrLSA *ABRNbr::get_rtrLSA()
{
    return(rtr);
}
inline SpfArea *ABRNbr::get_area()
{
    return(area);
}